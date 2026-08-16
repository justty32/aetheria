#pragma once

#include "core/rules/ruleset.h"
#include "core/zone/zone.h"

#include <cstdint>

namespace aetheria::serialize {

// normalized_state_hash 忽略 EnTT entity id 與執行期 LOD/cache/pinned，
// 依 StableId 排序後雜湊 Region 權威狀態，供不同建構歷史間比較。
[[nodiscard]] std::uint64_t normalized_state_hash(const zone::Zone& zone,
                                                  const rules::Ruleset& ruleset);

}  // namespace aetheria::serialize
