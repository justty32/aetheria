#pragma once

// dungeon_state.h：Local zone 唯一的地城持久層；不保存可由 seed 重算的內容。

#include <cstdint>
#include <vector>

namespace aetheria::local {

struct DungeonPersistentState {
    std::vector<std::uint64_t> triggered_trap_uids;
    std::vector<std::uint64_t> claimed_treasure_uids;

    bool operator==(const DungeonPersistentState&) const = default;
};

[[nodiscard]] bool valid_dungeon_persistent_state(
    const DungeonPersistentState& state) noexcept;

}  // namespace aetheria::local
