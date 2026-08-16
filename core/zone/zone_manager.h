#pragma once

#include "core/base/check.h"
#include "core/time/tick.h"
#include "core/zone/zone_store.h"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace aetheria::zone {

// ZoneHandle 是可跨 tick 保存的 zone 位址，不含 Zone 指標。
// 呼叫端擁有它，ZoneManager 每次使用時重新以 key 查找。
// 值本身永不失效；zone 可能在期間卸載，屆時 get 會回空。
class ZoneHandle {
public:
    [[nodiscard]] constexpr ZoneKey key() const noexcept { return key_; }
    constexpr bool operator==(const ZoneHandle&) const noexcept = default;

private:
    friend class ZoneManager;
    explicit constexpr ZoneHandle(ZoneKey key) noexcept : key_{key} {}
    ZoneKey key_;
};

// ZoneManager 是所有已載入 zone 的唯一生命週期入口。
// 建立它的世界狀態擁有它；它只借用 ZoneStore。
// manager 析構後 handle 仍是 key，但須交給另一個 manager 重新查找。
// 契約：tick 內的結構變更只能排進 FIFO；各 zone 以自己的 last_saved_tick 寫回單槽 store；
// 查詢 API 只回不含指標的 ZoneHandle，不把可能被逐出的 Zone* 交給呼叫端。
class ZoneManager {
public:
    explicit ZoneManager(ZoneStore& store);

    [[nodiscard]] std::optional<ZoneHandle> get(ZoneKey key) const noexcept;
    [[nodiscard]] ZoneHandle require(ZoneKey key);
    [[nodiscard]] bool load(ZoneKey key);
    [[nodiscard]] ZoneHandle materialize(ZoneKey key);
    [[nodiscard]] bool unload(ZoneKey key);
    [[nodiscard]] bool destroy(ZoneKey key);

    void queue_materialize(ZoneKey key);
    void queue_unload(ZoneKey key);
    void queue_destroy(ZoneKey key);

    template <typename System>
        requires std::invocable<System&, ZoneHandle>
    void tick(time::Tick now, System&& system) {
        AETH_CHECK(!in_tick_);
        in_tick_ = true;
        try {
            for (const auto key : tick_order_) {
                std::invoke(system, ZoneHandle{key});
            }
        } catch (...) {
            in_tick_ = false;
            commands_.clear();
            throw;
        }
        in_tick_ = false;
        current_tick_ = now;
        flush_commands();
    }

    void tick(time::Tick now) {
        tick(now, [](ZoneHandle) noexcept {});
    }

    [[nodiscard]] std::vector<ZoneKey> loaded_keys() const;
    [[nodiscard]] std::vector<ZoneKey> tick_order() const { return tick_order_; }
    [[nodiscard]] std::size_t loaded_count() const noexcept { return zones_.size(); }

private:
    struct MaterializeCommand {
        ZoneKey key;
    };
    struct UnloadCommand {
        ZoneKey key;
    };
    struct DestroyCommand {
        ZoneKey key;
    };
    using ZoneCommand = std::variant<MaterializeCommand, UnloadCommand, DestroyCommand>;

    void add_loaded(std::unique_ptr<Zone> zone);
    void remove_from_tick_order(ZoneKey key);
    void flush_commands();

    ZoneStore& store_;
    std::map<ZoneKey, std::unique_ptr<Zone>> zones_;
    std::vector<ZoneKey> tick_order_;
    std::vector<ZoneCommand> commands_;
    time::Tick current_tick_{};
    bool in_tick_{};
};

}  // namespace aetheria::zone
