#pragma once

// dungeon_rules.h：資料驅動的地城深度曲線、三種生成參數與 TrapDef。
// 三種地城只選參數列；房間、內容與深度曲線共用同一份生成實作。

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace aetheria::rules {

enum class TrapDefId : std::uint16_t {};

[[nodiscard]] constexpr std::uint16_t value_of(TrapDefId id) noexcept {
    return static_cast<std::uint16_t>(id);
}

enum class DungeonArchetype : std::uint8_t {
    Natural,
    Artificial,
    Hybrid,
};

inline constexpr std::size_t kDungeonArchetypeCount = 3;

enum class TrapKind : std::uint8_t {
    Damage,
    Obstacle,
    Summon,
    Curse,
};

enum class TrapCheckAttribute : std::uint8_t {
    Skill,
    Mind,
    Spirit,
};

enum class TrapDisarmMethod : std::uint8_t {
    Attribute,
    CannotDisarm,
    FaithOrMagic,
};

struct TrapDef {
    std::string id;
    std::string name_key;
    TrapKind kind{TrapKind::Damage};
    TrapCheckAttribute detection_attribute{TrapCheckAttribute::Skill};
    TrapDisarmMethod disarm_method{TrapDisarmMethod::Attribute};
    TrapCheckAttribute disarm_attribute{TrapCheckAttribute::Skill};
    std::int32_t detection_difficulty{};
    std::int32_t disarm_difficulty{};
    std::int32_t base_damage{};
    std::int32_t damage_per_depth{};
};

struct DungeonArchetypeRules {
    std::uint8_t room_count{};
    std::uint8_t natural_percent{};
    std::uint8_t symmetry_percent{};
    std::uint8_t erosion_percent{};
    std::uint8_t trap_weight{};
    std::uint8_t guardian_weight{};
};

struct DungeonRules {
    std::int32_t difficulty_base{};
    std::int32_t difficulty_depth_step{};
    std::int32_t clue_noise{};
    std::uint16_t enemy_base{};
    std::uint16_t enemy_depth_step{};
    std::uint16_t treasure_base{};
    std::uint16_t treasure_depth_step{};
    std::uint16_t light_base_cost{};
    std::uint16_t light_depth_step{};
    std::uint8_t lit_vision{};
    std::uint8_t unlit_vision{};
    std::int32_t unlit_hit_modifier{};
    std::int32_t unlit_detection_modifier{};
    std::uint16_t cleared_density_numerator{};
    std::uint16_t cleared_density_denominator{};
    std::array<DungeonArchetypeRules, kDungeonArchetypeCount> archetypes{};
    bool loaded{};
};

[[nodiscard]] constexpr std::size_t archetype_index(DungeonArchetype archetype) noexcept {
    return static_cast<std::size_t>(archetype);
}

}  // namespace aetheria::rules
