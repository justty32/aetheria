#pragma once

#include "core/serialize/zone_codec.h"

#include <cereal/archives/portable_binary.hpp>
#include <istream>
#include <vector>

namespace aetheria::serialize::detail {

struct DefinitionRemaps {
    std::vector<rules::TerrainId> terrain;
    std::vector<rules::ReliefId> relief;
    std::vector<rules::FeatureId> feature;
    std::vector<rules::EdgeId> edge;
};

DefinitionRemaps decode_definition_remaps(cereal::PortableBinaryInputArchive& archive,
                                          const rules::Ruleset& ruleset);
void decode_registry(std::istream& stream, zone::Zone* value,
                     const rules::Ruleset& ruleset, std::uint32_t version);

} // namespace aetheria::serialize::detail
