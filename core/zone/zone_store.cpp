#include "core/zone/zone_store.h"

#include "core/serialize/zone_codec.h"

namespace aetheria::zone {

bool InMemoryZoneStore::contains(ZoneKey key) const { return snapshots_.contains(key); }

std::unique_ptr<Zone> InMemoryZoneStore::load(ZoneKey key) const {
    const auto found = snapshots_.find(key);
    if (found == snapshots_.end()) {
        return nullptr;
    }
    return serialize::decode_zone(found->second, ruleset_);
}

void InMemoryZoneStore::save(const Zone& zone) {
    snapshots_.insert_or_assign(zone.key, serialize::encode_zone(zone, ruleset_));
}

bool InMemoryZoneStore::erase(ZoneKey key) { return snapshots_.erase(key) != 0; }

}  // namespace aetheria::zone
