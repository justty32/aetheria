// playable_session_calibration.cpp：往返與城建校準。

#include "core/runtime/playable_session.h"

#include "core/serialize/normalized_state_hash.h"
#include "core/serialize/zone_codec.h"
#include "core/site/site_build_loop.h"
#include "core/site/site_materialize.h"


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

void PlayableSession::measure_site_roundtrips() {
    record_command("measure_roundtrips");
    if (residence_ != PlayableResidence::Region) {
        throw std::logic_error{"Site 往返量測必須從 Region 開始"};
    }
    roundtrip_hashes_.clear();
    for (std::uint32_t index = 0; index < 3U; ++index) {
        perform_enter_site();
        perform_leave_site();
        roundtrip_hashes_.push_back(
            serialize::normalized_state_hash(*region_, ruleset_));
    }
    if (!std::ranges::all_of(roundtrip_hashes_, [&](std::uint64_t hash) {
            return hash == roundtrip_hashes_.front();
        })) {
        throw std::logic_error{"Site 往返三次造成世界雜湊漂移"};
    }
}

std::uint64_t PlayableSession::run_city_economy_sample(
    std::string_view site_bytes, std::string_view region_bytes) const {
    auto sample_site = serialize::decode_zone(site_bytes, ruleset_);
    auto sample_region = serialize::decode_zone(region_bytes, ruleset_);
    auto& sample_tiles =
        std::get<zone::RegionPayload>(sample_region->payload).layers.at(0);
    const auto index = sample_tiles.index_of(coverage_tile_);
    sample_tiles.site[index].has_live_site = true;
    sample_tiles.site[index].lod = zone::LodLevel::Full;
    sample_site->lod = zone::LodLevel::Full;
    auto& state = site::city_build_state(*sample_site);
    state.buildings.push_back({"city.workshop", {8, 8}});
    zone::InMemoryZoneStore sample_store{ruleset_};
    site::SiteTurnPipeline pipeline{ruleset_, sample_store};
    const auto report = pipeline.advance_hours(
        *sample_site, *sample_region, 0, coverage_tile_, 240);
    return report.production_produced;
}

void PlayableSession::measure_city_management(std::uint32_t samples) {
    record_command("measure_calibration",
                   "samples = " + std::to_string(samples) + "\n");
    if (residence_ != PlayableResidence::Region || samples == 0U) {
        throw std::logic_error{"城建期望值量測要求 Region 駐留與正樣本數"};
    }
    const auto site_handle = acquire_coverage_site();
    auto& region_tiles = std::get<zone::RegionPayload>(region_->payload).layers.at(0);
    std::string site_bytes;
    const bool borrowed = manager_->with(site_handle, [&](zone::Zone& loaded) {
        site::enter_full_site(loaded, region_tiles, coverage_tile_);
        site_bytes = serialize::encode_zone(loaded, ruleset_);
    });
    if (!borrowed) {
        throw std::logic_error{"城建期望值量測無法取得 Site"};
    }
    const auto region_bytes = serialize::encode_zone(*region_, ruleset_);
    site::unload_site_zone(*manager_, site_handle, region_tiles, coverage_tile_, seed_,
                           region_id_, now(), ruleset_);

    calibration_n_ = samples;
    calibration_manual_total_ = 0;
    calibration_managed_total_ = 0;
    for (std::uint32_t index = 0; index < samples; ++index) {
        calibration_manual_total_ +=
            run_city_economy_sample(site_bytes, region_bytes);
        calibration_managed_total_ +=
            run_city_economy_sample(site_bytes, region_bytes);
    }
    if (calibration_managed_total_ == 0U) {
        throw std::logic_error{"城建期望值校準分母為 0"};
    }
    calibration_signed_error_percent_ =
        (static_cast<double>(calibration_manual_total_) -
         static_cast<double>(calibration_managed_total_)) /
        static_cast<double>(calibration_managed_total_) * 100.0;
    ++revision_;
}

} // namespace aetheria::runtime
