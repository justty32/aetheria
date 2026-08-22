// ruleset_load_dungeon.cpp：載入地城曲線、三種共用生成參數與 TrapDef。

#include "core/rules/ruleset.h"

#include <array>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <toml++/toml.hpp>

#include "core/rules/toml_read.h"

namespace aetheria::rules {
namespace {

[[nodiscard]] toml::table parse_document(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error{"Ruleset 檔案不存在：" + path.string()};
    }
    try {
        return toml::parse_file(path.string());
    } catch (const toml::parse_error& error) {
        throw std::runtime_error{"Ruleset TOML 格式錯誤：" + path.string() + "：" +
                                 std::string{error.description()}};
    }
}

template <typename Value>
[[nodiscard]] Value require_unsigned(const toml::table& table, std::string_view field,
                                     const std::filesystem::path& path, Value minimum,
                                     Value maximum) {
    const auto raw = detail::require_integer(table, field, path);
    if (raw < static_cast<std::int64_t>(minimum) ||
        raw > static_cast<std::int64_t>(maximum)) {
        throw std::runtime_error{"dungeon.toml 欄位超出值域：" + std::string{field}};
    }
    return static_cast<Value>(raw);
}

[[nodiscard]] TrapKind trap_kind(std::string_view value) {
    if (value == "damage") return TrapKind::Damage;
    if (value == "obstacle") return TrapKind::Obstacle;
    if (value == "summon") return TrapKind::Summon;
    if (value == "curse") return TrapKind::Curse;
    throw std::runtime_error{"dungeon.toml 含無效機關種類：" + std::string{value}};
}

[[nodiscard]] TrapCheckAttribute trap_attribute(std::string_view value) {
    if (value == "skill") return TrapCheckAttribute::Skill;
    if (value == "mind") return TrapCheckAttribute::Mind;
    if (value == "spirit") return TrapCheckAttribute::Spirit;
    throw std::runtime_error{"dungeon.toml 含無效機關屬性：" + std::string{value}};
}

[[nodiscard]] TrapDisarmMethod disarm_method(std::string_view value) {
    if (value == "attribute") return TrapDisarmMethod::Attribute;
    if (value == "cannot_disarm") return TrapDisarmMethod::CannotDisarm;
    if (value == "faith_or_magic") return TrapDisarmMethod::FaithOrMagic;
    throw std::runtime_error{"dungeon.toml 含無效解除方式：" + std::string{value}};
}

[[nodiscard]] DungeonArchetype archetype(std::string_view value) {
    if (value == "natural") return DungeonArchetype::Natural;
    if (value == "artificial") return DungeonArchetype::Artificial;
    if (value == "hybrid") return DungeonArchetype::Hybrid;
    throw std::runtime_error{"dungeon.toml 含無效地城種類：" + std::string{value}};
}

}  // namespace

