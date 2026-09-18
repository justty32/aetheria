// local_generation_content.cpp：地表紋理、植被與資源點散佈。

#include "core/local/local_tiles.h"

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

#include "core/local/local_generation_detail.h"

namespace aetheria::local::detail::generation {
namespace {

constexpr std::uint64_t kGroundTextureSalt = UINT64_C(0x8E25C19A74D603BF);
constexpr std::uint64_t kScatterSalt = UINT64_C(0x31D8A6C7E50B924F);
constexpr std::uint64_t kObjectSalt = UINT64_C(0xB04E298DC6137A5F);

}  // namespace

void texture_ground(OpenLocalSkeleton& result, std::uint64_t local_seed,
                    const rules::Ruleset& ruleset) {
    const auto mud = ruleset.find_ground("ground.mud");
    const auto stone = ruleset.find_ground("ground.stone");
    if (!mud.has_value() || !stone.has_value()) {
        return;
    }
    constexpr std::uint16_t kPatchExtent = 4;
    for (std::uint16_t y = 0; y < kLocalHeight; ++y) {
        for (std::uint16_t x = 0; x < kLocalWidth; ++x) {
            const auto index = tile_index(x, y);
            const auto* base = ruleset.ground(result.tiles.ground[index]);
            if (base == nullptr || (base->flags & rules::kGroundWaterFlag) != 0) {
                continue;
            }
            const auto patch = static_cast<std::uint64_t>(y / kPatchExtent) *
                                   (kLocalWidth / kPatchExtent) +
                               x / kPatchExtent;
            const auto patch_noise =
                worldgen::splitmix64(local_seed ^ kGroundTextureSalt ^ patch);
            const auto fine_noise = worldgen::splitmix64(patch_noise ^ index);
            if (patch_noise % 100U >= 58U || fine_noise % 100U >= 46U) {
                continue;
            }
            result.tiles.ground[index] = (fine_noise >> 8U) % 5U == 0 ? *stone : *mud;
        }
    }
}

void scatter(OpenLocalSkeleton& result, const LocalSlowVars& slow, std::uint64_t local_seed,
             const rules::Ruleset& ruleset) {
    const auto& config = ruleset.wilderness_generation_rules();
    const auto* feature = ruleset.feature(slow.feature);
    const bool forest = feature != nullptr && (feature->flags & rules::kFeatureForestFlag) != 0;
    const auto density = forest ? config.forest_vegetation_percent
                                : config.sparse_vegetation_percent;
    const auto extent = config.jitter_cell_extent;
    for (std::uint16_t cell_y = 0; cell_y < kLocalHeight; cell_y += extent) {
        for (std::uint16_t cell_x = 0; cell_x < kLocalWidth; cell_x += extent) {
            const auto cell = static_cast<std::uint64_t>(cell_y) * kLocalWidth + cell_x;
            const auto seed = worldgen::splitmix64(local_seed ^ kScatterSalt ^ cell);
            if (seed % 100U >= density) {
                continue;
            }
            const auto width = std::min<std::uint16_t>(extent, kLocalWidth - cell_x);
            const auto height = std::min<std::uint16_t>(extent, kLocalHeight - cell_y);
            const auto x = static_cast<std::uint16_t>(cell_x + seed % width);
            const auto y = static_cast<std::uint16_t>(cell_y + (seed >> 16U) % height);
            const auto index = tile_index(x, y);
            const auto* ground = ruleset.ground(result.tiles.ground[index]);
            if (ground == nullptr || (ground->flags & rules::kGroundWaterFlag) != 0 ||
                result.tiles.overlay[index] != OverlayId::None) {
                continue;
            }
            result.tiles.overlay[index] =
                (seed & 3U) == 0 ? OverlayId::Stone : OverlayId::Vegetation;
            ++result.scatter_count;
        }
    }
}

void place_objects(OpenLocalSkeleton& result, const LocalSlowVars& slow,
                   std::uint64_t local_seed, const rules::Ruleset& ruleset) {
    const auto* feature = ruleset.feature(slow.feature);
    const bool mine = feature != nullptr && (feature->flags & rules::kFeatureMineFlag) != 0;
    const auto count = mine ? ruleset.wilderness_generation_rules().mine_resource_points
                            : ruleset.wilderness_generation_rules().base_resource_points;
    const auto start = static_cast<std::size_t>(
        worldgen::splitmix64(local_seed ^ kObjectSalt) % kLocalTileCount);
    for (std::size_t offset = 0;
         offset < kLocalTileCount && result.object_count < count; ++offset) {
        const auto index = (start + offset * 977U) % kLocalTileCount;
        const auto* ground = ruleset.ground(result.tiles.ground[index]);
        if (ground == nullptr || (ground->flags & rules::kGroundWaterFlag) != 0 ||
            result.tiles.overlay[index] != OverlayId::None) {
            continue;
        }
        result.tiles.overlay[index] = OverlayId::ScatteredObject;
        ++result.object_count;
    }
}

}  // namespace aetheria::local::detail::generation
