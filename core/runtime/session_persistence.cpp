#include "core/runtime/session_persistence.h"

#include "core/zone/save_manifest.h"
#include "core/zone/zone_manager.h"
#include "core/zone/zone_store.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace aetheria::runtime {

void save_session(zone::ZoneStore& active_store, zone::ZoneStore& destination,
                  zone::ZoneManager& manager, std::uint64_t world_seed,
                  time::Tick now) {
    manager.save_all();
    const auto source_keys = active_store.stored_keys();
    if (&active_store != &destination) {
        const auto destination_keys = destination.stored_keys();
        for (const auto key : destination_keys) {
            if (!std::ranges::contains(source_keys, key)) {
                static_cast<void>(destination.erase(key));
            }
        }
        for (const auto key : source_keys) {
            auto snapshot = active_store.load(key);
            if (snapshot == nullptr) {
                throw std::runtime_error{"列舉到的來源 zone 無法載入：" +
                                         std::to_string(zone::value_of(key))};
            }
            destination.save(*snapshot);
        }
    }

    auto manifest = destination.manifest().value_or(zone::SaveManifest{});
    manifest.world_seed = world_seed;
    manifest.now = now;
    destination.write_manifest(manifest);
}

SessionLoadMetadata inspect_session_save(const zone::ZoneStore& store) {
    if (!store.manifest().has_value()) {
        throw std::runtime_error{"存檔缺少 manifest"};
    }
    std::vector<zone::ZoneKey> regions;
    for (const auto key : store.stored_keys()) {
        if (zone::level_of(key) == zone::ZoneLevel::Region) {
            regions.push_back(key);
        }
    }
    if (regions.size() != 1U) {
        throw std::runtime_error{"可玩 session 存檔必須恰有一個 Region，實際=" +
                                 std::to_string(regions.size())};
    }
    const auto& manifest = *store.manifest();
    return {manifest.world_seed, zone::region_id_of(regions.front()), regions.front(),
            manifest.now};
}

}  // namespace aetheria::runtime
