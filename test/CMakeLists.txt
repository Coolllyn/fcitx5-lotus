# Wraps the core objects compiled once in src/lotus_core_objects, so the Go
# runtime is linked once here; exclude-libs keeps its symbols out of the
# dynamic symbol table.
add_library(lotus_test_core SHARED
    $<TARGET_OBJECTS:lotus_core_objects>
    $<TARGET_OBJECTS:lotus_executable_path>
)
target_link_options(lotus_test_core PRIVATE "-Wl,-exclude-libs,ALL")
target_include_directories(lotus_test_core PUBLIC
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(lotus_test_core PUBLIC
    Fcitx5::Core
    Fcitx5::Config
    Fcitx5::Module::Emoji
    Bamboo::Core
    Pthread::Pthread
)

# Helper to compile a standalone headless integration test linked with the
# Lotus engine core.
#
# Usage:
#   add_lotus_headless_test(<target_name> <source_file> "<labels>")
# Example:
#   add_lotus_headless_test(my_test my-test.cpp "integration;myfeature")
function(add_lotus_headless_test target source labels)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE
        lotus_test_core
        Fcitx5::Core
        Fcitx5::Config
        Fcitx5::Module::Emoji
        Pthread::Pthread
    )
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "${labels}" TIMEOUT 20)
endfunction()

# Helper to compile lightweight unit test linked with Bamboo engine core.
function(add_bamboo_test target source labels)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE Bamboo::Core)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "${labels}" TIMEOUT 20)
endfunction()

# Helper to compile a portable unit test of the C++ engine (lotus/), no fcitx or Go needed.
#
# Usage:
#   add_lotus_engine_test(<target_name> <source_file> "<labels>")
function(add_lotus_engine_test target source labels)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE Lotus::Engine)
    target_compile_definitions(${target} PRIVATE LOTUS_TEST_GOLDEN_DIR="${PROJECT_SOURCE_DIR}/test/golden")
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "${labels}" TIMEOUT 60)
endfunction()

add_lotus_engine_test(lotus_utf8 lotus-utf8-test.cpp "unit;lotus")
add_lotus_engine_test(lotus_data lotus-data-test.cpp "unit;lotus")
add_lotus_engine_test(lotus_utils lotus-utils-test.cpp "unit;lotus")
add_lotus_engine_test(lotus_encoder lotus-encoder-test.cpp "unit;lotus;charset")
add_lotus_engine_test(lotus_rules_parser lotus-rules-parser-test.cpp "unit;lotus;rules")
add_lotus_engine_test(lotus_golden_smoke lotus-golden-smoke.cpp "unit;lotus;golden")

add_lotus_headless_test(preedit_lifecycle preedit-lifecycle.cpp "integration;preedit")
add_lotus_headless_test(app_rule_reset app-rule-reset.cpp "integration;apprule")
add_lotus_headless_test(preedit_smoke_client preedit-smoke-client.cpp "integration;preedit")
add_lotus_headless_test(preedit_smoke_server preedit-smoke-server.cpp "integration;preedit")
add_lotus_headless_test(smooth_buffered_key_replay
    smooth-buffered-key-replay.cpp "integration;smooth")
add_lotus_headless_test(select_mode_replacement
    select-mode-replacement.cpp "integration;select")
add_lotus_headless_test(app_rule_delays app-rule-delays.cpp "integration;apprule")
add_lotus_headless_test(replacement_delays replacement-delays.cpp "integration;smooth")
add_lotus_headless_test(app_rule_delay_wire app-rule-delay-wire.cpp "integration;apprule")
add_lotus_headless_test(dict_reload dict-reload.cpp "integration;dictionary")
set_tests_properties(smooth_buffered_key_replay select_mode_replacement replacement_delays app_rule_delay_wire
    PROPERTIES RESOURCE_LOCK lotus_kb_socket)
add_lotus_headless_test(modifier_key_bypasses_surrounding_text
    modifier-key-bypasses-surrounding-text.cpp "integration;surrounding")
add_lotus_headless_test(emoji_history_page_navigation
    emoji-history-page-navigation.cpp "integration;emoji")
add_bamboo_test(numeric_macro_case_preservation
    numeric-macro-case-preservation.cpp "unit;macro")

add_executable(emoji_backspace_utf8
    test-emoji-backspace.cpp
    ${PROJECT_SOURCE_DIR}/src/lotus-utils.cpp
)
target_include_directories(emoji_backspace_utf8 PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(emoji_backspace_utf8 PRIVATE
    Fcitx5::Core
)
add_test(NAME emoji_backspace_utf8 COMMAND emoji_backspace_utf8)
set_tests_properties(emoji_backspace_utf8 PROPERTIES
    LABELS "unit;emoji"
    TIMEOUT 20
)

add_executable(executable_path_match
    executable-path-match.cpp
)
target_link_libraries(executable_path_match PRIVATE lotus_executable_path)
add_test(NAME executable_path_match COMMAND executable_path_match)
set_tests_properties(executable_path_match PROPERTIES
    LABELS "unit;packaging"
    TIMEOUT 20
)
