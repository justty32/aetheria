#pragma once

// 具名命運流程與個體抽樣共用的輸入驗證。

#include "core/world/named_fate.h"

namespace aetheria::world::detail {

[[nodiscard]] bool valid_member(const NamedFateMember& member) noexcept;
[[nodiscard]] bool valid_stage_one(const FateStageOne& stage) noexcept;

}  // namespace aetheria::world::detail
