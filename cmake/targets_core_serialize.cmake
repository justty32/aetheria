target_sources(aetheria_core PRIVATE
    core/serialize/normalized_state_hash.cpp
    core/serialize/zone_encode.cpp
    core/serialize/zone_decode.cpp
    core/serialize/zone_diplomacy_codec.cpp
    core/serialize/normalized_state_hash_city.cpp
    core/serialize/normalized_state_hash_diplomacy.cpp
    core/serialize/normalized_state_hash_fate.cpp
    core/serialize/normalized_state_hash_payload.cpp
    core/serialize/normalized_state_hash_units.cpp
    core/serialize/zone_decode_definitions.cpp
    core/serialize/zone_decode_registry.cpp
    core/serialize/zone_diplomacy_decode.cpp
    core/serialize/zone_encode_validate.cpp
)
