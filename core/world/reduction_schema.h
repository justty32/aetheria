#pragma once

// reduction_schema.h 定義 L2→L1 歸約量表的 row 型別與封閉 delta。

#include <cstdint>
#include <tuple>
#include <type_traits>
#include <vector>

namespace aetheria::site {
class ReductionTable;
}

namespace aetheria::world {

struct PopulationReduction {
    using Value = std::uint32_t;
};

struct DevelopmentLevelReduction {
    using Value = std::uint16_t;
};

struct FoodStockReduction {
    using Value = std::uint64_t;
};

struct ProductionStockReduction {
    using Value = std::uint64_t;
};

// 加一種連續量時把 row 加在尾端；table 與 Region storage 都由這份清單展開。
using RegionReductionRows =
    std::tuple<PopulationReduction, DevelopmentLevelReduction, FoodStockReduction,
               ProductionStockReduction>;

template <typename Row> struct ReductionField {
    using RowType = Row;
    std::vector<typename Row::Value> values;

    template <typename Archive> void serialize(Archive& archive) { archive(values); }
};

template <typename Row> struct ReductionValue {
    using RowType = Row;
    typename Row::Value value{};
};

template <typename Row, typename Rows> struct ReductionRowsContain;

template <typename Row, typename... Rows>
struct ReductionRowsContain<Row, std::tuple<Rows...>>
    : std::disjunction<std::is_same<Row, Rows>...> {};

template <typename Row>
inline constexpr bool kIsRegionReductionRow =
    ReductionRowsContain<Row, RegionReductionRows>::value;

template <typename Rows> struct ReductionStorageFor;

template <typename... Rows> struct ReductionStorageFor<std::tuple<Rows...>> {
    using Fields = std::tuple<ReductionField<Rows>...>;
    using Values = std::tuple<ReductionValue<Rows>...>;
};

struct RegionReductionStorage {
    typename ReductionStorageFor<RegionReductionRows>::Fields fields;
};

// RegionTileDelta 只能由固定量表建立；呼叫端只能讀，不能自行拼出可套用的 delta。
class RegionTileDelta {
public:
    template <typename Row> [[nodiscard]] typename Row::Value value() const noexcept {
        static_assert(kIsRegionReductionRow<Row>);
        return std::get<ReductionValue<Row>>(values_).value;
    }

private:
    friend class site::ReductionTable;

    RegionTileDelta() = default;

    typename ReductionStorageFor<RegionReductionRows>::Values values_;
};

static_assert(std::is_integral_v<PopulationReduction::Value>);
static_assert(std::is_integral_v<DevelopmentLevelReduction::Value>);
static_assert(std::is_integral_v<FoodStockReduction::Value>);
static_assert(std::is_integral_v<ProductionStockReduction::Value>);

}  // namespace aetheria::world
