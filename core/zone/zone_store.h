#pragma once

#include "core/zone/zone.h"

#include <map>
#include <memory>
#include <string>

namespace aetheria::zone {

// ZoneStore 是 ZoneManager 與持久化後端之間的快照介面。
// 呼叫端擁有實作，ZoneManager 只借用它。
// load 回獨立 Zone；後端保留快照，且必須比借用它的 ZoneManager 活得久。
class ZoneStore {
public:
    virtual ~ZoneStore() = default;

    [[nodiscard]] virtual bool contains(ZoneKey key) const = 0;
    [[nodiscard]] virtual std::unique_ptr<Zone> load(ZoneKey key) const = 0;
    virtual void save(const Zone& zone) = 0;
    [[nodiscard]] virtual bool erase(ZoneKey key) = 0;
};

// InMemoryZoneStore 是測試與工具使用的單槽快照替身。
// 建立它的測試或工具擁有它。
// 實例析構時其中所有 canonical snapshot 一併失效。
class InMemoryZoneStore final : public ZoneStore {
public:
    [[nodiscard]] bool contains(ZoneKey key) const override;
    [[nodiscard]] std::unique_ptr<Zone> load(ZoneKey key) const override;
    void save(const Zone& zone) override;
    [[nodiscard]] bool erase(ZoneKey key) override;

private:
    std::map<ZoneKey, std::string> snapshots_;
};

}  // namespace aetheria::zone
