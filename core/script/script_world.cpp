// script_world.cpp：Region tile 腳本綁定、事件化 owner commit 與決定性雜湊。

#include "core/script/script_world.h"

#include "core/script/context_internal.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace aetheria::script {
namespace {

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
}

template <typename Value> void hash_integer(std::uint64_t& hash, Value value) noexcept {
    const auto bits = static_cast<std::uint64_t>(value);
    for (std::size_t byte = 0; byte < sizeof(Value); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(bits >> (byte * 8U)));
    }
}

void hash_string(std::uint64_t& hash, std::string_view value) noexcept {
    hash_integer(hash, value.size());
    for (const auto byte : value) {
        hash_byte(hash, static_cast<std::uint8_t>(byte));
    }
}

}  // namespace

void ScriptWorldState::bind_region_tile(std::string object_id, std::string definition_id,
                                        world::RegionTiles& tiles, world::RegionXY tile) {
    if (object_id.empty() || definition_id.empty()) {
        throw std::invalid_argument{"腳本物件 id 與 definition_id 不得為空"};
    }
    static_cast<void>(tiles.index_of(tile));
    const auto [position, inserted] = bindings_.try_emplace(
        std::move(object_id), Binding{std::move(definition_id), &tiles, tile});
    if (!inserted) {
        throw std::invalid_argument{"腳本物件 id 重複：" + position->first};
    }
}

bool ScriptWorldState::contains(std::string_view object_id) const noexcept {
    return bindings_.contains(object_id);
}

const ScriptWorldState::Binding&
ScriptWorldState::require_binding(std::string_view object_id) const {
    const auto found = bindings_.find(object_id);
    if (found == bindings_.end()) {
        throw std::invalid_argument{"腳本物件 id 不存在：" + std::string{object_id}};
    }
    return found->second;
}

std::int64_t ScriptWorldState::owner(std::string_view object_id) const {
    const auto& binding = require_binding(object_id);
    return static_cast<std::int64_t>(
        binding.tiles->owner.at(binding.tiles->index_of(binding.tile)));
}

std::vector<std::string> ScriptWorldState::required_definition_ids() const {
    std::vector<std::string> result;
    result.reserve(bindings_.size());
    for (const auto& [object_id, binding] : bindings_) {
        static_cast<void>(object_id);
        result.push_back(binding.definition_id);
    }
    std::ranges::sort(result);
    result.erase(std::ranges::unique(result).begin(), result.end());
    return result;
}

void ScriptWorldState::commit_owner(std::string_view object_id, world::FactionId owner_value) {
    const auto& binding = require_binding(object_id);
    auto& owner = binding.tiles->owner.at(binding.tiles->index_of(binding.tile));
    if (owner == owner_value) {
        return;
    }
    events_.push_back({std::string{object_id}, owner, owner_value});
    owner = owner_value;
}

std::uint64_t ScriptWorldState::deterministic_hash() const noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_integer(hash, bindings_.size());
    for (const auto& [object_id, binding] : bindings_) {
        hash_string(hash, object_id);
        hash_string(hash, binding.definition_id);
        hash_integer(hash, static_cast<std::uint16_t>(
                               binding.tiles->owner[binding.tiles->index_of(binding.tile)]));
    }
    hash_integer(hash, events_.size());
    for (const auto& event : events_) {
        hash_string(hash, event.object_id);
        hash_integer(hash, static_cast<std::uint16_t>(event.before));
        hash_integer(hash, static_cast<std::uint16_t>(event.after));
    }
    return hash;
}

namespace detail {

std::int64_t ContextAccess::owner(std::string_view object_id) const {
    const auto pending = pending_.find(object_id);
    return pending == pending_.end() ? world_.owner(object_id)
                                     : static_cast<std::int64_t>(pending->second);
}

void ContextAccess::set_owner(std::string_view object_id, std::int64_t owner_value) {
    static_cast<void>(world_.owner(object_id));
    if (owner_value < 0 || owner_value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::out_of_range{"owner 必須落在 FactionId 的 uint16 範圍"};
    }
    pending_.insert_or_assign(std::string{object_id}, static_cast<std::uint16_t>(owner_value));
}

std::int64_t ContextAccess::random_integer(std::int64_t minimum, std::int64_t maximum) {
    if (minimum > maximum) {
        throw std::invalid_argument{"ctx.rng:int 的最小值不得大於最大值"};
    }
    return std::uniform_int_distribution<std::int64_t>{minimum, maximum}(rng_);
}

void ContextAccess::commit() {
    for (const auto& [object_id, owner_value] : pending_) {
        world_.commit_owner(object_id, static_cast<world::FactionId>(owner_value));
    }
    pending_.clear();
}

}  // namespace detail

std::int64_t Context::owner(std::string_view object_id) const { return access_->owner(object_id); }

void Context::set_owner(std::string_view object_id, std::int64_t owner_value) {
    access_->set_owner(object_id, owner_value);
}

std::int64_t ScriptRng::integer(std::int64_t minimum, std::int64_t maximum) {
    return access_->random_integer(minimum, maximum);
}

}  // namespace aetheria::script
