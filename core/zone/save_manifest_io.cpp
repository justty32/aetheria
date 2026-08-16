#include "core/zone/save_manifest_io.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/array.hpp>

#include <zstd.h>

#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace aetheria::zone::detail {

std::string read_file(const std::filesystem::path& path) {
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

std::string compress(std::string_view input) {
    const auto capacity = ZSTD_compressBound(input.size());
    std::string output(capacity, '\0');
    const auto written = ZSTD_compress(output.data(), output.size(), input.data(), input.size(), 3);
    if (ZSTD_isError(written) != 0) {
        throw std::runtime_error{"zstd 壓縮失敗：" + std::string{ZSTD_getErrorName(written)}};
    }
    output.resize(written);
    return output;
}

std::string decompress(std::string_view input) {
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

std::string encode_manifest(const SaveManifest& manifest) {
    std::ostringstream stream{std::ios::binary};
    cereal::PortableBinaryOutputArchive archive{
        stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
    const auto now = static_cast<std::int64_t>(manifest.now);
    archive(manifest.format_version, manifest.next_detached_id, manifest.next_entity_uid,
            manifest.dims.region_width, manifest.dims.region_height, manifest.dims.site_width,
            manifest.dims.site_height, manifest.world_seed, manifest.generation_parameters.groups,
            now);
    if (!stream) {
        throw std::runtime_error{"manifest 序列化失敗"};
    }
    return std::move(stream).str();
}

SaveManifest decode_manifest(std::string_view bytes) {
    std::istringstream stream{std::string{bytes}, std::ios::binary};
    SaveManifest manifest;
    std::int64_t now{};
    cereal::PortableBinaryInputArchive archive{stream};
    archive(manifest.format_version, manifest.next_detached_id, manifest.next_entity_uid,
            manifest.dims.region_width, manifest.dims.region_height, manifest.dims.site_width,
            manifest.dims.site_height, manifest.world_seed, manifest.generation_parameters.groups,
            now);
    manifest.now = time::Tick{now};
    if (stream.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error{"manifest 含未解析的尾端資料"};
    }
    return manifest;
}

bool has_bin_without_manifest(const std::filesystem::path& directory) {
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

void require_generation_parameters(
    const worldgen::GenerationParameterHashes& actual,
    const worldgen::GenerationParameterHashes& expected, std::string_view action) {
    for (std::size_t index = 0; index < actual.groups.size(); ++index) {
        if (actual.groups[index] != expected.groups[index]) {
            throw std::runtime_error{
                std::string{action} + " generation parameters 不符：group=" +
                std::string{worldgen::generation_parameter_group_name(index)} +
                " 檔內=" + std::to_string(actual.groups[index]) +
                " 預期=" + std::to_string(expected.groups[index])};
        }
    }
}

}  // namespace aetheria::zone::detail
