#pragma once

#include "core/zone/zone.h"
#include "core/world/region_movement.h"
#include "core/site/site_projection.h"
#include "core/site/site_build_loop.h"
#include "core/site/site_lifecycle.h"

#include <entt/core/type_traits.hpp>

namespace aetheria::serialize {

// AllComponents 是 registry snapshot 的唯一 component 順序清單。
// 編譯期序列化程式借用此型別，不擁有 component。
// 新型別只能加在尾端；調整順序會改變既有存檔位元流。
using AllComponents =
    entt::type_list<zone::ZoneMeta, world::StableId, world::RegionPosition, world::MovementPoints,
                    world::RegionMoveCommand, world::TurnClock, site::CityBuildState,
                    site::SiteDigest>;

// SavedSiteLayers 是 Site 存檔可見的資料層白名單。
// 程序層與易失層刻意不在清單中。
using SavedSiteLayers = entt::type_list<site::SitePersistentLayer>;

// Site payload 的存檔欄位只從 SavedSiteLayers 展開；新增可存層只有這一個入口。
template <typename Archive, typename Layers, typename... SavedLayers>
void archive_saved_site_layers(Archive& archive, Layers& layers, entt::type_list<SavedLayers...>) {
    (archive(layers.template get<SavedLayers>()), ...);
}

}  // namespace aetheria::serialize
