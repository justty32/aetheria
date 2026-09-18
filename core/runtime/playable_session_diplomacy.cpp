// playable_session_diplomacy.cpp：外交初始化與條約。

#include "core/runtime/playable_session.h"

#include "core/world/faction_ai.h"


#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>


namespace aetheria::runtime {

void PlayableSession::initialize_diplomacy() {
    const auto root = manager_->get(zone::kRootZone);
    if (!root.has_value()) {
        throw std::logic_error{"ZoneManager 缺少既有 root"};
    }
    const bool initialized = manager_->with(*root, [&](zone::Zone& loaded) {
        if (loaded.diplomacy.has_value()) {
            throw std::logic_error{"新遊戲 root 已有外交狀態"};
        }
        loaded.diplomacy.emplace(
            ruleset_.civilization_rules().factions.faction_count, seed_, ruleset_);
        diplomacy_ = &*loaded.diplomacy;
    });
    if (!initialized || diplomacy_ == nullptr) {
        throw std::logic_error{"無法在 ZoneManager root 建立外交狀態"};
    }
    diplomacy_->set_faction_truth(world::FactionId{1}, 120'000, 80'000);
    diplomacy_->set_faction_truth(world::FactionId{2}, 48'000, 42'000);
    diplomacy_->set_faction_truth(world::FactionId{3}, 60'000, 70'000);
    const auto faction_count = ruleset_.civilization_rules().factions.faction_count;
    for (std::uint16_t faction = 4; faction <= faction_count; ++faction) {
        diplomacy_->set_faction_truth(world::FactionId{faction}, 0, 0);
    }
    for (std::uint16_t observer = 1; observer <= faction_count; ++observer) {
        for (std::uint16_t target = 1; target <= faction_count; ++target) {
            if (observer != target) {
                diplomacy_->observe_faction(world::FactionId{observer},
                                            world::FactionId{target}, 0,
                                            time::Tick{0}, 4);
            }
        }
    }
    world::set_managed_faction_goal(*diplomacy_, world::FactionId{2},
                                    ai::FactionGoal::Conquer);
}

void PlayableSession::sign_peace_treaty() {
    record_command("sign_treaty");
    if (residence_ != PlayableResidence::Region) {
        throw std::logic_error{"外交條約只能在 Region 操作"};
    }
    const auto peace = ruleset_.find_treaty("treaty.peace");
    if (!peace.has_value()) {
        throw std::runtime_error{"外交規則缺少 treaty.peace"};
    }
    const bool exists = std::ranges::any_of(diplomacy_->treaties(), [&](const auto& treaty) {
        return treaty.def == *peace &&
               ((treaty.parties[0] == world::FactionId{1} &&
                 treaty.parties[1] == world::FactionId{2}) ||
                (treaty.parties[0] == world::FactionId{2} &&
                 treaty.parties[1] == world::FactionId{1}));
    });
    if (exists) {
        throw std::logic_error{"和平條約已存在"};
    }
    static_cast<void>(diplomacy_->start_treaty(
        *peace, world::FactionId{1}, world::FactionId{2}, now()));
    ++revision_;
    append_event(PlayableEventKind::TreatySigned, {}, 1, 2);
}

} // namespace aetheria::runtime
