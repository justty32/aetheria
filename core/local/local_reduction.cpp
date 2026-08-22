#include "core/local/local_reduction.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>

namespace aetheria::local {
namespace {

[[nodiscard]] std::uint8_t percentage(std::uint64_t numerator,
                                      std::uint64_t denominator) {
    if (denominator == 0 || numerator > denominator) {
        throw std::invalid_argument{"Local 歸約比例超出 0～100%"};
    }
    const auto whole = numerator / denominator;
    const auto remainder = numerator % denominator;
    auto result = static_cast<std::uint8_t>(whole * 100U);
    std::uint64_t accumulated{};
    for (std::uint8_t step = 0; step < 100U; ++step) {
        if (accumulated >= denominator - remainder) {
            accumulated -= denominator - remainder;
            ++result;
        } else {
            accumulated += remainder;
        }
    }
    return result;
}

[[nodiscard]] std::optional<std::uint8_t> measure_structure(
    const LocalReductionState& state) {
    if (state.structure_segments.empty()) {
        return std::nullopt;
    }
    const auto intact = std::ranges::count_if(
        state.structure_segments, [](const auto& segment) { return !segment.damaged; });
    return percentage(static_cast<std::uint64_t>(intact), state.structure_segments.size());
}

[[nodiscard]] std::optional<world::FactionId> measure_controller(
    const LocalReductionState& state) {
    if (state.control_points.empty()) {
        return std::nullopt;
    }
    std::map<world::FactionId, std::size_t> counts;
    for (const auto point : state.control_points) {
        ++counts[point.controller];
    }
    auto winner = world::FactionId{};
    std::size_t winner_count{};
    bool tied{};
    for (const auto [controller, count] : counts) {
        if (count > winner_count) {
            winner = controller;
            winner_count = count;
            tied = false;
        } else if (count == winner_count) {
            tied = true;
        }
    }
    return tied ? world::FactionId{} : winner;
}

[[nodiscard]] std::optional<std::uint8_t> measure_resources(
    const LocalReductionState& state) {
    if (state.gathering_points.empty()) {
        return std::nullopt;
    }
    std::uint64_t remaining{};
    std::uint64_t capacity{};
    for (const auto point : state.gathering_points) {
        if (point.capacity == 0 || point.remaining > point.capacity) {
            throw std::invalid_argument{"Local 採集點剩餘量超出容量或容量為 0"};
        }
        if (remaining > std::numeric_limits<std::uint64_t>::max() - point.remaining ||
            capacity > std::numeric_limits<std::uint64_t>::max() - point.capacity) {
            throw std::overflow_error{"Local 採集點總量超出歸約容量"};
        }
        remaining += point.remaining;
        capacity += point.capacity;
    }
    return percentage(remaining, capacity);
}

[[nodiscard]] std::optional<std::uint16_t> measure_passability(
    const LocalReductionState& state) {
    if (state.passages.empty()) {
        return std::nullopt;
    }
    return std::ranges::min(state.passages, {}, &PassageState::traversal_cost).traversal_cost;
}

[[nodiscard]] std::size_t site_index(site::SiteXY coordinate) {
    if (coordinate.x >= site::kSiteWidth || coordinate.y >= site::kSiteHeight) {
        throw std::out_of_range{"Local 歸約的 SiteXY 超出 Site 邊界"};
    }
    return static_cast<std::size_t>(coordinate.y) * site::kSiteWidth + coordinate.x;
}

void validate_local_identity(const zone::Zone& local, site::SiteXY coordinate) {
    if (zone::level_of(local.key) != zone::ZoneLevel::Local ||
        zone::local_x_of(local.key) != coordinate.x ||
        zone::local_y_of(local.key) != coordinate.y) {
        throw std::invalid_argument{"Local 歸約的 ZoneKey 與 SiteXY 不一致"};
    }
    if (local.lod == zone::LodLevel::Absent) {
        throw std::logic_error{"不能歸約 L_ABSENT Local"};
    }
}

}  // namespace

site::LocalTileDelta ReductionTable::reduce(const LocalReductionState& state) {
    site::LocalTileDelta result;
    std::get<spatial::reduction::SnapshotValue<site::StructureIntegrityReduction>>(
        result.values_)
        .value = measure_structure(state);
    std::get<spatial::reduction::SnapshotValue<site::ControllerReduction>>(result.values_).value =
        measure_controller(state);
    std::get<spatial::reduction::SnapshotValue<site::ResourceYieldModifierReduction>>(
        result.values_)
        .value = measure_resources(state);
    std::get<spatial::reduction::SnapshotValue<site::PassabilityCostReduction>>(result.values_)
        .value = measure_passability(state);
    return result;
}

site::LocalTileDelta ReductionTable::reduce(const zone::Zone& local) {
    const auto* payload = std::get_if<zone::LocalPayload>(&local.payload);
    if (payload == nullptr) {
        throw std::invalid_argument{"Local ReductionTable::reduce 只接受 LocalPayload"};
    }
    return reduce(payload->reduction);
}

std::size_t ReductionTable::apply(site::SiteLayers& site_layers, site::SiteXY coordinate,
                                  const site::LocalTileDelta& delta) {
    if (!site_layers.local_reductions.valid_layout(site::kSiteTileCount)) {
        throw std::runtime_error{"不能把 Local 歸約量套用到版面無效的 Site storage"};
    }
    return delta.apply_to(site_layers.local_reductions.storage_.fields, site_index(coordinate));
}

std::size_t reduce_live_local(site::SiteLayers& site_layers, site::SiteXY coordinate,
                              const zone::Zone& live_local) {
    validate_local_identity(live_local, coordinate);
    return ReductionTable::apply(site_layers, coordinate, ReductionTable::reduce(live_local));
}

}  // namespace aetheria::local
