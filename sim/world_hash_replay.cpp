#include "sim/world_hash.h"

#include "core/history/history_log.h"
#include "core/runtime/save_raws.h"
#include "core/runtime/playable_session.h"
#include "core/runtime/session_persistence.h"
#include "core/rules/ruleset.h"
#include "core/serialize/normalized_state_hash.h"
#include "core/zone/file_zone_store.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>


namespace aetheria::sim {

int run_replay(const std::filesystem::path& slot_directory) {
    const auto ruleset = rules::RulesetLoader::load(
        runtime::save_raws_directory(slot_directory));
    zone::FileZoneStore source{slot_directory, ruleset};
    const auto metadata = runtime::inspect_session_save(source);
    const history::HistoryLog source_history{slot_directory / "history.log"};

    zone::InMemoryZoneStore replay_store{ruleset};
    runtime::PlayableSession replay{metadata.world_seed, metadata.region_id,
                                    runtime::save_raws_directory(slot_directory).string(),
                                    replay_store};
    replay.replay_history_from(slot_directory);

    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto temporary = std::filesystem::temp_directory_path() /
                           ("aetheria-replay-" + unique);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } cleanup{temporary};
    zone::FileZoneStore replay_destination{temporary, ruleset};
    replay.save_game(replay_destination, "replay");

    const auto stored = world_state_hash(slot_directory);
    const auto replayed = world_state_hash(temporary);
    const auto replay_hash = compose_world_identity(
        replayed.zone_hash, replayed.raws_hash, source_history.head_hash());
    std::cout << "stored_zone_hash=" << stored.zone_hash
              << " replay_zone_hash=" << replayed.zone_hash << '\n'
              << "stored_raws_hash=" << stored.raws_hash
              << " replay_raws_hash=" << replayed.raws_hash << '\n'
              << "stored_head_hash=" << stored.history_head_hash
              << " replay_head_hash=" << source_history.head_hash() << '\n'
              << "stored_world_hash=" << stored.hash
              << " replay_world_hash=" << replay_hash
              << " history_seq=" << source_history.head_seq() << '\n';
    if (stored.zone_hash != replayed.zone_hash) {
        throw std::runtime_error{"sim replay zone 分量不一致"};
    }
    if (stored.raws_hash != replayed.raws_hash) {
        throw std::runtime_error{"sim replay raws 分量不一致"};
    }
    if (stored.history_head_hash != source_history.head_hash()) {
        throw std::runtime_error{"sim replay history head 分量不一致"};
    }
    if (stored.hash != replay_hash) {
        throw std::runtime_error{"sim replay 世界身分合成值不一致"};
    }
    return 0;
}

} // namespace aetheria::sim
