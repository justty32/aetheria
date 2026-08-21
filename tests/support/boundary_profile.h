#pragma once

#include "core/spatial/boundary_profile.h"

#include <cstdint>

namespace aetheria::tests {

[[nodiscard]] inline std::uint64_t hash_boundary_profile(
    const spatial::BoundaryProfile& profile) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    const auto add_byte = [&](std::uint8_t value) {
        hash ^= value;
        hash *= UINT64_C(1099511628211);
    };
    const auto add_u16 = [&](std::uint16_t value) {
        add_byte(static_cast<std::uint8_t>(value));
        add_byte(static_cast<std::uint8_t>(value >> 8U));
    };
    for (const auto value : profile.elevation) {
        add_u16(value);
    }
    for (const auto value : profile.ground) {
        add_u16(rules::value_of(value));
    }
    for (const auto value : profile.water_depth) {
        add_byte(value);
    }
    for (const auto value : profile.edges) {
        add_u16(rules::value_of(value));
    }
    for (const auto& crossing : profile.crossings) {
        add_byte(crossing.pos);
        add_byte(crossing.width);
        add_u16(rules::value_of(crossing.kind));
    }
    return hash;
}

}  // namespace aetheria::tests
