#pragma once

#include <cstdint>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace aetheria::bridge {

// AetheriaCore 是 Godot 可見的唯讀 facade。
// 場景樹擁有它。
// 它離開場景樹並被釋放後失效。
class AetheriaCore : public godot::Node {
  GDCLASS(AetheriaCore, godot::Node)

protected:
  static void _bind_methods();

public:
  [[nodiscard]] godot::String get_core_version() const;
  // 跨界 Tick 不在 core 合法域時回空 Dictionary，不讓不可信輸入觸發
  // AETH_CHECK。
  [[nodiscard]] godot::Dictionary tick_to_date(std::int64_t tick) const;
  // 產生一次性的 RegionTiles 唯讀快照；不在 bridge 或 Godot
  // 場景樹保存玩法狀態，包含 owner 與 portal 稀疏清單。seed／region_id 超出目前
  // Godot 整數可表達的非負範圍時回含 error 的 Dictionary。
  [[nodiscard]] godot::Dictionary generate_region(std::int64_t seed,
                                                  std::int64_t region_id) const;
};

} // namespace aetheria::bridge
