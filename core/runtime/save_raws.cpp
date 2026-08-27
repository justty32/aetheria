#include "core/runtime/save_raws.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace aetheria::runtime {
namespace {

inline constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
inline constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= kFnvPrime;
}

void hash_u64(std::uint64_t& hash, std::uint64_t value) noexcept {
    for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(value & UINT8_MAX));
        value >>= 8U;
    }
}

void hash_bytes(std::uint64_t& hash, std::string_view bytes) noexcept {
    hash_u64(hash, bytes.size());
    for (const auto byte : bytes) {
        hash_byte(hash, static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
    }
}

[[nodiscard]] std::vector<std::filesystem::path>
toml_files(const std::filesystem::path& directory, std::string_view description) {
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error) || error) {
        throw std::runtime_error{std::string{description} + " 目錄不存在或不可讀：" +
                                 directory.string()};
    }
    std::vector<std::filesystem::path> files;
    for (std::filesystem::directory_iterator iterator{directory, error}, end;
         iterator != end; iterator.increment(error)) {
        if (error) {
            throw std::runtime_error{"無法掃描 " + std::string{description} + "：" +
                                     directory.string() + "：" + error.message()};
        }
        if (iterator->is_regular_file(error) && !error &&
            iterator->path().extension() == ".toml") {
            files.push_back(iterator->path());
        }
        if (error) {
            throw std::runtime_error{"無法檢查 " + std::string{description} + " 項目：" +
                                     iterator->path().string() + "：" + error.message()};
        }
    }
    if (files.empty()) {
        throw std::runtime_error{std::string{description} + " 沒有任何 *.toml：" +
                                 directory.string()};
    }
    std::ranges::sort(files, {}, [](const auto& path) { return path.filename().string(); });
    return files;
}

[[nodiscard]] std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if (!stream) {
        throw std::runtime_error{"無法開啟 raws 檔案：" + path.string()};
    }
    const auto end = stream.tellg();
    if (end < 0) {
        throw std::runtime_error{"無法取得 raws 檔案大小：" + path.string()};
    }
    std::string bytes(static_cast<std::size_t>(end), '\0');
    stream.seekg(0);
    if (!bytes.empty()) {
        stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream) {
        throw std::runtime_error{"無法完整讀取 raws 檔案：" + path.string()};
    }
    return bytes;
}

[[nodiscard]] std::uint64_t hash_directory(const std::filesystem::path& directory) {
    const auto files = toml_files(directory, "存檔 raws");
    auto hash = kFnvOffset;
    hash_u64(hash, files.size());
    for (const auto& path : files) {
        hash_bytes(hash, path.filename().string());
        hash_bytes(hash, read_bytes(path));
    }
    return hash;
}

}  // namespace

std::filesystem::path save_raws_directory(const std::filesystem::path& slot_directory) {
    return slot_directory / "raws";
}

std::uint64_t raws_directory_hash(const std::filesystem::path& raws_directory) {
    return hash_directory(raws_directory);
}

std::uint64_t save_raws_hash(const std::filesystem::path& slot_directory) {
    return raws_directory_hash(save_raws_directory(slot_directory));
}

std::uint64_t copy_save_raws(const std::filesystem::path& source_data_directory,
                             const std::filesystem::path& slot_directory) {
    const auto source_files = toml_files(source_data_directory, "基準 data");
    const auto destination = save_raws_directory(slot_directory);
    std::error_code error;
    if (std::filesystem::exists(destination, error) || error) {
        throw std::runtime_error{"存檔 raws 已存在，禁止改寫：" + destination.string()};
    }
    auto temporary = destination;
    temporary += ".tmp";
    if (std::filesystem::exists(temporary, error) || error) {
        throw std::runtime_error{"raws 暫存目錄已存在，拒絕覆寫：" + temporary.string()};
    }

    bool temporary_created{};
    try {
        std::filesystem::create_directories(slot_directory);
        if (!std::filesystem::create_directory(temporary)) {
            throw std::runtime_error{"無法建立 raws 暫存目錄：" + temporary.string()};
        }
        temporary_created = true;
        for (const auto& source : source_files) {
            std::filesystem::copy_file(source, temporary / source.filename());
        }
        const auto hash = hash_directory(temporary);
        std::filesystem::rename(temporary, destination);
        return hash;
    } catch (...) {
        if (temporary_created) {
            std::error_code ignored;
            std::filesystem::remove_all(temporary, ignored);
        }
        throw;
    }
}

void require_save_raws_hash(const std::filesystem::path& slot_directory,
                            std::uint64_t expected_hash) {
    const auto actual_hash = save_raws_hash(slot_directory);
    if (actual_hash != expected_hash) {
        throw std::runtime_error{"raws 內容雜湊不符：檔內=" +
                                 std::to_string(expected_hash) +
                                 " 當場重算=" + std::to_string(actual_hash)};
    }
}

}  // namespace aetheria::runtime
