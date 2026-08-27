#include "core/zone/zone_store.h"

#include "core/serialize/zone_codec.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace aetheria::zone {

namespace {

[[nodiscard]] std::uint64_t take_next(std::uint64_t& next,
                                      std::optional<std::uint64_t> preferred,
                                      std::string_view description) {
    const auto value = preferred.value_or(next);
    if (value == 0U || value < next) {
        throw std::logic_error{std::string{description} + " 已配發或為 0：" +
                               std::to_string(value)};
    }
    if (value == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error{std::string{description} + " 配發器耗盡"};
    }
    next = value + 1U;
    return value;
}

}  // namespace

std::uint64_t ZoneStore::allocate_detached_id() {
    auto& manifest = allocator_manifest();
    return take_next(manifest.next_detached_id, std::nullopt, "detached id");
}

std::uint64_t ZoneStore::allocate_entity_uid(
    std::optional<std::uint64_t> preferred) {
    auto& manifest = allocator_manifest();
    return take_next(manifest.next_entity_uid, preferred, "entity uid");
}

void ZoneStore::observe_entity_uid(std::uint64_t uid) {
    if (uid == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error{"既有 entity uid 已達上限"};
    }
    auto& manifest = allocator_manifest();
    manifest.next_entity_uid = std::max(manifest.next_entity_uid, uid + 1U);
}

SaveManifest& ZoneStore::allocator_manifest() {
    return fallback_allocator_manifest_.has_value()
               ? *fallback_allocator_manifest_
               : fallback_allocator_manifest_.emplace();
}

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

SaveManifest& InMemoryZoneStore::allocator_manifest() {
    return manifest_.has_value() ? *manifest_ : manifest_.emplace();
}

}  // namespace aetheria::zone
