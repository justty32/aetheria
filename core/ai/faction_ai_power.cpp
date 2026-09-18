// 勢力 AI 行動對軍事與經濟實力的成長分配。

#include <aetheria/ai/faction_ai.h>

#include "faction_ai_detail.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace aetheria::ai {
using detail::saturating_score;

PowerChange faction_power_change(FactionActionKind action, FactionAiLod lod,
                                 std::int32_t military_power,
                                 std::int32_t economic_power,
                                 std::uint64_t world_seed, std::int64_t xun,
                                 FactionKey faction) noexcept {
    const auto total = std::max<std::int64_t>(
        1, static_cast<std::int64_t>(military_power) + economic_power);
    // 三旬內的 -1/0/+1 恰好守恆；seed、勢力與 LOD 只旋轉相位，
    // 避免有限樣本因各級使用不同隨機 channel 而留下固定方向偏差。
    const auto phase = (static_cast<std::uint64_t>(xun) +
                        static_cast<std::uint64_t>(faction) +
                        static_cast<std::uint64_t>(lod) + world_seed % 3U) % 3U;
    const auto jitter = static_cast<std::int32_t>(phase) - 1;
    const auto growth = std::max<std::int32_t>(
        1, saturating_score(total / 200 + jitter));
    std::int32_t military_share = growth / 2;
    switch (action) {
    case FactionActionKind::Develop:
    case FactionActionKind::OfferAlliance:
    case FactionActionKind::PayTribute:
        military_share = growth / 4;
        break;
    case FactionActionKind::Prepare:
    case FactionActionKind::DeclareWar:
        military_share = growth * 3 / 4;
        break;
    case FactionActionKind::Expand:
    case FactionActionKind::StatisticalProgress:
        break;
    }
    return {military_share, growth - military_share};
}

} // namespace aetheria::ai
