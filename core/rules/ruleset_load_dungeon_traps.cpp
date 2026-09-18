// ruleset_load_dungeon_traps.cpp：地城機關定義載入。

#include "core/rules/ruleset.h"
#include "core/rules/ruleset_load_dungeon_detail.h"
#include "core/rules/toml_read.h"

#include <stdexcept>
#include <string_view>
#include <toml++/toml.hpp>

namespace aetheria::rules {
namespace {

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

}  // namespace

void RulesetLoader::load_dungeon_traps(Ruleset& result,
                                        const detail::DungeonTrapLoadContext& context) {
    const auto* traps = context.traps;
    const auto& path = context.path;
    auto& global_ids = context.global_ids;
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
}

}  // namespace aetheria::rules
