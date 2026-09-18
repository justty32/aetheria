// core/rules/power_sources_faith.cpp：信徒來源、神祇隕落與教義衝突。

#include "core/rules/power_sources.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

#include "core/rules/power_sources_detail.h"

namespace aetheria::rules {

using detail::tier_value;

namespace {

[[nodiscard]] std::uint32_t apply_faith_event(const FaithIntermediaryEvent& event,
                                              std::span<FaithFollower> followers,
                                              std::uint32_t& total_tiers_lost) {
    std::uint32_t affected{};
    for (auto& follower : followers) {
        const auto before = resolve_follower(follower).tier;
        bool matched{};
        for (auto& source : follower.sources) {
            if (source.definition != nullptr && source.available &&
                source.definition->kind == PowerSourceKind::Faith &&
                source.definition->origin_id == event.deity_id) {
                source.available = false;
                source.unavailable_reason = "deity_fallen";
                matched = true;
            }
        }
        if (!matched) {
            continue;
        }
        const auto after = resolve_follower(follower).tier;
        total_tiers_lost += tier_value(before) - tier_value(after);
        ++affected;
    }
    return affected;
}

}  // namespace

PowerProfile resolve_follower(const FaithFollower& follower) {
    if (follower.entity_id == 0) {
        throw std::invalid_argument{"信徒 entity id 不可為零"};
    }
    return resolve_power_profile(follower.base_tier, follower.tier_cap,
                                 follower.base_attributes, follower.sources);
}

std::uint32_t deprive_faith(FaithFollower& follower, std::string_view deity_id,
                            std::string reason) {
    if (deity_id.empty() || reason.empty()) {
        throw std::invalid_argument{"信仰剝奪必須具名神祇與理由"};
    }
    const auto before = resolve_follower(follower).tier;
    for (auto& source : follower.sources) {
        if (source.definition != nullptr && source.available &&
            source.definition->kind == PowerSourceKind::Faith &&
            source.definition->origin_id == deity_id) {
            source.available = false;
            source.unavailable_reason = reason;
        }
    }
    const auto after = resolve_follower(follower).tier;
    return tier_value(before) - tier_value(after);
}

DeityFallResult fall_deity(RootFaithState& root, std::string_view deity_id,
                           std::span<FaithFollower> followers) {
    if (deity_id.empty()) {
        throw std::invalid_argument{"隕落神祇 id 不可為空"};
    }
    auto found = std::ranges::find_if(root.deities, [&](const RootDeityState& deity) {
        return deity.definition != nullptr && deity.definition->id == deity_id;
    });
    if (found == root.deities.end() || found->definition->significance !=
                                          world::Significance::World) {
        throw std::invalid_argument{"神祇必須是 root 中的 World 級實體"};
    }
    if (!found->alive) {
        throw std::invalid_argument{"神祇已經隕落"};
    }
    found->alive = false;
    DeityFallResult result;
    result.event.deity_id = std::string{deity_id};
    for (const auto& follower : followers) {
        const bool follows = std::ranges::any_of(follower.sources, [&](const auto& source) {
            return source.definition != nullptr && source.available &&
                   source.definition->kind == PowerSourceKind::Faith &&
                   source.definition->origin_id == deity_id;
        });
        if (follows) {
            result.event.follower_ids.push_back(follower.entity_id);
        }
    }
    result.affected_followers =
        apply_faith_event(result.event, followers, result.total_tiers_lost);
    return result;
}

std::uint32_t count_tenet_conflicts(std::span<const std::string> selected_tenets,
                                    std::span<const TenetDef> definitions) {
    std::map<std::string_view, const TenetDef*, std::less<>> by_id;
    for (const auto& definition : definitions) {
        if (definition.id.empty() || !by_id.emplace(definition.id, &definition).second) {
            throw std::invalid_argument{"教義定義 id 為空或重複"};
        }
    }
    std::set<std::pair<std::string_view, std::string_view>> conflicts;
    for (const auto& selected : selected_tenets) {
        const auto found = by_id.find(selected);
        if (found == by_id.end()) {
            throw std::invalid_argument{"信徒引用不存在的教義：" + selected};
        }
        for (const auto& other : found->second->conflicts) {
            if (std::ranges::find(selected_tenets, other) != selected_tenets.end()) {
                conflicts.emplace(std::min<std::string_view>(selected, other),
                                  std::max<std::string_view>(selected, other));
            }
        }
    }
    return static_cast<std::uint32_t>(conflicts.size());
}


}  // namespace aetheria::rules
