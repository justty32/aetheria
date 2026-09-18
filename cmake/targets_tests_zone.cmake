target_sources(aetheria_tests PRIVATE
    tests/zone/zone_key_test.cpp
    tests/zone/zone_lifecycle_test.cpp
    tests/zone/zone_codec_test.cpp
    tests/zone/zone_store_contract_test.cpp
    tests/zone/file_zone_store_test.cpp
    tests/zone/file_zone_store_manifest_test.cpp
    tests/zone/zone_manager_test.cpp
    tests/zone/zone_manager_tick_test.cpp
    tests/zone/cross_zone_test.cpp
    tests/zone/diplomacy_save_test.cpp
    tests/zone/cross_zone_migration_test.cpp
    tests/zone/cross_zone_rollback_test.cpp
    tests/zone/diplomacy_save_legacy_v14_test.cpp
    tests/zone/diplomacy_save_version_validation_test.cpp
)
