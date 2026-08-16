#pragma once

#include "core/zone/zone.h"

#include <map>
#include <memory>
#include <optional>

namespace aetheria::zone {

// ZoneStore 是 ZoneManager 與持久化後端之間的所有權介面。
// 呼叫端擁有實作，ZoneManager 只借用它。
// 實作必須比借用它的 ZoneManager 活得久。
class ZoneStore {
public:
    virtual ~ZoneStore() = default;

    [[nodiscard]] virtual bool contains(ZoneKey key) const noexcept = 0;
    [[nodiscard]] virtual std::unique_ptr<Zone> take(ZoneKey key) = 0;
    virtual void save(std::unique_ptr<Zone> zone) = 0;
    [[nodiscard]] virtual bool erase(ZoneKey key) noexcept = 0;
};

// InMemoryZoneStore 是 M0.5 的單槽活儲存替身。
// 建立它的測試或工具擁有它。
// 實例析構時其中所有未載入 Zone 一併失效。
class InMemoryZoneStore final : public ZoneStore {
public:
    [[nodiscard]] bool contains(ZoneKey key) const noexcept override;
    [[nodiscard]] std::unique_ptr<Zone> take(ZoneKey key) override;
    void save(std::unique_ptr<Zone> zone) override;
    [[nodiscard]] bool erase(ZoneKey key) noexcept override;

    [[nodiscard]] std::optional<time::Tick> last_saved_tick(ZoneKey key) const noexcept;

private:
    std::map<ZoneKey, std::unique_ptr<Zone>> zones_;
};

}  // namespace aetheria::zone
