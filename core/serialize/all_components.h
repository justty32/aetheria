#pragma once

#include "core/zone/zone.h"
#include "core/world/region_movement.h"

#include <entt/core/type_traits.hpp>

namespace aetheria::serialize {

// AllComponents 是 registry snapshot 的唯一 component 順序清單。
// 編譯期序列化程式借用此型別，不擁有 component。
// 新型別只能加在尾端；調整順序會改變既有存檔位元流。
using AllComponents =
    entt::type_list<zone::ZoneMeta, world::StableId, world::RegionPosition, world::MovementPoints,
                    world::RegionMoveCommand, world::TurnClock>;

}  // namespace aetheria::serialize
