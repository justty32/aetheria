#pragma once

#include "core/rules/ruleset.h"
#include "core/time/tick.h"
#include "core/world/region_tiles.h"
#include "core/zone/zone.h"
#include "core/zone/zone_store.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace aetheria::world {

inline constexpr std::int32_t kMovementPointScale = 2;

// StableId 是不受 EnTT entity 配發歷史影響的持久實體鍵。
// Zone registry 擁有 component，正規化雜湊與命令排序只讀取其值。
// 所屬 entity 被銷毀或 registry 析構後失效；同一 zone 內必須唯一。
struct StableId {
    std::uint64_t uid{};

    template <typename Archive> void serialize(Archive& archive) { archive(uid); }
    constexpr bool operator==(const StableId&) const noexcept = default;
};

// RegionPosition 是單位在某個 Region layer 的整數座標。
// Zone registry 擁有 component，移動系統在命令執行階段修改。
// 所屬 entity 被銷毀或 registry 析構後失效。
struct RegionPosition {
    std::int8_t z{};
    RegionXY tile;

    template <typename Archive> void serialize(Archive& archive) { archive(z, tile.x, tile.y); }
    constexpr bool operator==(const RegionPosition&) const noexcept = default;
};

// MovementPoints 是單位本旬剩餘與每旬可用的整數 MP（1 點 = 半個顯示 MP）。
// Zone registry 擁有 component，旬回合在執行移動前補滿 current。
// 所屬 entity 被銷毀或 registry 析構後失效；值不得為負。
struct MovementPoints {
    std::int32_t current{};
    std::int32_t per_xun{};

    template <typename Archive> void serialize(Archive& archive) { archive(current, per_xun); }
    constexpr bool operator==(const MovementPoints&) const noexcept = default;
};

// RegionMoveCommand 是已排入或持續執行中的下一次移動命令。
// Zone registry 擁有 component；stage 1 收集後，stage 2 每旬自動推進。
// 抵達、無路或外部中斷時移除；因屬權威狀態所以進存檔與正規化雜湊。
struct RegionMoveCommand {
    RegionXY target;
    bool collected{};

    template <typename Archive> void serialize(Archive& archive) {
        archive(target.x, target.y, collected);
    }
    constexpr bool operator==(const RegionMoveCommand&) const noexcept = default;
};

// TurnClock 是 Region 旬回合的權威時鐘。
// Zone registry 的 placeholder entity 擁有它，流水線在 stage 7 推進。
// 所屬 entity 被銷毀或 registry 析構後失效。
struct TurnClock {
    time::Tick now{};

    template <typename Archive> void serialize(Archive& archive) {
        auto raw = static_cast<std::int64_t>(now);
        archive(raw);
        now = time::Tick{raw};
    }
    constexpr bool operator==(const TurnClock&) const noexcept = default;
};

// Region 上的 TurnClock 是唯一全局時鐘；下層流水線只透過這個入口讀寫。
[[nodiscard]] TurnClock& turn_clock(zone::Zone& region);
[[nodiscard]] const TurnClock& turn_clock(const zone::Zone& region);

// RegionPath 是包含起點與終點的四鄰接路徑及其整數總成本。
// 呼叫端擁有回傳值。
// 值本身永不失效；空 optional 代表無可行路徑。
struct RegionPath {
    std::vector<RegionXY> tiles;
    std::int64_t cost{};
};

[[nodiscard]] std::int32_t region_step_cost(const RegionTiles& tiles, RegionXY from, RegionXY to,
                                            const rules::Ruleset& ruleset, std::uint8_t season);
[[nodiscard]] std::int32_t minimum_region_step_cost(const rules::Ruleset& ruleset,
                                                    std::uint8_t season);
[[nodiscard]] std::optional<RegionPath> find_region_path(const RegionTiles& tiles, RegionXY start,
                                                         RegionXY goal,
                                                         const rules::Ruleset& ruleset,
                                                         std::uint8_t season,
                                                         std::uint32_t heuristic_multiplier = 1);

enum class TurnStage : std::uint8_t {
    PlayerCommands = 1,
    CommandExecution = 2,
    Encounters = 3,
    FactionAi = 4,
    WorldSimulation = 5,
    Events = 6,
    TurnEnd = 7,
};

using TurnStageObserver = std::function<void(TurnStage)>;
using LiveSiteReductionPass = std::function<void(zone::Zone&)>;
using FactionAiPass = std::function<void(time::Tick)>;
using ScriptTurnPass = std::function<void(time::Tick)>;

// RegionTurnPipeline 依固定七階段推進單一 Region，並在 stage 7 自動存檔。
// 呼叫端擁有 pipeline；它只借用 Ruleset 與 ZoneStore。
// 兩個被借用物件失效後不可再呼叫；stub 階段仍會通知 observer。
class RegionTurnPipeline {
public:
    RegionTurnPipeline(const rules::Ruleset& ruleset, zone::ZoneStore& store)
        : ruleset_{ruleset}, store_{store} {}

    void issue_move(zone::Zone& region, StableId unit, RegionXY target) const;
    void advance_xun(zone::Zone& region, const TurnStageObserver& observer = {},
                     const LiveSiteReductionPass& live_site_reduction = {},
                     const FactionAiPass& faction_ai = {},
                     const ScriptTurnPass& scripts = {}) const;
    // 下層逐小時流水線已把全局時鐘推到旬界後，只結算剛結束的一旬，不再推時鐘。
    void settle_elapsed_xun(zone::Zone& region, const TurnStageObserver& observer = {},
                            const LiveSiteReductionPass& live_site_reduction = {},
                            const FactionAiPass& faction_ai = {},
                            const ScriptTurnPass& scripts = {}) const;

private:
    void run_xun_stages(zone::Zone& region, time::Tick simulation_start,
                        const TurnStageObserver& observer,
                        const LiveSiteReductionPass& live_site_reduction,
                        const FactionAiPass& faction_ai,
                        const ScriptTurnPass& scripts) const;

    const rules::Ruleset& ruleset_;
    zone::ZoneStore& store_;
};

}  // namespace aetheria::world
