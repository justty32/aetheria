#pragma once

// 世界層外交權威狀態：有向關係矩陣、條約／宣戰理由與持續戰爭事件。
// AI 不可 include 本標頭，只能接收複本化的 FactionView。

#include <aetheria/ai/faction_view.h>
#include <aetheria/diplomacy/peace.h>

#include "core/rules/ruleset.h"
#include "core/time/tick.h"
#include "core/world/region_tiles.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace aetheria::world {

struct DiplomaticRelation {
    std::int32_t favor{};
    std::int32_t trust{};
    std::int32_t fear{};
    std::int32_t grievance{};

    constexpr bool
    operator==(const DiplomaticRelation&) const noexcept = default;
};

struct ActiveTreaty {
    rules::TreatyDefId def{};
    std::array<FactionId, 2> parties{};
    time::Tick started{};
    std::optional<time::Tick> expires_at;

    constexpr bool operator==(const ActiveTreaty&) const noexcept = default;
};

struct CasusBelliClaim {
    FactionId owner{};
    FactionId target{};
    rules::CasusBelliDefId def{};
    time::Tick granted_at{};
    time::Tick expires_at{};

    constexpr bool operator==(const CasusBelliClaim&) const noexcept = default;
};

// WarEvent 是 Region 級持續事件；戰果以 participants[0] 的視角記錄。
struct WarEvent {
    std::array<FactionId, 2> participants{};
    std::optional<rules::CasusBelliDefId> cause;
    time::Tick started{};
    std::int32_t war_score{};
    std::array<std::int32_t, 2> weariness{};
    bool active{true};

    constexpr bool operator==(const WarEvent&) const noexcept = default;
};

// DiplomacyPersistentState 是外交存檔白名單；AI 觀測快取不屬此持久層。
// WorldDiplomacyState 匯出值複本，codec 載入後交回 restore 驗證並重建規則借用。
struct DiplomacyPersistentState {
    std::uint16_t faction_count{};
    std::uint64_t world_seed{};
    std::vector<DiplomaticRelation> relations;
    std::vector<ActiveTreaty> treaties;
    std::vector<CasusBelliClaim> casus_belli;
    std::vector<WarEvent> wars;

    constexpr bool
    operator==(const DiplomacyPersistentState&) const noexcept = default;
};

class WorldDiplomacyState {
    public:
    WorldDiplomacyState(std::uint16_t faction_count, std::uint64_t world_seed,
                        const rules::Ruleset& ruleset);

    [[nodiscard]] std::uint16_t faction_count() const noexcept {
        return faction_count_;
    }
    [[nodiscard]] const DiplomaticRelation& relation(FactionId observer,
                                                     FactionId target) const;
    void set_relation(FactionId observer, FactionId target,
                      DiplomaticRelation value);
    void adjust_relation(FactionId observer, FactionId target,
                         DiplomaticRelation delta);
    void advance_relations_xun();

    [[nodiscard]] const ActiveTreaty& start_treaty(rules::TreatyDefId def,
                                                   FactionId first,
                                                   FactionId second,
                                                   time::Tick now);
    [[nodiscard]] std::span<const ActiveTreaty> treaties() const noexcept {
        return treaties_;
    }

    [[nodiscard]] const CasusBelliClaim&
    grant_casus_belli(FactionId owner, FactionId target,
                      rules::CasusBelliDefId def, time::Tick now);
    [[nodiscard]] bool has_casus_belli(FactionId owner, FactionId target,
                                       rules::CasusBelliDefId def,
                                       time::Tick now) const noexcept;

    [[nodiscard]] WarEvent&
    declare_war(FactionId attacker, FactionId defender,
                std::optional<rules::CasusBelliDefId> cause, time::Tick now);
    void advance_war_xun(WarEvent& war,
                         std::array<std::uint32_t, 2> casualties);
    void add_war_score(WarEvent& war, std::int32_t score_delta) noexcept;
    [[nodiscard]] bool peace_pressure_reached(const WarEvent& war,
                                              std::size_t participant) const;
    [[nodiscard]] std::int32_t
    player_peace_leverage(const WarEvent& war, FactionId perspective,
                          std::int32_t third_party_pressure) const;
    [[nodiscard]] diplomacy::PeaceTerms
    peace_terms(std::int32_t leverage) const noexcept;
    [[nodiscard]] std::span<const WarEvent> wars() const noexcept {
        return wars_;
    }

    [[nodiscard]] DiplomacyPersistentState persistent_state() const;
    [[nodiscard]] static WorldDiplomacyState
    restore(DiplomacyPersistentState state, const rules::Ruleset& ruleset);

    void set_faction_truth(FactionId faction, std::int32_t military_power,
                           std::int32_t economic_power);
    void observe_faction(FactionId observer, FactionId target,
                         std::uint16_t uncertainty_permyriad, time::Tick now);

    private:
    friend struct FactionViewFactory;

    struct FactionTruth {
        std::int32_t military_power{};
        std::int32_t economic_power{};
    };
    struct KnowledgeRecord {
        std::int32_t military_power{};
        std::int32_t economic_power{};
        std::uint16_t uncertainty_permyriad{10000};
        time::Tick observed_at{};
    };

    [[nodiscard]] std::size_t faction_index(FactionId faction) const;
    [[nodiscard]] std::size_t matrix_index(FactionId observer,
                                           FactionId target) const;
    [[nodiscard]] std::int32_t
    bounded_relation(std::int64_t value) const noexcept;
    [[nodiscard]] std::int32_t
    estimated_power(FactionId observer, FactionId target, std::int32_t truth,
                    std::uint16_t uncertainty,
                    std::uint64_t channel) const noexcept;

    std::uint16_t faction_count_{};
    std::uint64_t world_seed_{};
    const rules::Ruleset* ruleset_{};
    std::vector<DiplomaticRelation> relations_;
    std::vector<ActiveTreaty> treaties_;
    std::vector<CasusBelliClaim> casus_belli_;
    std::vector<WarEvent> wars_;
    std::vector<FactionTruth> truths_;
    std::vector<KnowledgeRecord> knowledge_;
};

struct FactionViewFactory {
    [[nodiscard]] static ai::FactionView make(const WorldDiplomacyState& world,
                                              FactionId observer);
};

[[nodiscard]] ai::FactionView
make_faction_view(const WorldDiplomacyState& world, FactionId observer);

} // namespace aetheria::world
