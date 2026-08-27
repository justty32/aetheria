#include "core/runtime/character_save.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace aetheria::runtime {
namespace {

[[nodiscard]] std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary | std::ios::ate};
    if (!stream) {
        throw std::runtime_error{"無法開啟角色檔：" + path.string()};
    }
    const auto end = stream.tellg();
    if (end < 0) {
        throw std::runtime_error{"無法取得角色檔大小：" + path.string()};
    }
    std::string bytes(static_cast<std::size_t>(end), '\0');
    stream.seekg(0);
    if (!bytes.empty()) {
        stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    if (!stream) {
        throw std::runtime_error{"無法完整讀取角色檔：" + path.string()};
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
            throw std::runtime_error{"無法建立角色暫存檔：" + temporary.string()};
        }
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        stream.flush();
        if (!stream) {
            throw std::runtime_error{"角色暫存檔寫入失敗：" + temporary.string()};
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        throw std::runtime_error{"角色檔原子替換失敗：" + temporary.string() + " -> " +
                                 path.string() + "：" + error.message()};
    }
}

}  // namespace

std::filesystem::path character_save_path(
    const std::filesystem::path& slot_directory,
    std::string_view character_name) {
    if (character_name.empty() || character_name == "." || character_name == ".." ||
        character_name.find('/') != std::string_view::npos ||
        character_name.find('\\') != std::string_view::npos ||
        character_name.find('\0') != std::string_view::npos) {
        throw std::invalid_argument{"角色名必須是非空的單一檔名"};
    }
    return slot_directory / "chars" / (std::string{character_name} + ".bin");
}

void write_character_save(const std::filesystem::path& path,
                          CharacterWorldIdentity identity,
                          const CharacterState& state) {
    std::ostringstream stream{std::ios::binary};
    {
        cereal::PortableBinaryOutputArchive archive{
            stream, cereal::PortableBinaryOutputArchive::Options::LittleEndian()};
        archive(kCharacterFormatVersion, identity.world_seed, identity.raws_hash,
                state.player_army_id.uid, state.residence_id, state.local_z,
                state.local_player_x, state.local_player_y,
                state.accepted_quest_id);
    }
    if (!stream) {
        throw std::runtime_error{"角色檔序列化失敗：" + path.string()};
    }
    atomic_replace(path, std::move(stream).str());
}

CharacterState read_character_save(const std::filesystem::path& path,
                                   CharacterWorldIdentity expected_identity) {
    try {
        std::istringstream stream{read_file(path), std::ios::binary};
        cereal::PortableBinaryInputArchive archive{stream};
        std::uint32_t version{};
        CharacterWorldIdentity identity;
        CharacterState state;
        archive(version);
        if (version != kCharacterFormatVersion) {
            throw std::runtime_error{
                "character format_version 不符：檔內=" + std::to_string(version) +
                " 預期=" + std::to_string(kCharacterFormatVersion)};
        }
        archive(identity.world_seed, identity.raws_hash,
                state.player_army_id.uid, state.residence_id, state.local_z,
                state.local_player_x, state.local_player_y,
                state.accepted_quest_id);
        if (stream.peek() != std::char_traits<char>::eof()) {
            throw std::runtime_error{"含未解析的尾端資料"};
        }
        if (identity.world_seed != expected_identity.world_seed) {
            throw std::runtime_error{
                "角色檔 world_seed 與世界不符：檔內=" +
                std::to_string(identity.world_seed) + " 世界=" +
                std::to_string(expected_identity.world_seed)};
        }
        if (identity.raws_hash != expected_identity.raws_hash) {
            throw std::runtime_error{
                "角色檔 raws_hash 與世界 manifest 不符：檔內=" +
                std::to_string(identity.raws_hash) + " 世界=" +
                std::to_string(expected_identity.raws_hash)};
        }
        return state;
    } catch (const std::exception& exception) {
        throw std::runtime_error{"角色檔載入失敗：" + path.string() + "：" +
                                 exception.what()};
    }
}

std::vector<std::string>
list_character_saves(const std::filesystem::path& slot_directory) {
    const auto directory = slot_directory / "chars";
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        if (error) {
            throw std::runtime_error{"無法檢查角色檔目錄：" + error.message()};
        }
        return {};
    }
    std::vector<std::string> result;
    for (std::filesystem::directory_iterator iterator{directory, error}, end;
         iterator != end; iterator.increment(error)) {
        if (error) {
            throw std::runtime_error{"無法掃描角色檔目錄：" + error.message()};
        }
        if (iterator->is_regular_file(error) && !error &&
            iterator->path().extension() == ".bin") {
            result.push_back(iterator->path().stem().string());
        }
    }
    if (error) {
        throw std::runtime_error{"無法掃描角色檔目錄：" + error.message()};
    }
    std::ranges::sort(result);
    return result;
}

}  // namespace aetheria::runtime
