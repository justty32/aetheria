#pragma once

// zone_codec 拆分後，encode_zone 與 decode_zone 兩側共用的常數與 helper。

#include "core/zone/zone.h"

#include <cstdint>
#include <stdexcept>

namespace aetheria::serialize::detail {

inline constexpr std::uint32_t kZoneMagic = UINT32_C(0x415A4F4E);
inline constexpr std::uint8_t kReservedPersistenceFlags = 0;

inline void validate_zone_meta(const zone::Zone& value) {
    const auto meta = value.reg.view<const zone::ZoneMeta>();
    if (meta.empty()) {
        throw std::runtime_error{"zone registry 缺少 ZoneMeta placeholder"};
    }
    for (const auto entity : meta) {
        if (meta.get<zone::ZoneMeta>(entity).zone_key != zone::value_of(value.key)) {
            throw std::runtime_error{"ZoneMeta 的 zone_key 與 zone 檔頭不符"};
        }
    }
}

}  // namespace aetheria::serialize::detail
