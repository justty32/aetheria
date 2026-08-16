#pragma once

#include "core/serialize/zone_codec.h"
#include "core/worldgen/region_generator.h"
#include "core/zone/zone_store.h"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace aetheria::zone {

// WorldDims 是存檔建立時固定的 Region 與 Site 格網尺寸。
// SaveManifest 擁有它的值。
// manifest 作廢或被取代後，先前讀出的複本仍有效。
struct WorldDims {
    std::uint32_t region_width{128};
    std::uint32_t region_height{96};
    std::uint32_t site_width{64};
    std::uint32_t site_height{64};

    constexpr bool operator==(const WorldDims&) const noexcept = default;
};

// SaveManifest 是單一存檔槽的世界級中繼資料。
// FileZoneStore 擁有已開啟 manifest 的複本。
// FileZoneStore 析構後，呼叫端自行複製出的值仍有效。
struct SaveManifest {
    std::uint32_t format_version{serialize::kSaveFormatVersion};
    std::uint64_t next_detached_id{1};
    std::uint64_t next_entity_uid{1};
    WorldDims dims{};
    std::uint64_t world_seed{};
    worldgen::GenerationParameterHashes generation_parameters{
        worldgen::generation_parameter_hashes()};
    time::Tick now{};

    constexpr bool operator==(const SaveManifest&) const noexcept = default;
};

// FileZoneStore 是一個 zone 一檔、zstd 壓縮的單槽磁碟後端。
// 建立它的世界狀態擁有它，ZoneManager 只借用。
// 實例析構不刪檔；slot 目錄持續保有最後一次原子寫入的狀態。
class FileZoneStore final : public ZoneStore {
public:
    FileZoneStore(std::filesystem::path slot_directory, const rules::Ruleset& ruleset,
                  worldgen::GenerationParameterHashes expected_generation_parameters =
                      worldgen::generation_parameter_hashes());

    [[nodiscard]] bool contains(ZoneKey key) const override;
    [[nodiscard]] std::unique_ptr<Zone> load(ZoneKey key) const override;
    void save(const Zone& zone) override;
    [[nodiscard]] bool erase(ZoneKey key) override;

    [[nodiscard]] std::filesystem::path path_for(ZoneKey key) const;
    [[nodiscard]] std::filesystem::path manifest_path() const;
    [[nodiscard]] const std::optional<SaveManifest>& manifest() const noexcept { return manifest_; }
    void write_manifest(const SaveManifest& manifest);

private:
    std::filesystem::path slot_directory_;
    const rules::Ruleset& ruleset_;
    worldgen::GenerationParameterHashes expected_generation_parameters_;
    std::optional<SaveManifest> manifest_;
};

}  // namespace aetheria::zone
