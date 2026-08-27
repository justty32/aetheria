#pragma once

#include "core/world/region_tiles.h"

#include <cstdint>

namespace aetheria::world {

// ArmyState 是 Region registry 上的部隊權威狀態。
// 所屬 entity 與 registry 擁有 component；執行期 session 只保存 StableId／entity 把手。
struct ArmyState {
    FactionId faction{};
    std::int32_t power{};
    bool player_controlled{};

    template <typename Archive> void serialize(Archive& archive) {
        archive(faction, power, player_controlled);
    }

    constexpr bool operator==(const ArmyState&) const noexcept = default;
};

}  // namespace aetheria::world
