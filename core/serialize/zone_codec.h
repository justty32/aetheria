#pragma once

#include "core/zone/zone.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace aetheria::serialize {

inline constexpr std::uint32_t kSaveFormatVersion = 1;

[[nodiscard]] std::string encode_zone(const zone::Zone& zone);
[[nodiscard]] std::unique_ptr<zone::Zone> decode_zone(std::string_view bytes);
[[nodiscard]] std::uint64_t persistent_state_hash(const zone::Zone& zone);

}  // namespace aetheria::serialize
