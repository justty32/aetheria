#pragma once

#include "core/rules/ruleset.h"
#include "core/zone/zone.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace aetheria::serialize {

inline constexpr std::uint32_t kSaveFormatVersion = 15;

[[nodiscard]] std::string encode_zone(const zone::Zone& zone, const rules::Ruleset& ruleset);
[[nodiscard]] std::unique_ptr<zone::Zone> decode_zone(std::string_view bytes,
                                                      const rules::Ruleset& ruleset);
[[nodiscard]] std::uint64_t persistent_state_hash(const zone::Zone& zone,
                                                  const rules::Ruleset& ruleset);

}  // namespace aetheria::serialize
