#include "bridge/aetheria_core.h"

#include "core/api/version.h"
#include "core/time/tick.h"

#include <godot_cpp/core/class_db.hpp>

namespace aetheria::bridge {

void AetheriaCore::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("get_core_version"),
                                &AetheriaCore::get_core_version);
    godot::ClassDB::bind_method(godot::D_METHOD("tick_to_date", "tick"),
                                &AetheriaCore::tick_to_date);
}

godot::String AetheriaCore::get_core_version() const {
    return godot::String{aetheria::core_version()};
}

godot::Dictionary AetheriaCore::tick_to_date(std::int64_t tick) const {
    const auto date = aetheria::time::to_date(aetheria::time::Tick{tick});
    godot::Dictionary result;
    result["year"] = date.year;
    result["season"] = date.season;
    result["month"] = date.month;
    result["xun"] = date.xun;
    return result;
}

}  // namespace aetheria::bridge