void RulesetLoader::load_dungeon_rules(Ruleset& result,
                                        const std::filesystem::path& data_directory,
                                        std::set<std::string, std::less<>>& global_ids) {
    const auto path = data_directory / "dungeon.toml";
    const auto document = parse_document(path);
    const auto* curve = document["curve"].as_table();
    const auto* archetypes = document["archetypes"].as_array();
    const auto* traps = document["traps"].as_array();
    if (curve == nullptr || archetypes == nullptr || traps == nullptr ||
        archetypes->size() != kDungeonArchetypeCount || traps->empty()) {
        throw std::runtime_error{"dungeon.toml 缺少曲線、三種 archetype 或 TrapDef"};
    }

    auto& rules = result.dungeon_rules_;
    rules.difficulty_base = detail::require_int32(*curve, "difficulty_base", path);
    rules.difficulty_depth_step = detail::require_int32(*curve, "difficulty_depth_step", path);
    rules.clue_noise = detail::require_int32(*curve, "clue_noise", path);
    rules.enemy_base = require_unsigned<std::uint16_t>(*curve, "enemy_base", path, 1, 1000);
    rules.enemy_depth_step =
        require_unsigned<std::uint16_t>(*curve, "enemy_depth_step", path, 1, 1000);
    rules.treasure_base = require_unsigned<std::uint16_t>(*curve, "treasure_base", path, 1, 10000);
    rules.treasure_depth_step =
        require_unsigned<std::uint16_t>(*curve, "treasure_depth_step", path, 1, 10000);
    rules.light_base_cost =
        require_unsigned<std::uint16_t>(*curve, "light_base_cost", path, 1, 1000);
    rules.light_depth_step =
        require_unsigned<std::uint16_t>(*curve, "light_depth_step", path, 1, 1000);
    rules.lit_vision = require_unsigned<std::uint8_t>(*curve, "lit_vision", path, 1, 64);
    rules.unlit_vision = require_unsigned<std::uint8_t>(*curve, "unlit_vision", path, 1, 64);
    rules.unlit_hit_modifier = detail::require_int32(*curve, "unlit_hit_modifier", path);
    rules.unlit_detection_modifier =
        detail::require_int32(*curve, "unlit_detection_modifier", path);
    rules.cleared_density_numerator =
        require_unsigned<std::uint16_t>(*curve, "cleared_density_numerator", path, 0, 1000);
    rules.cleared_density_denominator =
        require_unsigned<std::uint16_t>(*curve, "cleared_density_denominator", path, 1, 1000);
    if (rules.difficulty_depth_step <= 0 || rules.clue_noise < 0 ||
        rules.unlit_vision >= rules.lit_vision || rules.unlit_hit_modifier >= 0 ||
        rules.unlit_detection_modifier >= 0 ||
        rules.cleared_density_numerator >= rules.cleared_density_denominator) {
        throw std::runtime_error{"dungeon.toml 深度曲線或 cleared 密度比例無效"};
    }

    std::array<bool, kDungeonArchetypeCount> seen{};
    for (const auto& node : *archetypes) {
        const auto& table = detail::require_table(node, path);
        const auto kind = archetype(detail::require_string(table, "id", path));
        const auto index = archetype_index(kind);
        if (seen[index]) {
            throw std::runtime_error{"dungeon.toml archetype 重複"};
        }
        seen[index] = true;
        auto& row = rules.archetypes[index];
        row.room_count = require_unsigned<std::uint8_t>(table, "room_count", path, 2, 32);
        row.natural_percent =
            require_unsigned<std::uint8_t>(table, "natural_percent", path, 0, 100);
        row.symmetry_percent =
            require_unsigned<std::uint8_t>(table, "symmetry_percent", path, 0, 100);
        row.erosion_percent =
            require_unsigned<std::uint8_t>(table, "erosion_percent", path, 0, 100);
        row.trap_weight = require_unsigned<std::uint8_t>(table, "trap_weight", path, 0, 100);
        row.guardian_weight =
            require_unsigned<std::uint8_t>(table, "guardian_weight", path, 0, 100);
    }

    for (const auto& node : *traps) {
        const auto& table = detail::require_table(node, path);
        TrapDef def{
            .id = detail::require_string(table, "id", path),
            .name_key = detail::require_string(table, "name_key", path),
            .kind = trap_kind(detail::require_string(table, "kind", path)),
            .detection_attribute =
                trap_attribute(detail::require_string(table, "detection_attribute", path)),
            .disarm_method =
                disarm_method(detail::require_string(table, "disarm_method", path)),
            .disarm_attribute =
                trap_attribute(detail::require_string(table, "disarm_attribute", path)),
            .detection_difficulty = detail::require_int32(table, "detection_difficulty", path),
            .disarm_difficulty = detail::require_int32(table, "disarm_difficulty", path),
            .base_damage = detail::require_int32(table, "base_damage", path),
            .damage_per_depth = detail::require_int32(table, "damage_per_depth", path),
        };
        if (def.detection_difficulty < 0 || def.disarm_difficulty < 0 || def.base_damage < 0 ||
            def.damage_per_depth < 0) {
            throw std::runtime_error{"dungeon.toml TrapDef 數值不可為負：" + def.id};
        }
        detail::register_global_id(global_ids, def.id, "trap.");
        const auto id = detail::append_def<TrapDefId>(result.traps_, std::move(def));
        result.trap_index_.emplace(result.traps_.back().id, id);
    }
    rules.loaded = true;
}

}  // namespace aetheria::rules
