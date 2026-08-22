#pragma once

// local_reduction_schema.h：L3→L2 固定四列與 Site tile 私有歸約 storage。

#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>

#include "core/spatial/reduction.h"
#include "core/world/region_tiles.h"

namespace aetheria::local {
class ReductionTable;
}

namespace aetheria::site {

struct StructureIntegrityReduction {
    using Value = std::uint8_t;
};

struct ControllerReduction {
    using Value = world::FactionId;
};

struct ResourceYieldModifierReduction {
    using Value = std::uint8_t;
};

struct PassabilityCostReduction {
    using Value = std::uint16_t;
};

using LocalReductionRows =
    std::tuple<StructureIntegrityReduction, ControllerReduction,
               ResourceYieldModifierReduction, PassabilityCostReduction>;
using LocalTileDelta =
    spatial::reduction::Snapshot<LocalReductionRows, local::ReductionTable>;

class LocalReductionLayer {
public:
    explicit LocalReductionLayer(std::size_t tile_count = 0) {
        spatial::reduction::resize(storage_, tile_count);
    }

    template <typename Row>
    [[nodiscard]] std::span<const typename Row::Value> values() const noexcept {
        static_assert(spatial::reduction::RowsContain<Row, LocalReductionRows>::value);
        return std::get<spatial::reduction::Field<Row>>(storage_.fields).values;
    }

    template <typename Row>
    [[nodiscard]] typename Row::Value value(std::size_t index) const {
        return values<Row>()[index];
    }

    [[nodiscard]] bool valid_layout(std::size_t tile_count) const noexcept {
        return spatial::reduction::valid_layout(storage_, tile_count);
    }

private:
    friend class local::ReductionTable;

    spatial::reduction::Storage<LocalReductionRows> storage_;
};

static_assert(std::tuple_size_v<LocalReductionRows> == 4);
static_assert(std::is_integral_v<StructureIntegrityReduction::Value>);
static_assert(std::is_enum_v<ControllerReduction::Value>);
static_assert(std::is_integral_v<ResourceYieldModifierReduction::Value>);
static_assert(std::is_integral_v<PassabilityCostReduction::Value>);

}  // namespace aetheria::site
