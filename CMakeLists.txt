cmake_minimum_required(VERSION 3.28)

project(SystemForge
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "LeetCode for System Design"
)

# --- C++23 Standard ---
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Compiler Warnings (per 08-code-standards.md) ---
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# --- Third-party Dependencies ---

# Cortex (coroutine/fiber library)
set(CORTEX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CORTEX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CORTEX_BUILD_APPS OFF CACHE BOOL "" FORCE)
# Cortex uses CMAKE_SOURCE_DIR for includes, which breaks as a subdirectory.
# Override after add_subdirectory to fix the include path.
add_subdirectory(third_party/cortex)
target_include_directories(cortex PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/cortex/include>
)

# nlohmann/json (header-only)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# --- SystemForge Engine Library ---
# Shared between native and WASM targets (no main())
add_library(systemforge_engine STATIC
    src/engine/version.cpp
    src/lang/token.cpp
    src/lang/value.cpp
    src/lang/lexer.cpp
    src/lang/parser.cpp
    src/lang/environment.cpp
    src/lang/interpreter.cpp
    src/engine/event.cpp
    src/engine/clock.cpp
    src/engine/event_queue.cpp
    src/engine/component.cpp
    src/engine/graph.cpp
    src/components/client.cpp
    src/components/http_server.cpp
    src/components/database.cpp
    src/components/cache.cpp
    src/engine/simulation_engine.cpp
    src/engine/test_runner.cpp
    src/bridge/bridge.cpp
)

target_include_directories(systemforge_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(systemforge_engine PUBLIC
    cortex::cortex
    nlohmann_json
)

if(EMSCRIPTEN)
    # --- WASM Target ---
    add_executable(systemforge_wasm
        src/bridge/bridge.cpp
    )

    target_link_libraries(systemforge_wasm PRIVATE
        systemforge_engine
    )

    target_link_options(systemforge_wasm PRIVATE
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToNewUTF8']"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORT_ES6=0"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME='SystemForge'"
    )

    set_target_properties(systemforge_wasm PROPERTIES
        OUTPUT_NAME "systemforge"
        SUFFIX ".js"
    )
else()
    # --- Native Target ---
    add_executable(systemforge
        src/main.cpp
    )

    target_link_libraries(systemforge PRIVATE
        systemforge_engine
    )

    # --- Tests (native only) ---
    include(cmake/CPM.cmake)

    CPMAddPackage(
        NAME GTest
        GITHUB_REPOSITORY google/googletest
        VERSION 1.14.0
    )

    enable_testing()
    include(GoogleTest)

    add_executable(systemforge_tests
        tests/engine/placeholder_test.cpp
        tests/lang/value_test.cpp
        tests/lang/lexer_test.cpp
        tests/lang/parser_test.cpp
        tests/lang/interpreter_test.cpp
        tests/engine/event_queue_test.cpp
        tests/components/component_test.cpp
        tests/engine/simulation_engine_test.cpp
        tests/engine/test_runner_test.cpp
        tests/bridge/bridge_test.cpp
    )

    target_link_libraries(systemforge_tests PRIVATE
        systemforge_engine
        GTest::gtest_main
    )

    gtest_discover_tests(systemforge_tests)
endif()
