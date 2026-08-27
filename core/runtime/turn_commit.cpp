#include "core/runtime/turn_commit.h"

#include "core/runtime/playable_session.h"
#include "core/runtime/session_persistence.h"
#include "core/zone/zone_manager.h"
#include "core/zone/zone_store.h"

#include <array>
#include <charconv>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

namespace aetheria::runtime {
namespace {

[[nodiscard]] std::uint64_t parse_u64(std::string_view payload,
                                      std::string_view key) {
    const auto prefix = std::string{key} + " = ";
    const auto position = payload.find(prefix);
    if (position == std::string_view::npos) {
        throw std::runtime_error{"history payload 缺欄位：" + std::string{key}};
    }
    const auto begin = payload.data() + position + prefix.size();
    const auto line_end = payload.find('\n', position + prefix.size());
    const auto end = line_end == std::string_view::npos
                         ? payload.data() + payload.size()
                         : payload.data() + line_end;
    std::uint64_t value{};
    const auto [parsed_end, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || parsed_end != end) {
        throw std::runtime_error{"history payload 整數無效：" + std::string{key}};
    }
    return value;
}

[[nodiscard]] std::int64_t parse_i64(std::string_view payload,
                                     std::string_view key) {
    const auto prefix = std::string{key} + " = ";
    const auto position = payload.find(prefix);
    if (position == std::string_view::npos) {
        throw std::runtime_error{"history payload 缺欄位：" + std::string{key}};
    }
    const auto begin = payload.data() + position + prefix.size();
    const auto line_end = payload.find('\n', position + prefix.size());
    const auto end = line_end == std::string_view::npos
                         ? payload.data() + payload.size()
                         : payload.data() + line_end;
    std::int64_t value{};
    const auto [parsed_end, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || parsed_end != end) {
        throw std::runtime_error{"history payload 整數無效：" + std::string{key}};
    }
    return value;
}

[[nodiscard]] std::string parse_string(std::string_view payload,
                                       std::string_view key) {
    const auto prefix = std::string{key} + " = \"";
    const auto position = payload.find(prefix);
    if (position == std::string_view::npos) {
        throw std::runtime_error{"history payload 缺欄位：" + std::string{key}};
    }
    const auto value_begin = position + prefix.size();
    const auto value_end = payload.find('"', value_begin);
    if (value_end == std::string_view::npos) {
        throw std::runtime_error{"history payload 字串無效：" + std::string{key}};
    }
    return std::string{payload.substr(value_begin, value_end - value_begin)};
}

[[nodiscard]] std::string marker_bytes(std::uint64_t value) {
    std::string bytes(sizeof(value), '\0');
    for (auto& byte : bytes) {
        byte = static_cast<char>(value & UINT8_MAX);
        value >>= 8U;
    }
    return bytes;
}

[[nodiscard]] std::uint64_t decode_marker(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    std::array<unsigned char, sizeof(std::uint64_t)> bytes{};
    stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    if (!stream || stream.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error{"history.commit 不是單一 u64：" + path.string()};
    }
    std::uint64_t value{};
    for (std::size_t index = bytes.size(); index-- > 0;) {
        value <<= 8U;
        value |= bytes[index];
    }
    return value;
}

void atomic_write(const std::filesystem::path& path, std::string_view bytes) {
    std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream stream{temporary, std::ios::binary | std::ios::trunc};
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        stream.flush();
        if (!stream) {
            throw std::runtime_error{"history.commit 暫存寫入失敗"};
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        throw std::runtime_error{"history.commit 原子替換失敗：" + error.message()};
    }
}

}  // namespace

TurnCommit::TurnCommit(std::uint64_t genesis_hash) : history_{genesis_hash} {}

TurnCommit::TurnCommit(std::filesystem::path slot_directory,
                       std::uint64_t genesis_hash)
    : history_{log_path(slot_directory), genesis_hash},
      slot_directory_{std::move(slot_directory)} {
    load_marker();
}

const history::HistoryEntry& TurnCommit::record(time::Tick tick,
                                                std::string_view kind,
                                                std::string_view payload) {
    const auto& entry = history_.append(tick, kind, payload);
    if (!slot_directory_.empty()) {
        ensure_marker();
    }
    if (interrupt_after_journal_) {
        throw std::runtime_error{"M10.3a 測試鉤：日誌與 marker 已落，套用未跑"};
    }
    return entry;
}

void TurnCommit::attach(const std::filesystem::path& slot_directory) {
    if (!slot_directory_.empty() &&
        slot_directory_.lexically_normal() != slot_directory.lexically_normal()) {
        throw std::logic_error{"turn commit 已綁定另一個世界槽"};
    }
    history_.bind(log_path(slot_directory));
    slot_directory_ = slot_directory;
    load_marker();
    ensure_marker();
}

void TurnCommit::replay_tail(PlayableSession& session) const {
    history_.replay_after(committed_seq_,
                          [&](const auto& entry) { apply_entry(session, entry); });
}

void TurnCommit::replay_all(PlayableSession& session) const {
    history_.replay_after(0U,
                          [&](const auto& entry) { apply_entry(session, entry); });
}

void TurnCommit::commit_world(zone::ZoneStore& active_store,
                              zone::ZoneStore& destination,
                              zone::ZoneManager& manager,
                              std::uint64_t world_seed,
                              std::uint64_t raws_hash, time::Tick now) {
    ensure_marker();
    save_session(active_store, destination, manager, world_seed, raws_hash, now);
    write_marker(history_.head_seq());
}

std::filesystem::path TurnCommit::log_path(
    const std::filesystem::path& slot_directory) {
    return slot_directory / "history.log";
}

std::filesystem::path TurnCommit::marker_path(
    const std::filesystem::path& slot_directory) {
    return slot_directory / "history.commit";
}

void TurnCommit::load_marker() {
    if (slot_directory_.empty()) {
        committed_seq_ = 0;
        return;
    }
    std::error_code error;
    const auto path = marker_path(slot_directory_);
    if (!std::filesystem::exists(path, error)) {
        if (error) {
            throw std::runtime_error{"無法檢查 history.commit：" + error.message()};
        }
        committed_seq_ = 0;
        return;
    }
    committed_seq_ = decode_marker(path);
    if (committed_seq_ > history_.head_seq()) {
        throw std::runtime_error{"history.commit 超過日誌鏈頭：marker=" +
                                 std::to_string(committed_seq_) + " head=" +
                                 std::to_string(history_.head_seq())};
    }
}

void TurnCommit::ensure_marker() {
    if (slot_directory_.empty()) {
        throw std::logic_error{"turn commit 尚未綁定世界槽"};
    }
    const auto path = marker_path(slot_directory_);
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        if (error) {
            throw std::runtime_error{"無法檢查 history.commit：" + error.message()};
        }
        write_marker(committed_seq_);
    }
}

