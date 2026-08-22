#pragma once

// script_world.h：將穩定物件 id 映射到 Region owner 真值，並保存結構化變更事件。

#include "core/world/region_tiles.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace aetheria::script {

namespace detail {
class ContextAccess;
}

struct ScriptOwnerChanged {
    std::string object_id;
    world::FactionId before{};
    world::FactionId after{};

    bool operator==(const ScriptOwnerChanged&) const = default;
};

// ScriptWorldState 只保存非擁有的 Region tile 綁定；owner 真值仍在 RegionTiles。
// 呼叫端擁有 RegionTiles，且必須讓它們活得比本物件久；definition_id 用於熱重載驗證。
class ScriptWorldState {
public:
    void bind_region_tile(std::string object_id, std::string definition_id,
                          world::RegionTiles& tiles, world::RegionXY tile);

    [[nodiscard]] bool contains(std::string_view object_id) const noexcept;
    [[nodiscard]] std::int64_t owner(std::string_view object_id) const;
    [[nodiscard]] std::vector<std::string> required_definition_ids() const;
    [[nodiscard]] const std::vector<ScriptOwnerChanged>& events() const noexcept { return events_; }
    [[nodiscard]] std::uint64_t deterministic_hash() const noexcept;

private:
    friend class detail::ContextAccess;
    struct Binding {
        std::string definition_id;
        world::RegionTiles* tiles{};
        world::RegionXY tile;
    };

    [[nodiscard]] const Binding& require_binding(std::string_view object_id) const;
    void commit_owner(std::string_view object_id, world::FactionId owner);

    std::map<std::string, Binding, std::less<>> bindings_;
    std::vector<ScriptOwnerChanged> events_;
};

}  // namespace aetheria::script
