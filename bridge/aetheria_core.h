#pragma once

#include "core/narrative/narrative_event.h"
#include "core/runtime/playable_session.h"

#include <cstdint>
#include <memory>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace aetheria::bridge {

// AetheriaCore 是 Godot 可見的命令／批次快照 facade。場景樹擁有它；
// 權威玩法 session 仍完全位於純 C++ core。
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
  // 回傳 core 事件快照；非消耗式，讓顯示場景 free 後能只靠 core 重建。
  [[nodiscard]] godot::Array poll_events() const;
  // 建立新的 core 可玩 session；外部整數先在 bridge 驗證。
  [[nodiscard]] godot::Dictionary new_game(std::int64_t seed,
                                           std::int64_t region_id);
  // 一次打包整個 Region、部隊、事件與戰報；不存在逐格 getter。
  [[nodiscard]] godot::Dictionary get_playable_snapshot() const;
  // 送移動意圖；合法性仍由 core RegionTurnPipeline 裁決。
  [[nodiscard]] godot::Dictionary issue_move(std::int64_t unit_id,
                                             std::int64_t x,
                                             std::int64_t y);
  [[nodiscard]] godot::Dictionary advance_xun();
  [[nodiscard]] godot::Dictionary
  resolve_encounter(const godot::String &choice);

private:
  narrative::EventFeed event_feed_{narrative::make_fate_presentation_fixture()};
  std::unique_ptr<runtime::PlayableSession> playable_;
};

} // namespace aetheria::bridge
