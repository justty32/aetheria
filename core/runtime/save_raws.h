#pragma once

// 存檔基底 raws 的路徑、逐位元組複製與內容雜湊。
// 本模組只依賴檔案系統，不知道 PlayableSession、bridge 或 Godot。

#include <cstdint>
#include <filesystem>

namespace aetheria::runtime {

[[nodiscard]] std::filesystem::path
save_raws_directory(const std::filesystem::path& slot_directory);

// 依檔名排序，將檔名與逐檔內容合併成一個 FNV-1a 64-bit 雜湊。
// raws 必須存在、是目錄且至少含一個頂層 *.toml。
[[nodiscard]] std::uint64_t
raws_directory_hash(const std::filesystem::path& raws_directory);

[[nodiscard]] std::uint64_t
save_raws_hash(const std::filesystem::path& slot_directory);

// 將 source_data_directory 的全部頂層 *.toml 位元組複製到 slot/raws。
// raws 一旦存在即拒絕，避免任何差分、合併或改寫。
[[nodiscard]] std::uint64_t
copy_save_raws(const std::filesystem::path& source_data_directory,
               const std::filesystem::path& slot_directory);

void require_save_raws_hash(const std::filesystem::path& slot_directory,
                            std::uint64_t expected_hash);

}  // namespace aetheria::runtime
