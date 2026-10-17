target_sources(aetheria_tests PRIVATE
    tests/runtime/playable_session_test.cpp
    tests/runtime/character_save_test.cpp
    tests/runtime/save_raws_test.cpp
    tests/runtime/session_persistence_test.cpp
    tests/runtime/history_log_test.cpp
    tests/runtime/character_save_persistence_test.cpp
    tests/runtime/history_log_recovery_test.cpp
    tests/runtime/history_log_identity_test.cpp
    tests/runtime/history_log_continuity_test.cpp
    tests/runtime/history_log_multi_character_test.cpp
)
