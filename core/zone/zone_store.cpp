#include "core/zone/zone_store.h"

#include "core/base/check.h"

namespace aetheria::zone {

bool InMemoryZoneStore::contains(ZoneKey key) const noexcept { return zones_.contains(key); }

std::unique_ptr<Zone> InMemoryZoneStore::take(ZoneKey key) {
    const auto found = zones_.find(key);
    if (found == zones_.end()) {
        return nullptr;
    }
    auto zone = std::move(found->second);
    zones_.erase(found);
    return zone;
}

void InMemoryZoneStore::save(std::unique_ptr<Zone> zone) {
    AETH_CHECK(zone != nullptr);
    const auto key = zone->key;
    zones_.insert_or_assign(key, std::move(zone));
}

bool InMemoryZoneStore::erase(ZoneKey key) noexcept { return zones_.erase(key) != 0; }

std::optional<time::Tick> InMemoryZoneStore::last_saved_tick(ZoneKey key) const noexcept {
    const auto found = zones_.find(key);
    if (found == zones_.end()) {
        return std::nullopt;
    }
    return found->second->last_saved_tick;
}

std::optional<std::size_t> InMemoryZoneStore::tile_count(ZoneKey key) const noexcept {
    const auto found = zones_.find(key);
    if (found == zones_.end()) {
        return std::nullopt;
    }
    std::size_t count = 0;
    for (const auto& [z, layer] : found->second->layers) {
        static_cast<void>(z);
        count += layer.tiles.size();
    }
    return count;
}

}  // namespace aetheria::zone
