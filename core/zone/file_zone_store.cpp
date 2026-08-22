#include "core/zone/file_zone_store.h"

#include "core/zone/save_manifest_io.h"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace aetheria::zone {

FileZoneStore::FileZoneStore(
    std::filesystem::path slot_directory, const rules::Ruleset& ruleset,
    worldgen::GenerationParameterHashes expected_generation_parameters)
    : slot_directory_{std::move(slot_directory)}, ruleset_{ruleset},
      expected_generation_parameters_{expected_generation_parameters} {
    const auto path = manifest_path();
    std::error_code error;
    const bool has_manifest = std::filesystem::exists(path, error);
    if (error) {
        throw std::runtime_error{"無法檢查 manifest：" + error.message()};
    }
    if (!has_manifest) {
        if (detail::has_bin_without_manifest(slot_directory_)) {
            throw std::runtime_error{"存檔目錄有 .bin 卻沒有 manifest：" +
                                     slot_directory_.string()};
        }
        return;
    }

    try {
        manifest_ = detail::decode_manifest(detail::read_file(path));
    } catch (const std::exception& exception) {
        throw std::runtime_error{"manifest 損毀：" + path.string() + "：" + exception.what()};
    }
    if (manifest_->format_version != 14 && manifest_->format_version != 15 &&
        manifest_->format_version != serialize::kSaveFormatVersion) {
        throw std::runtime_error{
            "manifest format_version 不符：檔內=" + std::to_string(manifest_->format_version) +
            " 預期=" + std::to_string(serialize::kSaveFormatVersion)};
    }
    detail::require_generation_parameters(manifest_->generation_parameters,
                                          expected_generation_parameters_, "載入 manifest");
    if (!contains(kRootZone)) {
        throw std::runtime_error{"存檔有 manifest 但缺 root.bin：" + slot_directory_.string()};
    }
}

bool FileZoneStore::contains(ZoneKey key) const {
    std::error_code error;
    return std::filesystem::is_regular_file(path_for(key), error) && !error;
}

std::unique_ptr<Zone> FileZoneStore::load(ZoneKey key) const {
    const auto path = path_for(key);
    if (!contains(key)) {
        return nullptr;
    }
    try {
        auto value = serialize::decode_zone(detail::decompress(detail::read_file(path)), ruleset_);
        if (value->key != key) {
            throw std::runtime_error{"zone key 不符：請求=" + std::to_string(value_of(key)) +
                                     " 檔內=" + std::to_string(value_of(value->key))};
        }
        return value;
    } catch (const std::exception& exception) {
        throw std::runtime_error{"zone 載入失敗：" + path.string() + "：" + exception.what()};
    }
}

void FileZoneStore::save(const Zone& zone) {
    detail::atomic_replace(path_for(zone.key), detail::compress(serialize::encode_zone(zone, ruleset_)));
}

bool FileZoneStore::erase(ZoneKey key) {
    std::error_code error;
    const bool removed = std::filesystem::remove(path_for(key), error);
    if (error) {
        throw std::runtime_error{"刪除 zone 檔失敗：" + path_for(key).string() + "：" +
                                 error.message()};
    }
    return removed;
}

std::filesystem::path FileZoneStore::path_for(ZoneKey key) const {
    if (key == kRootZone) {
        return slot_directory_ / "root.bin";
    }
    char buffer[17]{};
    const auto written = std::snprintf(buffer, sizeof(buffer), "%016llx",
                                       static_cast<unsigned long long>(value_of(key)));
    if (written != 16) {
        throw std::runtime_error{"ZoneKey hex 路徑格式化失敗"};
    }
    const std::string hex{buffer};
    const auto mixed = detail::mix_zone_key(value_of(key));
    char bucket_buffer[3]{};
    const auto bucket_written =
        std::snprintf(bucket_buffer, sizeof(bucket_buffer), "%02llx",
                      static_cast<unsigned long long>(mixed >> 56U));
    if (bucket_written != 2) {
        throw std::runtime_error{"ZoneKey 分桶路徑格式化失敗"};
    }
    return slot_directory_ / bucket_buffer / (hex + ".bin");
}

std::filesystem::path FileZoneStore::manifest_path() const {
    return slot_directory_ / "manifest.bin";
}

void FileZoneStore::write_manifest(const SaveManifest& manifest) {
    if (manifest.format_version != serialize::kSaveFormatVersion) {
        throw std::runtime_error{"拒絕寫入非目前版本的 manifest"};
    }
    if (!contains(kRootZone)) {
        throw std::runtime_error{"寫 manifest 前必須先寫 root.bin"};
    }
    detail::require_generation_parameters(manifest.generation_parameters,
                                          expected_generation_parameters_, "寫入 manifest");
    if (manifest_.has_value() && manifest.dims != manifest_->dims) {
        throw std::runtime_error{"既有存檔的 WorldDims 不可變更"};
    }
    detail::atomic_replace(manifest_path(), detail::encode_manifest(manifest));
    manifest_ = manifest;
}

}  // namespace aetheria::zone
