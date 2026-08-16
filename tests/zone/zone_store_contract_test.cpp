#include "core/serialize/zone_codec.h"
#include "core/zone/file_zone_store.h"
#include "core/zone/zone_store.h"
#include "tests/support/ruleset_fixture.h"
#include "tests/zone/zone_test_support.h"

#include <gtest/gtest.h>

namespace {

using aetheria::serialize::encode_zone;
using aetheria::serialize::persistent_state_hash;
using aetheria::tests::entity_count;
using aetheria::tests::populated_zone;
using aetheria::tests::TemporaryDirectory;
using aetheria::tests::test_ruleset;
using aetheria::zone::child_key;
using aetheria::zone::FileZoneStore;
using aetheria::zone::InMemoryZoneStore;
using aetheria::zone::kRootZone;
using aetheria::zone::ZoneStore;

void expect_store_contract(ZoneStore& store) {
    const auto& ruleset = test_ruleset();
    const auto key = child_key(kRootZone, UINT16_C(0x1234), 0);
    auto source = populated_zone(key);
    const auto source_hash = persistent_state_hash(source, ruleset);
    const auto source_entities = entity_count(source);

    EXPECT_FALSE(store.contains(key));
    EXPECT_EQ(store.load(key), nullptr);
    store.save(source);
    EXPECT_TRUE(store.contains(key));

    const auto first = store.load(key);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(persistent_state_hash(*first, ruleset), source_hash);
    EXPECT_EQ(entity_count(*first), source_entities);
    EXPECT_EQ(encode_zone(*first, ruleset), encode_zone(source, ruleset));

    const auto second = store.load(key);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(persistent_state_hash(*second, ruleset), source_hash);
    EXPECT_TRUE(store.contains(key));

    EXPECT_TRUE(store.erase(key));
    EXPECT_FALSE(store.contains(key));
    EXPECT_EQ(store.load(key), nullptr);
    EXPECT_FALSE(store.erase(key));
}

TEST(ZoneStoreContract, InMemoryBackend) {
    InMemoryZoneStore store{test_ruleset()};
    expect_store_contract(store);
}

TEST(ZoneStoreContract, FileBackend) {
    TemporaryDirectory directory;
    FileZoneStore store{directory.path(), test_ruleset()};
    expect_store_contract(store);
}

}  // namespace
