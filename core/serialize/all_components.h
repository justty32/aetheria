#pragma once

#include "core/zone/zone.h"
#include "core/world/region_movement.h"
#include "core/site/site_projection.h"

#include <entt/core/type_traits.hpp>

namespace aetheria::serialize {

// AllComponents 是 registry snapshot 的唯一 component 順序清單。
// 編譯期序列化程式借用此型別，不擁有 component。
// 新型別只能加在尾端；調整順序會改變既有存檔位元流。
using AllComponents =
    entt::type_list<zone::ZoneMeta, world::StableId, world::RegionPosition, world::MovementPoints,
                    world::RegionMoveCommand, world::TurnClock>;

// SavedSiteLayers 是未來 Site 存檔可見的資料層白名單。
// 程序層與易失層刻意不在清單中；M2.1 不改動既有 zone 位元流。
using SavedSiteLayers = entt::type_list<site::SitePersistentLayer>;

}  // namespace aetheria::serialize
