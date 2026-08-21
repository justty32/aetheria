#pragma once

#include "core/base/check.h"
#include "core/rules/ruleset.h"
#include "core/world/reduction_schema.h"
#include "core/zone/lod_level.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace aetheria::zone {
struct Zone;
}

namespace aetheria::serialize {
[[nodiscard]] std::string encode_zone(const zone::Zone& zone,
                                      const rules::Ruleset& ruleset);
[[nodiscard]] std::unique_ptr<zone::Zone> decode_zone(std::string_view bytes,
                                                      const rules::Ruleset& ruleset);
}

namespace aetheria::site {
class ReductionTable;
}

namespace aetheria::world {

class RegionSimulation;

// FactionId 是勢力資料表的強型別下標，0 代表無主。
// 世界狀態配發其值，RegionTiles 只保存複本。
// 所屬世界狀態存在期間有效。
enum class FactionId : std::uint16_t {};

// SettlementTier 是 Region tile 上城市選址的持久三級結果。
// RegionTiles 擁有每格的值複本，Site 生成只讀取它。
// 值本身永不失效；None 代表該格沒有聚落。
enum class SettlementTier : std::uint8_t {
    None,
    Village,
    Town,
    City,
};

// RegionXY 是 L1 Region 內的非環繞方格座標。
// 呼叫端擁有值。
// 值本身永不失效。
struct RegionXY {
    std::int16_t x{};
    std::int16_t y{};

    constexpr auto operator<=>(const RegionXY&) const noexcept = default;
};

// RegionPortal 是 Region 中稀疏的 WorldGraph 出境點綁定。
// RegionTiles 擁有所有實例；channel 指向手工 WorldGraph 通道識別。
// 同一 channel 與同一 tile 在單一 Region 都只能出現一次。
struct RegionPortal {
    RegionXY tile;
    rules::WorldConnectionId channel{};

    constexpr bool operator==(const RegionPortal&) const noexcept = default;
};

// SiteState 是 Region tile 對其下層 Site 的具現化狀態。
// RegionTiles 擁有所有實例。
// 所屬 RegionTiles 析構或重配後失效；lod 不進存檔。
struct SiteState {
    zone::LodLevel lod{zone::LodLevel::Absent};
    bool has_live_site{};
    bool ever_realized{};

    constexpr bool operator==(const SiteState&) const noexcept = default;
};

// RegionTiles 是 L1 地圖的平行陣列資料與雙邊一致 edge 寫入入口。
// 所屬 RegionPayload 的 layers map 擁有它。
// payload 被替換或 Zone 析構後失效；任何 vector 重配會使先前元素參考失效。
struct RegionTiles {
    RegionTiles() = default;
    RegionTiles(std::uint32_t grid_width, std::uint32_t grid_height);

    [[nodiscard]] std::size_t tile_count() const noexcept;
    [[nodiscard]] std::size_t index_of(RegionXY coordinate) const;
    [[nodiscard]] bool valid_layout() const noexcept;
    void set_edge(RegionXY a, RegionXY b, rules::EdgeId edge_id);
    [[nodiscard]] rules::EdgeId edge_between(RegionXY a, RegionXY b) const;
    [[nodiscard]] std::size_t edge_storage_bytes() const noexcept;
    [[nodiscard]] std::size_t dynamic_storage_bytes() const noexcept;

    template <typename Row>
    [[nodiscard]] std::span<const typename Row::Value> reduction_values() const noexcept {
        static_assert(kIsRegionReductionRow<Row>);
        return std::get<ReductionField<Row>>(reduction_fields_.fields).values;
    }

    template <typename Row>
    [[nodiscard]] typename Row::Value reduction_value(RegionXY coordinate) const {
        return reduction_values<Row>()[index_of(coordinate)];
    }

    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<rules::TerrainId> base;
    std::vector<rules::ReliefId> relief;
    std::vector<rules::FeatureId> feature;
    std::vector<std::uint8_t> temperature;
    std::vector<std::uint8_t> moisture;
    std::vector<std::uint16_t> elevation;
    std::vector<rules::EdgeId> edges;
    std::vector<FactionId> owner;
    std::vector<SettlementTier> settlement;
    std::vector<SiteState> site;
    std::vector<RegionPortal> portals;

private:
    friend class site::ReductionTable;
    friend class RegionSimulation;
    friend std::string serialize::encode_zone(const zone::Zone&, const rules::Ruleset&);
    friend std::unique_ptr<zone::Zone> serialize::decode_zone(std::string_view,
                                                              const rules::Ruleset&);

    RegionReductionStorage reduction_fields_;
};

namespace detail {

template <typename Value> struct IsIntegerWorldStateVector : std::false_type {};

template <typename Value, typename Allocator>
struct IsIntegerWorldStateVector<std::vector<Value, Allocator>>
    : std::bool_constant<std::is_integral_v<Value> || std::is_enum_v<Value> ||
                         std::is_same_v<Value, SiteState>> {};

template <typename Allocator>
struct IsIntegerWorldStateVector<std::vector<RegionPortal, Allocator>> : std::true_type {};

}  // namespace detail

static_assert(std::is_integral_v<decltype(RegionTiles::width)>);
static_assert(std::is_integral_v<decltype(RegionTiles::height)>);
static_assert(detail::IsIntegerWorldStateVector<decltype(RegionTiles::base)>::value);
static_assert(detail::IsIntegerWorldStateVector<decltype(RegionTiles::relief)>::value);
static_assert(detail::IsIntegerWorldStateVector<decltype(RegionTiles::feature)>::value);
static_assert(detail::IsIntegerWorldStateVector<decltype(RegionTiles::temperature)>::value);
static_assert(detail::IsIntegerWorldStateVector<decltype(RegionTiles::moisture)>::value);
static_assert(detail::IsIntegerWorldStateVector<decltype(RegionTiles::elevation)>::value);
static_assert(detail::IsIntegerWorldStateVector<decltype(RegionTiles::edges)>::value);
static_assert(detail::IsIntegerWorldStateVector<decltype(RegionTiles::owner)>::value);
static_assert(detail::IsIntegerWorldStateVector<decltype(RegionTiles::settlement)>::value);
static_assert(detail::IsIntegerWorldStateVector<decltype(RegionTiles::site)>::value);
static_assert(detail::IsIntegerWorldStateVector<decltype(RegionTiles::portals)>::value);

inline constexpr std::size_t kDeclaredRegionTilesStorageSize =
    sizeof(decltype(RegionTiles::width)) + sizeof(decltype(RegionTiles::height)) +
    sizeof(decltype(RegionTiles::base)) + sizeof(decltype(RegionTiles::relief)) +
    sizeof(decltype(RegionTiles::feature)) + sizeof(decltype(RegionTiles::temperature)) +
    sizeof(decltype(RegionTiles::moisture)) + sizeof(decltype(RegionTiles::elevation)) +
    sizeof(decltype(RegionTiles::edges)) + sizeof(decltype(RegionTiles::owner)) +
    sizeof(decltype(RegionTiles::settlement)) + sizeof(decltype(RegionTiles::site)) +
    sizeof(decltype(RegionTiles::portals)) + sizeof(RegionReductionStorage);
static_assert(sizeof(RegionTiles) == kDeclaredRegionTilesStorageSize,
              "新增 RegionTiles 世界狀態欄位時必須登記並驗證其為整數或 enum");

}  // namespace aetheria::world
