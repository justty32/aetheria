#pragma once

#include "core/serialize/zone_codec.h"

namespace aetheria::serialize::detail {

void validate_encode_zone(const zone::Zone& value, const rules::Ruleset& ruleset);

} // namespace aetheria::serialize::detail
