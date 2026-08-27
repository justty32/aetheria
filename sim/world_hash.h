#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace aetheria::sim {

// WorldStateHashReport 是驗證工具單次磁碟走訪的結果，不屬於執行期世界狀態。
// 呼叫端擁有回傳值；zone_count 包含 root.bin。
struct WorldStateHashReport {
    std::uint64_t hash{};
    std::size_t zone_count{};
    std::uint64_t zone_hash{};
    std::uint64_t raws_hash{};
    std::uint64_t history_head_hash{};
    std::uint64_t history_head_seq{};
};

// 世界身分固定依 zone -> raws -> history head 的順序餵進同一條 FNV-1a。
[[nodiscard]] std::uint64_t compose_world_identity(
    std::uint64_t zone_hash, std::uint64_t raws_hash,
    std::uint64_t history_head_hash) noexcept;

// world_state_hash 直接列舉 slot_directory 底下的 zone 檔，依 ZoneKey 排序後合併。
// 它只供 aetheria_sim 與測試使用；不常駐、不寫回存檔，也不進玩法路徑。
[[nodiscard]] WorldStateHashReport
world_state_hash(const std::filesystem::path& slot_directory);

int run_world_hash(const std::filesystem::path& slot_directory);
int run_replay(const std::filesystem::path& slot_directory);

}  // namespace aetheria::sim
