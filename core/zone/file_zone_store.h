#pragma once

#include "core/zone/zone_store.h"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace aetheria::zone {

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
    [[nodiscard]] std::vector<ZoneKey> stored_keys() const override;

    [[nodiscard]] std::filesystem::path path_for(ZoneKey key) const;
    [[nodiscard]] std::filesystem::path manifest_path() const;
    [[nodiscard]] const std::filesystem::path& slot_directory() const noexcept {
        return slot_directory_;
    }
    [[nodiscard]] const std::optional<SaveManifest>& manifest() const noexcept override {
        return manifest_;
    }
    void write_manifest(const SaveManifest& manifest) override;

private:
    std::filesystem::path slot_directory_;
    const rules::Ruleset& ruleset_;
    worldgen::GenerationParameterHashes expected_generation_parameters_;
    std::optional<SaveManifest> manifest_;
};

}  // namespace aetheria::zone
