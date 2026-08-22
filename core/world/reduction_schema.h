#pragma once

// reduction_schema.h 定義 L2→L1 的固定 row 清單；機制與 L3→L2 共用。

#include <cstdint>
#include <tuple>
#include <type_traits>

#include "core/spatial/reduction.h"

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

struct OrderReduction {
    using Value = std::uint16_t;
};

// 加一種連續量時把 row 加在尾端；table 與 Region storage 都由這份清單展開。
using RegionReductionRows =
    std::tuple<PopulationReduction, DevelopmentLevelReduction, FoodStockReduction,
               ProductionStockReduction, OrderReduction>;

template <typename Row> using ReductionField = spatial::reduction::Field<Row>;
template <typename Row> using ReductionValue = spatial::reduction::SnapshotValue<Row>;

template <typename Row>
inline constexpr bool kIsRegionReductionRow =
    spatial::reduction::RowsContain<Row, RegionReductionRows>::value;

using RegionReductionStorage = spatial::reduction::Storage<RegionReductionRows>;

// delta 只能由 Site ReductionTable 建立；空列的共用語意是「不寫父層」。
using RegionTileDelta =
    spatial::reduction::Snapshot<RegionReductionRows, site::ReductionTable>;

static_assert(std::is_integral_v<PopulationReduction::Value>);
static_assert(std::is_integral_v<DevelopmentLevelReduction::Value>);
static_assert(std::is_integral_v<FoodStockReduction::Value>);
static_assert(std::is_integral_v<ProductionStockReduction::Value>);
static_assert(std::is_integral_v<OrderReduction::Value>);

}  // namespace aetheria::world
