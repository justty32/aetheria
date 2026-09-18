#pragma once

#include "core/runtime/playable_session.h"
#include "core/runtime/save_raws.h"
#include "core/rules/ruleset.h"
#include "core/zone/file_zone_store.h"
#include "sim/world_hash.h"
#include "tests/zone/zone_test_support.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace aetheria::tests::save_raws {

using aetheria::runtime::PlayableSession;
using aetheria::tests::TemporaryDirectory;

[[nodiscard]] inline std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if (!stream) {
        throw std::runtime_error{"測試無法開啟檔案：" + path.string()};
    }
    const auto end = stream.tellg();
    if (end < 0) {
        throw std::runtime_error{"測試無法取得檔案大小：" + path.string()};
    }
    std::string bytes(static_cast<std::size_t>(end), '\0');
    stream.seekg(0);
    if (!bytes.empty()) {
        stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream) {
        throw std::runtime_error{"測試無法完整讀檔：" + path.string()};
    }
    return bytes;
}

inline void write_file(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error{"測試無法寫檔：" + path.string()};
    }
}

inline void replace_once(std::string& text, std::string_view from, std::string_view to) {
    const auto position = text.find(from);
    if (position == std::string::npos || text.find(from, position + from.size()) != std::string::npos) {
        throw std::runtime_error{"測試替換目標不存在或不唯一：" + std::string{from}};
    }
    text.replace(position, from.size(), to);
}

struct LoadedWorld {
    explicit LoadedWorld(const std::filesystem::path& slot)
        : ruleset{std::make_unique<aetheria::rules::Ruleset>(
              aetheria::rules::RulesetLoader::load(
                  aetheria::runtime::save_raws_directory(slot)))},
          store{std::make_unique<aetheria::zone::FileZoneStore>(slot, *ruleset)},
          session{PlayableSession::load(slot, *store, "default")} {}

    std::unique_ptr<aetheria::rules::Ruleset> ruleset;
    std::unique_ptr<aetheria::zone::FileZoneStore> store;
    std::unique_ptr<PlayableSession> session;
};

struct ObservableWorld {
    std::uint64_t world_hash{};
    std::vector<aetheria::rules::TerrainId> terrain_tiles;
    std::vector<std::tuple<std::uint64_t, std::uint16_t, std::int16_t,
                           std::int16_t, std::int32_t, bool>> armies;
    std::size_t terrain_count{};
    std::int32_t swamp_move_cost{};
    std::uint16_t faction_count{};
    std::uint16_t diplomacy_faction_count{};

    bool operator==(const ObservableWorld&) const = default;
};

[[nodiscard]] inline ObservableWorld observe(const std::filesystem::path& slot) {
    LoadedWorld loaded{slot};
    ObservableWorld result;
    result.world_hash = aetheria::sim::world_state_hash(slot).hash;
    result.terrain_tiles = loaded.session->tiles().base;
    for (const auto& army : loaded.session->armies()) {
        result.armies.emplace_back(army.id.uid, static_cast<std::uint16_t>(army.faction),
                                   army.tile.x, army.tile.y, army.power,
                                   army.player_controlled);
    }
    const auto& ruleset = loaded.session->ruleset();
    result.terrain_count = ruleset.terrains().size();
    const auto swamp = ruleset.find_terrain("terrain.swamp");
    if (!swamp.has_value()) {
        throw std::runtime_error{"測試 Ruleset 缺少 terrain.swamp"};
    }
    result.swamp_move_cost = ruleset.terrain(*swamp)->move_cost;
    result.faction_count = ruleset.civilization_rules().factions.faction_count;
    result.diplomacy_faction_count = loaded.session->diplomacy().faction_count();
    return result;
}

[[nodiscard]] inline std::size_t toml_count(const std::filesystem::path& directory) {
    std::size_t result{};
    for (const auto& entry : std::filesystem::directory_iterator{directory}) {
        if (entry.is_regular_file() && entry.path().extension() == ".toml") {
            ++result;
        }
    }
    return result;
}

}  // namespace aetheria::tests::save_raws
