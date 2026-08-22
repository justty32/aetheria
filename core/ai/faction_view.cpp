// FactionView 的只讀查詢與 AI-facing 共用和談公式呼叫點。
// 此檔由受限 target 編譯，include path 上看不到世界真值標頭。

#include <aetheria/ai/faction_view.h>

#include <algorithm>
#include <utility>

namespace aetheria::ai {

FactionView::FactionView(FactionKey observer, std::int32_t own_military_power,
                         std::int32_t own_economic_power,
                         std::vector<FactionEstimate> estimates)
    : observer_{observer}, own_military_power_{own_military_power},
      own_economic_power_{own_economic_power},
      estimates_{std::move(estimates)} {}

std::optional<FactionEstimate>
FactionView::estimate(FactionKey faction) const noexcept {
    const auto found =
        std::ranges::find(estimates_, faction, &FactionEstimate::faction);
    return found == estimates_.end() ? std::nullopt
                                     : std::optional<FactionEstimate>{*found};
}

std::int32_t
ai_peace_leverage(diplomacy::PeaceLeverageInput input,
                  diplomacy::PeaceLeverageWeights weights) noexcept {
    return diplomacy::calculate_peace_leverage(input, weights);
}

} // namespace aetheria::ai
