#include "core/local/local_materialize.h"

#include "core/zone/zone_key.h"

#include <stdexcept>
#include <utility>

namespace aetheria::local {
namespace {

[[nodiscard]] zone::ZoneKey local_key_for(zone::ZoneKey site_key,
                                          site::SiteXY coordinate) {
    if (zone::level_of(site_key) != zone::ZoneLevel::Site) {
        throw std::invalid_argument{"Local materialize 的父 key 必須是 Site"};
    }
    if (coordinate.x >= site::kSiteWidth || coordinate.y >= site::kSiteHeight) {
        throw std::out_of_range{"Local materialize 的 Site tile 座標超界"};
    }
    return zone::child_key(site_key, coordinate.x, coordinate.y);
}

[[nodiscard]] OpenLocalSkeleton prepare_local(const site::SiteProceduralLayer& parent,
                                              site::SiteXY coordinate,
                                              std::uint64_t site_seed,
                                              rules::FeatureId feature,
                                              const rules::Ruleset& ruleset) {
    const auto slow = project_local_slow_vars(parent, coordinate, site_seed, feature, ruleset);
    const auto seed = derive_local_seed(site_seed, coordinate.x, coordinate.y);
    return build_open_local_skeleton(slow, seed, ruleset);
}

}  // namespace

zone::Zone materialize_local_zone(zone::ZoneKey site_key,
                                  const site::SiteProceduralLayer& parent,
                                  site::SiteXY coordinate, std::uint64_t site_seed,
                                  rules::FeatureId feature,
                                  const rules::Ruleset& ruleset) {
    auto skeleton = prepare_local(parent, coordinate, site_seed, feature, ruleset);
    zone::Zone result{local_key_for(site_key, coordinate),
                      zone::LocalPayload{std::move(skeleton.tiles)}};
    result.lod = zone::LodLevel::Full;
    return result;
}

std::optional<zone::ZoneHandle> load_local_zone(zone::ZoneManager& manager,
                                               zone::ZoneKey local_key) {
    if (zone::level_of(local_key) != zone::ZoneLevel::Local) {
        throw std::invalid_argument{"Local load 只接受 Local ZoneKey"};
    }
    if (!manager.load(local_key)) {
        return std::nullopt;
    }
    return manager.get(local_key);
}

zone::ZoneHandle rematerialize_local_zone(zone::ZoneManager& manager,
                                          zone::ZoneKey site_key,
                                          const site::SiteProceduralLayer& parent,
                                          site::SiteXY coordinate, std::uint64_t site_seed,
                                          rules::FeatureId feature,
                                          const rules::Ruleset& ruleset) {
    const auto local_key = local_key_for(site_key, coordinate);
    if (manager.get(local_key).has_value()) {
        throw std::logic_error{"Local rematerialize 要求起點為未載入狀態"};
    }
    auto skeleton = prepare_local(parent, coordinate, site_seed, feature, ruleset);
    const auto loaded = load_local_zone(manager, local_key);
    if (!loaded.has_value()) {
        throw std::runtime_error{"Local rematerialize 找不到磁碟持久層"};
    }
    const bool borrowed = manager.with(*loaded, [&](zone::Zone& zone) {
        std::get<zone::LocalPayload>(zone.payload).tiles = std::move(skeleton.tiles);
        zone.lod = zone::LodLevel::Full;
    });
    if (!borrowed) {
        throw std::logic_error{"Local rematerialize 冷載後未留在 ZoneManager"};
    }
    return *loaded;
}

}  // namespace aetheria::local
