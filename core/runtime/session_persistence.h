#pragma once

#include "core/time/tick.h"
#include "core/zone/zone_key.h"

#include <cstdint>

namespace aetheria::zone {
class ZoneManager;
class ZoneStore;
struct SaveManifest;
}  // namespace aetheria::zone

namespace aetheria::runtime {

// SessionLoadMetadata 是從 manifest 與已存 zone 集合推導出的冷讀入口資料。
struct SessionLoadMetadata {
    std::uint64_t world_seed{};
    std::uint32_t region_id{};
    zone::ZoneKey region_key{};
    time::Tick now{};
};

// 先把 manager 的全部 live zone 寫進 active_store，再完整鏡像到 destination，
// 最後才寫 manifest。active_store 與 destination 可以是同一物件。
void save_session(zone::ZoneStore& active_store, zone::ZoneStore& destination,
                  zone::ZoneManager& manager, std::uint64_t world_seed,
                  time::Tick now);

// 僅讀 manifest 與 zone keys，不生成世界；存檔必須恰有一個 Region。
[[nodiscard]] SessionLoadMetadata inspect_session_save(const zone::ZoneStore& store);

}  // namespace aetheria::runtime
