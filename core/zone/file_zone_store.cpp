#include "core/zone/file_zone_store.h"

#include <cereal/archives/portable_binary.hpp>

#include <zstd.h>

#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace aetheria::zone {
namespace {

[[nodiscard]] std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if (!stream) {
        throw std::runtime_error{"無法開啟檔案：" + path.string()};
    }
    const auto end = stream.tellg();
    if (end < 0) {
        throw std::runtime_error{"無法取得檔案大小：" + path.string()};
    }
    std::string bytes(static_cast<std::size_t>(end), '\0');
    stream.seekg(0);
    if (!bytes.empty()) {
        stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream) {
        throw std::runtime_error{"無法完整讀取檔案：" + path.string()};
    }
    return bytes;
}

void atomic_replace(const std::filesystem::path& path, std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream stream{temporary, std::ios::binary | std::ios::trunc};
        if (!stream) {
            throw std::runtime_error{"無法建立暫存檔：" + temporary.string()};
        }
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        stream.flush();
        if (!stream) {
            throw std::runtime_error{"暫存檔寫入失敗：" + temporary.string()};
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        throw std::runtime_error{"原子替換失敗：" + temporary.string() + " -> " + path.string() +
                                 "：" + error.message()};
    }
}

[[nodiscard]] std::string compress(std::string_view input) {
    const auto capacity = ZSTD_compressBound(input.size());
    std::string output(capacity, '\0');
    const auto written = ZSTD_compress(output.data(), output.size(), input.data(), input.size(), 3);
    if (ZSTD_isError(written) != 0) {
        throw std::runtime_error{"zstd 壓縮失敗：" + std::string{ZSTD_getErrorName(written)}};
    }
    output.resize(written);
    return output;
}

[[nodiscard]] std::string decompress(std::string_view input) {
    const auto content_size = ZSTD_getFrameContentSize(input.data(), input.size());
    if (content_size == ZSTD_CONTENTSIZE_ERROR) {
        throw std::runtime_error{"zone 檔不是有效的 zstd frame"};
    }
    if (content_size == ZSTD_CONTENTSIZE_UNKNOWN ||
        content_size > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error{"zone 檔缺少可接受的解壓後大小"};
    }
    std::string output(static_cast<std::size_t>(content_size), '\0');
    const auto written = ZSTD_decompress(output.data(), output.size(), input.data(), input.size());
    if (ZSTD_isError(written) != 0) {
        throw std::runtime_error{"zstd 解壓失敗：" + std::string{ZSTD_getErrorName(written)}};
    }
    if (written != output.size()) {
        throw std::runtime_error{"zstd 解壓大小與 frame 宣告不符"};
    }
    return output;
}

[[nodiscard]] std::string encode_manifest(const SaveManifest& manifest) {
    std::ostringstream stream{std::ios::binary};
    cereal::PortableBinaryOutputArchive archive{
        stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
    const auto now = static_cast<std::int64_t>(manifest.now);
    archive(manifest.format_version, manifest.next_detached_id, manifest.next_entity_uid,
            manifest.dims.region_width, manifest.dims.region_height, manifest.dims.site_width,
            manifest.dims.site_height, manifest.world_seed, now);
    if (!stream) {
        throw std::runtime_error{"manifest 序列化失敗"};
    }
    return std::move(stream).str();
}

[[nodiscard]] SaveManifest decode_manifest(std::string_view bytes) {
    std::istringstream stream{std::string{bytes}, std::ios::binary};
    SaveManifest manifest;
    std::int64_t now{};
    cereal::PortableBinaryInputArchive archive{stream};
    archive(manifest.format_version, manifest.next_detached_id, manifest.next_entity_uid,
            manifest.dims.region_width, manifest.dims.region_height, manifest.dims.site_width,
            manifest.dims.site_height, manifest.world_seed, now);
    manifest.now = time::Tick{now};
    if (stream.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error{"manifest 含未解析的尾端資料"};
    }
    return manifest;
}

[[nodiscard]] bool has_bin_without_manifest(const std::filesystem::path& directory) {
    std::error_code error;
    if (!std::filesystem::exists(directory, error)) {
        if (error) {
            throw std::runtime_error{"無法檢查存檔目錄：" + error.message()};
        }
        return false;
    }
    for (std::filesystem::recursive_directory_iterator iterator{directory, error}, end;
         iterator != end; iterator.increment(error)) {
        if (error) {
            throw std::runtime_error{"無法掃描存檔目錄：" + error.message()};
        }
        if (iterator->is_regular_file() && iterator->path().extension() == ".bin") {
            return true;
        }
    }
    if (error) {
        throw std::runtime_error{"無法掃描存檔目錄：" + error.message()};
    }
    return false;
}

}  // namespace

FileZoneStore::FileZoneStore(std::filesystem::path slot_directory)
    : slot_directory_{std::move(slot_directory)} {
    const auto path = manifest_path();
    std::error_code error;
    const bool has_manifest = std::filesystem::exists(path, error);
    if (error) {
        throw std::runtime_error{"無法檢查 manifest：" + error.message()};
    }
    if (!has_manifest) {
        if (has_bin_without_manifest(slot_directory_)) {
            throw std::runtime_error{"存檔目錄有 .bin 卻沒有 manifest：" +
                                     slot_directory_.string()};
        }
        return;
    }

    try {
        manifest_ = decode_manifest(read_file(path));
    } catch (const std::exception& exception) {
        throw std::runtime_error{"manifest 損毀：" + path.string() + "：" + exception.what()};
    }
    if (manifest_->format_version != serialize::kSaveFormatVersion) {
        throw std::runtime_error{
            "manifest format_version 不符：檔內=" + std::to_string(manifest_->format_version) +
            " 預期=" + std::to_string(serialize::kSaveFormatVersion)};
    }
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
        auto value = serialize::decode_zone(decompress(read_file(path)));
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
    atomic_replace(path_for(zone.key), compress(serialize::encode_zone(zone)));
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
    return slot_directory_ / hex.substr(0, 2) / (hex + ".bin");
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
    if (manifest_.has_value() && manifest.dims != manifest_->dims) {
        throw std::runtime_error{"既有存檔的 WorldDims 不可變更"};
    }
    atomic_replace(manifest_path(), encode_manifest(manifest));
    manifest_ = manifest;
}

}  // namespace aetheria::zone
