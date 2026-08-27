#pragma once

// character_save.h：獨立於世界 zone codec 的玩家角色檔 schema 與檔案入口。
// 本模組只接收值型別角色狀態、世界身分與路徑，不依賴 PlayableSession。

#include "core/world/region_movement.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aetheria::runtime {

inline constexpr std::uint32_t kCharacterFormatVersion = 1;

struct CharacterWorldIdentity {
    std::uint64_t world_seed{};
    std::uint64_t raws_hash{};

    constexpr bool operator==(const CharacterWorldIdentity&) const noexcept = default;
};

// residence_id 是穩定字串（region/site/local/dungeon），不保存 enum 下標。
struct CharacterState {
    world::StableId player_army_id{};
    std::string residence_id;
    std::int8_t local_z{};
    std::uint16_t local_player_x{};
    std::uint16_t local_player_y{};
    std::optional<std::uint64_t> accepted_quest_id;

    bool operator==(const CharacterState&) const = default;
};

[[nodiscard]] std::filesystem::path
character_save_path(const std::filesystem::path& slot_directory,
                    std::string_view character_name);

void write_character_save(const std::filesystem::path& path,
                          CharacterWorldIdentity identity,
                          const CharacterState& state);

[[nodiscard]] CharacterState
read_character_save(const std::filesystem::path& path,
                    CharacterWorldIdentity expected_identity);

[[nodiscard]] std::vector<std::string>
list_character_saves(const std::filesystem::path& slot_directory);

}  // namespace aetheria::runtime
