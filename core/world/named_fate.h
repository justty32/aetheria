#pragma once

// named_fate.h 定義具名成員命運的三階段演算法、離線補算與持久事件 ledger。

#include "core/time/tick.h"
#include "core/world/region_tiles.h"
#include "core/world/significance.h"

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace aetheria::world {

inline constexpr std::uint32_t kFateBasisPoints = 10'000;

enum class FateOutcome : std::uint8_t {
    Unharmed,
    Injured,
    PropertyLost,
    Displaced,
    Died,
};

enum class FateNarrativeKind : std::uint8_t {
    NamedOutcome,
    NamedQuotaOverflow,
};

enum class FateResolutionPath : std::uint8_t {
    Present,
    ReloadCatchUp,
};

// 六個欄位是唯一可用的個體修正；每一項都直接對應可敘述的世界內原因。
struct FateModifiers {
    std::uint32_t status_basis_points{kFateBasisPoints};
    std::uint32_t wealth_basis_points{kFateBasisPoints};
    std::uint32_t relationship_basis_points{kFateBasisPoints};
    std::uint32_t occupation_basis_points{kFateBasisPoints};
    std::uint32_t vulnerability_basis_points{kFateBasisPoints};
    std::uint32_t district_basis_points{kFateBasisPoints};

    template <typename Archive> void serialize(Archive& archive) {
        archive(status_basis_points, wealth_basis_points, relationship_basis_points,
                occupation_basis_points, vulnerability_basis_points, district_basis_points);
    }
    bool operator==(const FateModifiers&) const = default;
};

struct NamedFateMember {
    std::uint64_t entity_uid{};
    std::uint64_t cohort_id{};
    std::string name_key;
    Significance significance{Significance::Local};
    std::string significance_reason;
    FateModifiers modifiers;
    bool marked{};
    bool rescued{};
    bool has_outcome{};
    FateOutcome outcome{FateOutcome::Unharmed};

    template <typename Archive> void serialize(Archive& archive) {
        archive(entity_uid, cohort_id, name_key, significance, significance_reason, modifiers,
                marked, rescued, has_outcome, outcome);
    }
    bool operator==(const NamedFateMember&) const = default;
};

struct FateCrisis {
    std::uint64_t event_id{};
    std::uint64_t cohort_id{};
    std::uint64_t site_key{};
    std::uint32_t base_loss_basis_points{};
    std::uint32_t relief_basis_points{};
    time::Tick occurred_at{};
    std::string place_key;

    bool operator==(const FateCrisis&) const = default;
};

// 階段 1 的結果是權威總量；離線路徑只排隊這份已計算完成的值。
struct FateStageOne {
    FateCrisis crisis;
    std::uint32_t population_before{};
    std::uint32_t total_loss{};
    std::uint32_t population_after{};

    template <typename Archive> void serialize(Archive& archive) {
        auto raw_tick = static_cast<std::int64_t>(crisis.occurred_at);
        archive(crisis.event_id, crisis.cohort_id, crisis.site_key,
                crisis.base_loss_basis_points, crisis.relief_basis_points, raw_tick,
                crisis.place_key, population_before, total_loss, population_after);
        crisis.occurred_at = time::Tick{raw_tick};
    }
    bool operator==(const FateStageOne&) const = default;
};

struct FateNarrativeEvent {
    FateNarrativeKind kind{FateNarrativeKind::NamedOutcome};
    std::uint64_t event_id{};
    std::uint64_t cohort_id{};
    std::uint64_t entity_uid{};
    std::string place_key;
    std::string person_key;
    std::string template_key;
    FateOutcome outcome{FateOutcome::Unharmed};

    template <typename Archive> void serialize(Archive& archive) {
        archive(kind, event_id, cohort_id, entity_uid, place_key, person_key, template_key,
                outcome);
    }
    bool operator==(const FateNarrativeEvent&) const = default;
};

// Region entity 用 pending；Site entity 用 members/events。三者一起走既有 registry 存檔。
struct NamedFateLedger {
    std::vector<NamedFateMember> members;
    std::vector<FateStageOne> pending;
    std::vector<FateNarrativeEvent> events;

    template <typename Archive> void serialize(Archive& archive) {
        archive(members, pending, events);
    }
    bool operator==(const NamedFateLedger&) const = default;
};

struct FateRules {
    // 50% 是一般難度的 mark 倖存偏袒；硬核模式設成 10000 即不偏袒。
    std::uint32_t marked_death_bias_basis_points{5'000};
    std::array<std::uint32_t, 4> surviving_outcome_weights{5'500, 1'500, 2'000, 1'000};
};

struct NamedFateDecision {
    std::uint64_t entity_uid{};
    std::uint32_t death_basis_points{};
    FateOutcome outcome{FateOutcome::Unharmed};

    bool operator==(const NamedFateDecision&) const = default;
};

struct FateResolution {
    FateResolutionPath path{FateResolutionPath::Present};
    std::uint32_t stage_one_total_loss{};
    std::uint32_t named_deaths{};
    std::uint32_t unnamed_deaths{};
    bool named_quota_overflow{};
    std::vector<NamedFateDecision> decisions;
};

struct FateExecutionCounters {
    std::uint64_t present{};
    std::uint64_t reload_catch_up{};
};

[[nodiscard]] bool valid_named_fate_ledger(const NamedFateLedger& ledger) noexcept;
[[nodiscard]] std::uint64_t fate_seed(std::uint64_t event_id, std::uint64_t cohort_id,
                                      std::uint64_t entity_uid) noexcept;
[[nodiscard]] std::uint32_t adjusted_death_basis_points(const FateStageOne& stage_one,
                                                        const NamedFateMember& member,
                                                        const FateRules& rules);
[[nodiscard]] FateOutcome roll_fate_outcome(std::mt19937_64& rng,
                                            std::uint32_t death_basis_points,
                                            const FateRules& rules);

class FateResolver {
public:
    // 只此入口可改 Region 人口；relief 在算 total_loss 前生效。
    [[nodiscard]] static FateStageOne apply_stage_one(RegionTiles& tiles, RegionXY coordinate,
                                                       FateCrisis crisis);
    [[nodiscard]] static FateResolution resolve_present(NamedFateLedger& site_ledger,
                                                        const FateStageOne& stage_one,
                                                        const FateRules& rules,
                                                        FateExecutionCounters& counters);
    static void enqueue_absent(NamedFateLedger& region_ledger, FateStageOne stage_one);
    [[nodiscard]] static std::vector<FateResolution> resolve_reload(
        NamedFateLedger& site_ledger, NamedFateLedger& region_ledger, std::uint64_t site_key,
        const FateRules& rules, FateExecutionCounters& counters);
};

}  // namespace aetheria::world
