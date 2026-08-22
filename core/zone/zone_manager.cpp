#include "core/zone/zone_manager.h"

#include "core/world/region_movement.h"

#include <ranges>
#include <string>

namespace aetheria::zone {
namespace {

void rebuild_uid_index(Zone& zone) {
    zone.uid_index.clear();
    for (const auto entity : zone.reg.view<const world::StableId>()) {
        const auto uid = zone.reg.get<const world::StableId>(entity).uid;
        if (!zone.uid_index.emplace(uid, entity).second) {
            throw std::runtime_error{"zone 含重複 StableId：" + std::to_string(uid)};
        }
    }
}

[[nodiscard]] bool has_complete_procedural_layer(const Zone& zone) noexcept {
    if (const auto* region = std::get_if<RegionPayload>(&zone.payload)) {
        return !region->layers.empty() &&
               std::ranges::all_of(region->layers,
                                   [](const auto& layer) { return layer.second.valid_layout(); });
    }
    if (const auto* site = std::get_if<SitePayload>(&zone.payload)) {
        return site->layers.procedural.valid_layout();
    }
    if (const auto* local = std::get_if<LocalPayload>(&zone.payload)) {
        return local->layers.contains(0) &&
               std::ranges::all_of(local->layers,
                                   [](const auto& layer) { return layer.second.valid_layout(); });
    }
    return true;
}

}  // namespace

ZoneManager::ZoneManager(ZoneStore& store, ZoneMaterializer materializer)
    : store_{store}, materializer_{std::move(materializer)} {
    auto root = store_.load(kRootZone);
    if (root == nullptr) {
        root = std::make_unique<Zone>(kRootZone);
    }
    AETH_CHECK(root->key == kRootZone);
    zones_.emplace(kRootZone, std::move(root));
}

std::optional<ZoneHandle> ZoneManager::get(ZoneKey key) const noexcept {
    if (!zones_.contains(key)) {
        return std::nullopt;
    }
    return ZoneHandle{key};
}

std::optional<ZoneHandle> ZoneManager::acquire(ZoneKey key) {
    return acquire(key, materializer_);
}

std::optional<ZoneHandle> ZoneManager::acquire(ZoneKey key,
                                               const ZoneMaterializer& materializer) {
    AETH_CHECK(!in_tick_);
    if (const auto loaded = get(key)) {
        if (has_complete_procedural_layer(*zones_.at(key))) {
            return loaded;
        }
        if (!materializer) {
            return std::nullopt;
        }
    }

    std::unique_ptr<Zone> persistent;
    if (const auto found = zones_.find(key); found != zones_.end()) {
        persistent = std::move(found->second);
        zones_.erase(found);
        if (key != kRootZone) {
            remove_from_tick_order(key);
        }
    } else {
        persistent = store_.load(key);
    }

    if (!materializer) {
        if (persistent == nullptr || !has_complete_procedural_layer(*persistent)) {
            return std::nullopt;
        }
        add_loaded(std::move(persistent));
        return ZoneHandle{key};
    }

    auto materialized = std::invoke(materializer, key, std::move(persistent));
    if (materialized == nullptr || materialized->key != key ||
        !has_complete_procedural_layer(*materialized)) {
        return std::nullopt;
    }
    add_loaded(std::move(materialized));
    return ZoneHandle{key};
}

ZoneHandle ZoneManager::require(ZoneKey key) {
    AETH_CHECK(!in_tick_);
    if (const auto loaded = get(key)) {
        return *loaded;
    }
    if (!load(key)) {
        throw std::runtime_error{"required zone is absent: " + std::to_string(value_of(key))};
    }
    return ZoneHandle{key};
}

bool ZoneManager::load(ZoneKey key) {
    AETH_CHECK(!in_tick_);
    if (zones_.contains(key)) {
        return true;
    }
    auto zone = store_.load(key);
    if (zone == nullptr) {
        return false;
    }
    AETH_CHECK(zone->key == key);
    add_loaded(std::move(zone));
    return true;
}

ZoneHandle ZoneManager::materialize(ZoneKey key) {
    AETH_CHECK(!in_tick_);
    if (const auto loaded = get(key)) {
        return *loaded;
    }
    if (load(key)) {
        return ZoneHandle{key};
    }
    add_loaded(std::make_unique<Zone>(key));
    return ZoneHandle{key};
}

ZoneHandle ZoneManager::adopt(std::unique_ptr<Zone> zone) {
    AETH_CHECK(!in_tick_);
    if (zone == nullptr) {
        throw std::invalid_argument{"ZoneManager 不能接管空 zone"};
    }
    const auto key = zone->key;
    if (zones_.contains(key)) {
        throw std::logic_error{"ZoneManager 不能重複接管已載入 zone"};
    }
    add_loaded(std::move(zone));
    return ZoneHandle{key};
}

bool ZoneManager::unload(ZoneKey key) {
    AETH_CHECK(!in_tick_);
    if (key == kRootZone) {
        return false;
    }
    const auto found = zones_.find(key);
    if (found == zones_.end()) {
        return false;
    }
    found->second->last_saved_tick = current_tick_;
    store_.save(*found->second);
    zones_.erase(found);
    remove_from_tick_order(key);
    return true;
}

bool ZoneManager::destroy(ZoneKey key) {
    AETH_CHECK(!in_tick_);
    if (key == kRootZone) {
        return false;
    }
    const bool was_stored = store_.erase(key);
    const bool was_loaded = zones_.erase(key) != 0;
    if (was_loaded) {
        remove_from_tick_order(key);
    }
    return was_loaded || was_stored;
}

void ZoneManager::save_all() {
    AETH_CHECK(!in_tick_);
    for (auto& [key, zone] : zones_) {
        static_cast<void>(key);
        zone->last_saved_tick = current_tick_;
        store_.save(*zone);
    }
}

void ZoneManager::queue_materialize(ZoneKey key) {
    AETH_CHECK(in_tick_);
    commands_.emplace_back(MaterializeCommand{key});
}

void ZoneManager::queue_unload(ZoneKey key) {
    AETH_CHECK(in_tick_);
    commands_.emplace_back(UnloadCommand{key});
}

void ZoneManager::queue_destroy(ZoneKey key) {
    AETH_CHECK(in_tick_);
    commands_.emplace_back(DestroyCommand{key});
}

std::vector<ZoneKey> ZoneManager::loaded_keys() const {
    std::vector<ZoneKey> result;
    result.reserve(zones_.size());
    for (const auto& [key, zone] : zones_) {
        static_cast<void>(zone);
        result.push_back(key);
    }
    return result;
}

void ZoneManager::add_loaded(std::unique_ptr<Zone> zone) {
    AETH_CHECK(zone != nullptr);
    rebuild_uid_index(*zone);
    const auto key = zone->key;
    const auto [position, inserted] = zones_.emplace(key, std::move(zone));
    static_cast<void>(position);
    AETH_CHECK(inserted);
    if (key != kRootZone) {
        tick_order_.push_back(key);
    }
}

void ZoneManager::remove_from_tick_order(ZoneKey key) {
    const auto found = std::ranges::find(tick_order_, key);
    AETH_CHECK(found != tick_order_.end());
    tick_order_.erase(found);
}

void ZoneManager::flush_commands() {
    auto commands = std::move(commands_);
    commands_.clear();
    for (const auto& command : commands) {
        std::visit(
            [this](const auto& concrete) {
                using Command = std::remove_cvref_t<decltype(concrete)>;
                if constexpr (std::same_as<Command, MaterializeCommand>) {
                    static_cast<void>(materialize(concrete.key));
                } else if constexpr (std::same_as<Command, UnloadCommand>) {
                    static_cast<void>(unload(concrete.key));
                } else {
                    static_cast<void>(destroy(concrete.key));
                }
            },
            command);
    }
}

}  // namespace aetheria::zone
