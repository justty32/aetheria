#pragma once

// dungeon_detail.h：地城生成與互動跨實作檔共用的內部入口。

#include "core/local/dungeon.h"

namespace aetheria::local::detail::dungeon {

[[nodiscard]] bool contains_uid(const std::vector<std::uint64_t>& values,
                                std::uint64_t uid) noexcept;
[[nodiscard]] std::vector<DungeonRoom> generate_rooms(
    std::uint64_t seed, std::uint8_t depth, const rules::DungeonArchetypeRules& profile);

}  // namespace aetheria::local::detail::dungeon
