target_sources(aetheria_tests PRIVATE
    tests/rules/ruleset_load_test.cpp
    tests/rules/power_test.cpp
    tests/rules/combat_test.cpp
    tests/rules/individual_rules_test.cpp
    tests/rules/ruleset_error_test.cpp
    tests/rules/ruleset_zone_codec_test.cpp
    tests/rules/diplomacy_rules_test.cpp
    tests/rules/power_sources_test.cpp
    tests/rules/combat_calibration_test.cpp
    tests/rules/combat_formula_test.cpp
    tests/rules/individual_rules_check_test.cpp
    tests/rules/individual_rules_damage_test.cpp
    tests/rules/power_sources_connectors_test.cpp
    tests/rules/power_sources_magic_test.cpp
    tests/rules/power_sources_restrictions_test.cpp
    tests/rules/ruleset_error_constraints_test.cpp
)
