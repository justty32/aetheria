#pragma once

// narrative_event.h：core 對顯示層輸出的結構化 i18n 事件與具名參數。
// 所有值由物件自身擁有；顯示層只能讀取，不承擔玩法狀態。

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace aetheria::narrative {

enum class ArgumentKind : std::uint8_t {
    Literal,
    I18nKey,
};

struct NamedArgument {
    std::string name;
    std::string value;
    ArgumentKind kind{ArgumentKind::Literal};

    bool operator==(const NamedArgument&) const = default;
};

struct LocalizedText {
    std::string key;
    std::vector<NamedArgument> arguments;

    bool operator==(const LocalizedText&) const = default;
};

struct NarrativeEvent {
    std::uint64_t id{};
    LocalizedText heading;
    std::vector<LocalizedText> lines;

    bool operator==(const NarrativeEvent&) const = default;
};

// EventFeed 是 core 擁有的事件唯讀快照。poll 不消耗資料，因此 UI 被 free 後可重建。
class EventFeed {
public:
    EventFeed() = default;
    explicit EventFeed(std::vector<NarrativeEvent> events) : events_{std::move(events)} {}

    [[nodiscard]] std::span<const NarrativeEvent> poll() const noexcept { return events_; }

private:
    std::vector<NarrativeEvent> events_;
};

// M6.6 只接呈現，命運三階段尚未落地；這筆假資料刻意保持與真輸出相同形狀。
[[nodiscard]] EventFeed make_fate_presentation_fixture();

}  // namespace aetheria::narrative
