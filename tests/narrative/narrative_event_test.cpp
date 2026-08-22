#include "core/narrative/narrative_event.h"

#include <algorithm>
#include <string_view>

#include <gtest/gtest.h>

namespace {

TEST(NarrativeEvent, FateFixtureSeparatesAggregateNumberFromNamedStories) {
    const auto feed = aetheria::narrative::make_fate_presentation_fixture();
    const auto events = feed.poll();
    ASSERT_EQ(events.size(), 1U);
    ASSERT_EQ(events.front().lines.size(), 3U);
    EXPECT_EQ(events.front().heading.key, "event.fate.heading");
    EXPECT_EQ(events.front().lines.front().key, "event.fate.cohort_loss");
    EXPECT_EQ(events.front().lines[1].key, "event.fate.named.property_lost");
    EXPECT_EQ(events.front().lines[2].key, "event.fate.named.died");

    const auto& heading = events.front().heading.arguments;
    EXPECT_NE(std::ranges::find(heading, "event", &aetheria::narrative::NamedArgument::name),
              heading.end());
    EXPECT_NE(std::ranges::find(heading, "place", &aetheria::narrative::NamedArgument::name),
              heading.end());
    EXPECT_NE(std::ranges::find(heading, "xun", &aetheria::narrative::NamedArgument::name),
              heading.end());
}

TEST(NarrativeEvent, PollIsANonConsumingCoreSnapshotForUiRebuild) {
    const auto feed = aetheria::narrative::make_fate_presentation_fixture();
    const auto first = feed.poll();
    const auto second = feed.poll();
    ASSERT_EQ(first.size(), second.size());
    EXPECT_TRUE(std::ranges::equal(first, second));
}

}  // namespace