void TurnCommit::write_marker(std::uint64_t value) {
    atomic_write(marker_path(slot_directory_), marker_bytes(value));
    committed_seq_ = value;
}

void TurnCommit::apply_entry(PlayableSession& session,
                             const history::HistoryEntry& entry) const {
    if (entry.payload.find("context_residence = ") != std::string::npos) {
        const auto residence = parse_string(entry.payload, "context_residence");
        if (residence == "region") {
            session.residence_ = PlayableResidence::Region;
        } else if (residence == "site") {
            session.residence_ = PlayableResidence::Site;
        } else if (residence == "local") {
            session.residence_ = PlayableResidence::Local;
        } else if (residence == "dungeon") {
            session.residence_ = PlayableResidence::Dungeon;
        } else {
            throw std::runtime_error{"history context residence 無效：" + residence};
        }
        const auto local_z = parse_i64(entry.payload, "context_local_z");
        const auto player_x = parse_u64(entry.payload, "context_player_x");
        const auto player_y = parse_u64(entry.payload, "context_player_y");
        const auto army = parse_u64(entry.payload, "context_player_army");
        const auto has_accepted =
            parse_u64(entry.payload, "context_has_accepted_quest");
        const auto accepted = parse_u64(entry.payload, "context_accepted_quest");
        if (local_z < std::numeric_limits<std::int8_t>::min() ||
            local_z > std::numeric_limits<std::int8_t>::max() ||
            player_x > std::numeric_limits<std::uint16_t>::max() ||
            player_y > std::numeric_limits<std::uint16_t>::max() ||
            has_accepted > 1U) {
            throw std::runtime_error{"history character context 超出範圍"};
        }
        session.local_z_ = static_cast<std::int8_t>(local_z);
        session.local_player_x_ = static_cast<std::uint16_t>(player_x);
        session.local_player_y_ = static_cast<std::uint16_t>(player_y);
        session.player_army_id_ = world::StableId{army};
        session.accepted_quest_id_ = has_accepted == 0U
                                         ? std::nullopt
                                         : std::optional<std::uint64_t>{accepted};
    }
    session.replaying_history_ = true;
    session.current_command_seq_ = entry.seq;
    try {
        if (entry.kind == "issue_move") {
            const auto unit = parse_u64(entry.payload, "unit");
            const auto x = parse_i64(entry.payload, "target_x");
            const auto y = parse_i64(entry.payload, "target_y");
            if (x < std::numeric_limits<std::int16_t>::min() ||
                x > std::numeric_limits<std::int16_t>::max() ||
                y < std::numeric_limits<std::int16_t>::min() ||
                y > std::numeric_limits<std::int16_t>::max()) {
                throw std::runtime_error{"history issue_move 座標超出 int16"};
            }
            session.issue_move(world::StableId{unit},
                               {static_cast<std::int16_t>(x),
                                static_cast<std::int16_t>(y)});
        } else if (entry.kind == "advance_xun") {
            static_cast<void>(session.advance_xun());
        } else if (entry.kind == "resolve_encounter") {
            const auto choice = parse_string(entry.payload, "choice");
            static_cast<void>(session.resolve_encounter(
                choice == "manual" ? PlayableBattleChoice::CommandSite
                                     : PlayableBattleChoice::AutoRegion));
        } else if (entry.kind == "enter_site_manual") {
            session.enter_site();
        } else if (entry.kind == "enter_site_auto") {
            session.manage_city();
        } else if (entry.kind == "leave_site") {
            session.leave_site();
        } else if (entry.kind == "build_city") {
            session.build_city();
        } else if (entry.kind == "accept_bandit") {
            session.accept_bandit_quest();
        } else if (entry.kind == "enter_local_manual") {
            session.enter_local();
        } else if (entry.kind == "enter_local_auto") {
            session.manage_local();
        } else if (entry.kind == "leave_local") {
            session.leave_local();
        } else if (entry.kind == "open_door") {
            session.open_door_and_move();
        } else if (entry.kind == "suppress_bandits") {
            session.suppress_bandits();
        } else if (entry.kind == "enter_dungeon_manual") {
            session.enter_dungeon();
        } else if (entry.kind == "enter_dungeon_auto") {
            session.manage_dungeon();
        } else if (entry.kind == "leave_dungeon") {
            session.leave_dungeon();
        } else if (entry.kind == "descend_dungeon") {
            session.descend_dungeon();
        } else if (entry.kind == "clear_dungeon") {
            session.clear_dungeon();
        } else if (entry.kind == "sign_treaty") {
            session.sign_peace_treaty();
        } else if (entry.kind == "measure_roundtrips") {
            session.measure_site_roundtrips();
        } else if (entry.kind == "measure_calibration") {
            session.measure_city_management(
                static_cast<std::uint32_t>(parse_u64(entry.payload, "samples")));
        } else {
            throw std::runtime_error{"history kind 不支援重放：" + entry.kind};
        }
    } catch (...) {
        session.replaying_history_ = false;
        throw;
    }
    session.replaying_history_ = false;
}

}  // namespace aetheria::runtime
