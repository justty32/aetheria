#pragma once

#include <cstdint>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace aetheria::bridge {

// AetheriaCore 是 Godot 可見的唯讀 M0 facade。
// 場景樹擁有它。
// 它離開場景樹並被釋放後失效。
class AetheriaCore : public godot::Node {
    GDCLASS(AetheriaCore, godot::Node)

protected:
    static void _bind_methods();

public:
    [[nodiscard]] godot::String get_core_version() const;
    [[nodiscard]] godot::Dictionary tick_to_date(std::int64_t tick) const;
};

}  // namespace aetheria::bridge
