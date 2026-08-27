#include "core/zone/zone_store.h"

#include "core/serialize/zone_codec.h"

#include <stdexcept>

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

std::vector<ZoneKey> InMemoryZoneStore::stored_keys() const {
    std::vector<ZoneKey> result;
    result.reserve(snapshots_.size());
    for (const auto& [key, snapshot] : snapshots_) {
        static_cast<void>(snapshot);
        result.push_back(key);
    }
    return result;
}

void InMemoryZoneStore::write_manifest(const SaveManifest& manifest) {
    if (manifest.format_version != serialize::kSaveFormatVersion) {
        throw std::runtime_error{"拒絕寫入非目前版本的 manifest"};
    }
    if (!contains(kRootZone)) {
        throw std::runtime_error{"寫 manifest 前必須先寫 root zone"};
    }
    if (manifest_.has_value() && manifest.dims != manifest_->dims) {
        throw std::runtime_error{"既有存檔的 WorldDims 不可變更"};
    }
    manifest_ = manifest;
}

}  // namespace aetheria::zone
