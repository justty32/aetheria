#pragma once

// FactionView 是 AI 唯一可見的外交知識快照，不保存 World 參考或真值指標。
// 世界層透過受友元保護的工廠建立快照；AI 只能讀取複本化的估計值。

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <aetheria/diplomacy/peace.h>

namespace aetheria::world {
struct FactionViewFactory;
}

namespace aetheria::ai {

using FactionKey = std::uint16_t;

struct KnownRelation {
    std::int32_t favor{};
    std::int32_t trust{};
    std::int32_t fear{};
    std::int32_t grievance{};

    constexpr bool operator==(const KnownRelation&) const noexcept = default;
};

struct FactionEstimate {
    FactionKey faction{};
    std::int32_t military_power{};
    std::int32_t economic_power{};
    std::uint16_t uncertainty_permyriad{};
    std::int64_t observed_at{};
    std::int32_t route_cost{};
    KnownRelation relation;

    constexpr bool operator==(const FactionEstimate&) const noexcept = default;
};

class FactionView {
    public:
    [[nodiscard]] FactionKey observer() const noexcept { return observer_; }
    [[nodiscard]] std::int32_t own_military_power() const noexcept {
        return own_military_power_;
    }
    [[nodiscard]] std::int32_t own_economic_power() const noexcept {
        return own_economic_power_;
    }
    [[nodiscard]] std::span<const FactionEstimate> estimates() const noexcept {
        return estimates_;
    }
    [[nodiscard]] std::optional<FactionEstimate>
    estimate(FactionKey faction) const noexcept;

    private:
    friend struct aetheria::world::FactionViewFactory;
    FactionView(FactionKey observer, std::int32_t own_military_power,
                std::int32_t own_economic_power,
                std::vector<FactionEstimate> estimates);

    FactionKey observer_{};
    std::int32_t own_military_power_{};
    std::int32_t own_economic_power_{};
    std::vector<FactionEstimate> estimates_;
};

// AI-facing 呼叫點只轉交公開公式，不接觸世界真值。
[[nodiscard]] std::int32_t
ai_peace_leverage(diplomacy::PeaceLeverageInput input,
                  diplomacy::PeaceLeverageWeights weights) noexcept;

} // namespace aetheria::ai
