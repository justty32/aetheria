// playable_session_save.cpp：世界存檔。

#include "core/runtime/playable_session.h"
#include "core/runtime/save_raws.h"
#include "core/runtime/turn_commit.h"

#include "core/zone/file_zone_store.h"


#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>


namespace aetheria::runtime {

void PlayableSession::save_game(zone::FileZoneStore& destination,
                                std::string_view character_name) {
    if (encounter_tile_.has_value()) {
        throw std::logic_error{"遭遇尚未處理：只能在回合尾端、無 pending 遭遇時存檔"};
    }
    const auto character_path =
        character_save_path(destination.slot_directory(), character_name);
    const auto source_hash = raws_directory_hash(base_raws_directory_);
    const auto destination_raws = save_raws_directory(destination.slot_directory());
    std::error_code error;
    const bool destination_has_raws = std::filesystem::exists(destination_raws, error);
    if (error) {
        throw std::runtime_error{"無法檢查目的存檔 raws：" + error.message()};
    }
    std::uint64_t destination_hash{};
    if (!destination_has_raws) {
        destination_hash = copy_save_raws(base_raws_directory_, destination.slot_directory());
    } else {
        destination_hash = save_raws_hash(destination.slot_directory());
    }
    if (source_hash != destination_hash) {
        throw std::runtime_error{"目的存檔 raws 與目前世界的基底 raws 不符，拒絕改寫"};
    }
    character_save_path_ = character_path;
    turn_commit_->attach(destination.slot_directory());
    turn_commit_->commit_world(store_, destination, *manager_, seed_,
                               destination_hash, now(), [&] {
                                   write_character_save(
                                       character_path,
                                       {seed_, destination_hash},
                                       export_character_state());
                               });
}

} // namespace aetheria::runtime
