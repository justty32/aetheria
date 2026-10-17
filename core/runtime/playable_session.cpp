// playable_session.cpp：建立與載入。

#include "core/runtime/playable_session.h"
#include "core/runtime/character_save.h"
#include "core/runtime/save_raws.h"
#include "core/runtime/session_persistence.h"
#include "core/runtime/turn_commit.h"



#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <toml++/toml.hpp>

namespace aetheria::runtime {

namespace {

[[nodiscard]] std::unique_ptr<zone::InMemoryZoneStore>
copy_slot_store(const zone::ZoneStore& slot, const rules::Ruleset& ruleset) {
    auto copy = std::make_unique<zone::InMemoryZoneStore>(ruleset);
    for (const auto key : slot.stored_keys()) {
        const auto zone = slot.load(key);
        if (zone == nullptr) {
            throw std::runtime_error{"冷讀時列舉到的 zone 無法載入：" +
                                     std::to_string(zone::value_of(key))};
        }
        copy->save(*zone);
    }
    if (slot.manifest().has_value()) {
        copy->write_manifest(*slot.manifest());
    }
    return copy;
}

struct PlayableUidConfig {
    std::optional<std::uint64_t> player_army;
    std::optional<std::uint64_t> enemy_army;
    std::optional<std::uint64_t> named_commander;
};

[[nodiscard]] PlayableUidConfig
load_playable_uids(const std::filesystem::path& raws_directory) {
    const auto document = toml::parse_file((raws_directory / "civilization.toml").string());
    const auto* table = document["playable_session_uids"].as_table();
    if (table == nullptr) {
        return {};
    }
    const auto player = (*table)["player_army"].value<std::uint64_t>();
    const auto enemy = (*table)["enemy_army"].value<std::uint64_t>();
    const auto named = (*table)["named_commander"].value<std::uint64_t>();
    if (!player.has_value() || !enemy.has_value() || !named.has_value()) {
        throw std::runtime_error{"playable_session_uids 必須完整宣告三個 uid"};
    }
    return {player, enemy, named};
}

[[nodiscard]] rules::Ruleset
load_saved_ruleset(const std::filesystem::path& slot_directory) {
    static_cast<void>(save_raws_hash(slot_directory));
    return rules::RulesetLoader::load(save_raws_directory(slot_directory));
}

} // namespace

PlayableSession::PlayableSession(std::uint64_t seed, std::uint32_t region_id,
                                 std::string data_directory, zone::ZoneStore& store)
    : seed_{seed}, region_id_{region_id},
      base_raws_directory_{std::move(data_directory)},
      ruleset_{rules::RulesetLoader::load(base_raws_directory_)}, store_{store},
      turn_pipeline_{ruleset_, store_} {
    if (store_.manifest().has_value() || !store_.stored_keys().empty()) {
        throw std::logic_error{"new_game 需要空的 ZoneStore"};
    }
    const auto uid_config = load_playable_uids(base_raws_directory_);
    if (!uid_config.player_army.has_value() || !uid_config.enemy_army.has_value() ||
        !uid_config.named_commander.has_value()) {
        throw std::runtime_error{"新世界基底 raws 缺少 playable_session_uids"};
    }
    player_army_id_ = world::StableId{
        store_.allocate_entity_uid(uid_config.player_army)};
    enemy_army_id_ = world::StableId{
        store_.allocate_entity_uid(uid_config.enemy_army)};
    named_commander_uid_ =
        store_.allocate_entity_uid(uid_config.named_commander);
    turn_commit_ = std::make_unique<TurnCommit>();
    manager_ = std::make_unique<zone::ZoneManager>(
        store_, [this](zone::ZoneKey key, std::unique_ptr<zone::Zone> persistent) {
            return materialize_session_zone(key, std::move(persistent));
        });
    initialize_scenario();
    initialize_diplomacy();
}

PlayableSession::PlayableSession(LoadTag, std::filesystem::path slot_directory,
                                 zone::ZoneStore& store)
    : base_raws_directory_{save_raws_directory(slot_directory)},
      ruleset_{load_saved_ruleset(slot_directory)},
      loaded_store_{copy_slot_store(store, ruleset_)}, store_{*loaded_store_},
      commit_destination_{&store}, turn_pipeline_{ruleset_, store_} {
    if (!store_.manifest().has_value() || store_.manifest()->raws_hash == 0) {
        throw std::runtime_error{"存檔 manifest 缺少 v23 raws 內容雜湊"};
    }
    require_save_raws_hash(slot_directory, store_.manifest()->raws_hash);
    const auto named_uid = load_playable_uids(base_raws_directory_).named_commander;
    if (!named_uid.has_value()) {
        throw std::runtime_error{"存檔基底 raws 缺少 named_commander uid"};
    }
    named_commander_uid_ = *named_uid;
    turn_commit_ = std::make_unique<TurnCommit>(slot_directory);
    const auto metadata = inspect_session_save(store_);
    seed_ = metadata.world_seed;
    region_id_ = metadata.region_id;
    region_key_ = metadata.region_key;
    manager_ = std::make_unique<zone::ZoneManager>(
        store_, [this](zone::ZoneKey key, std::unique_ptr<zone::Zone> persistent) {
            return materialize_session_zone(key, std::move(persistent));
        });
    initialize_loaded_scenario();
    if (now() != metadata.now) {
        throw std::runtime_error{"manifest now 與 Region TurnClock 不一致"};
    }
}

PlayableSession::~PlayableSession() = default;

std::unique_ptr<PlayableSession>
PlayableSession::load(std::filesystem::path slot_directory, zone::ZoneStore& store,
                      std::string_view character_name) {
    if (!store.manifest().has_value()) {
        throw std::runtime_error{"載入角色前世界存檔缺少 manifest"};
    }
    const auto manifest = *store.manifest();
    const auto character_path =
        character_save_path(slot_directory, character_name);
    TurnCommit recovery{slot_directory};
    if (recovery.recovery_needed()) {
        const auto metadata = inspect_session_save(store);
        const auto recovery_ruleset = load_saved_ruleset(slot_directory);
        zone::InMemoryZoneStore rebuilt_store{recovery_ruleset};
        PlayableSession rebuilt{
            metadata.world_seed, metadata.region_id,
            save_raws_directory(slot_directory).string(), rebuilt_store};
        recovery.replay_all(rebuilt);
        recovery.commit_world(
            rebuilt_store, store, *rebuilt.manager_, metadata.world_seed,
            manifest.raws_hash, rebuilt.now(), [&] {
                write_character_save(
                    character_path,
                    {metadata.world_seed, manifest.raws_hash},
                    rebuilt.export_character_state());
            });
    }

    auto result = std::unique_ptr<PlayableSession>{
        new PlayableSession{LoadTag{}, slot_directory, store}};
    result->character_save_path_ = character_path;
    result->import_character_state(read_character_save(
        character_path,
        {manifest.world_seed, manifest.raws_hash}));
    return result;
}

} // namespace aetheria::runtime
