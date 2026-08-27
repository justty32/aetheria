#pragma once

#include "core/serialize/zone_codec.h"
#include "core/worldgen/region_generator.h"

#include <cstdint>

namespace aetheria::zone {

// WorldDims 是存檔建立時固定的 Region 與 Site 格網尺寸。
struct WorldDims {
    std::uint32_t region_width{128};
    std::uint32_t region_height{96};
    std::uint32_t site_width{64};
    std::uint32_t site_height{64};

    constexpr bool operator==(const WorldDims&) const noexcept = default;
};

// SaveManifest 是單一存檔槽的世界級中繼資料。
struct SaveManifest {
    std::uint32_t format_version{serialize::kSaveFormatVersion};
    std::uint64_t next_detached_id{1};
    std::uint64_t next_entity_uid{1};
    WorldDims dims{};
    std::uint64_t world_seed{};
    worldgen::GenerationParameterHashes generation_parameters{
        worldgen::generation_parameter_hashes()};
    std::uint64_t raws_hash{};
    time::Tick now{};

    constexpr bool operator==(const SaveManifest&) const noexcept = default;
};

}  // namespace aetheria::zone
