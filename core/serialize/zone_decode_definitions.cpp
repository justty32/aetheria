#include "core/serialize/zone_codec.h"

#include "core/serialize/all_components.h"
#include "core/serialize/registry_codec.h"
#include "core/serialize/zone_codec_detail.h"
#include "core/serialize/zone_diplomacy_codec.h"
#include "core/serialize/zone_region_portals.h"
#include "core/site/site_lifecycle.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/vector.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "core/serialize/zone_decode_detail.h"

namespace aetheria::serialize::detail {
namespace {

template <typename Id, typename Finder>
[[nodiscard]] std::vector<Id> build_remap(const std::vector<std::string>& saved_ids,
                                          Finder&& find, std::string_view kind) {
    if (saved_ids.size() > static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) +
                               1U) {
        throw std::runtime_error{"zone 存檔的 " + std::string{kind} + " id 表超過 uint16 容量"};
    }
    std::vector<Id> result;
    result.reserve(saved_ids.size());
    for (const auto& string_id : saved_ids) {
        const auto current = find(string_id);
        if (!current.has_value()) {
            throw std::runtime_error{"zone 存檔含目前 Ruleset 不存在的 " + std::string{kind} +
                                     " id：" + string_id};
        }
        result.push_back(*current);
    }
    return result;
}

} // namespace

DefinitionRemaps decode_definition_remaps(cereal::PortableBinaryInputArchive& archive,
                                          const rules::Ruleset& ruleset) {
    std::vector<std::string> terrain_ids;
    std::vector<std::string> relief_ids;
    std::vector<std::string> feature_ids;
    std::vector<std::string> edge_ids;
    archive(terrain_ids, relief_ids, feature_ids, edge_ids);
    return {
        build_remap<rules::TerrainId>(
            terrain_ids, [&](std::string_view id) { return ruleset.find_terrain(id); }, "terrain"),
        build_remap<rules::ReliefId>(
            relief_ids, [&](std::string_view id) { return ruleset.find_relief(id); }, "relief"),
        build_remap<rules::FeatureId>(
            feature_ids, [&](std::string_view id) { return ruleset.find_feature(id); }, "feature"),
        build_remap<rules::EdgeId>(
            edge_ids, [&](std::string_view id) { return ruleset.find_edge(id); }, "edge"),
    };
}

} // namespace aetheria::serialize::detail
