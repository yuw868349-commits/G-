cmake_minimum_required(VERSION 3.24)
project(SwiftAgent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SWIFTAGENT_COVERAGE "Enable coverage flags" OFF)
option(SWIFTAGENT_BUILD_EXAMPLES "Build example programs" ON)
option(SWIFTAGENT_BUILD_BENCHMARKS "Build benchmark programs" ON)
option(SWIFTAGENT_BUILD_TESTS "Build unit and integration tests" ON)

if(SWIFTAGENT_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage -O0 -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
    add_compile_definitions(SWIFTAGENT_COVERAGE=1)
endif()

include(cmake/Dependencies.cmake)
include(cmake/CompilerWarnings.cmake)

enable_testing()

add_library(swiftagent_deps INTERFACE)
target_link_libraries(swiftagent_deps INTERFACE nlohmann_json::nlohmann_json httplib::httplib)
target_include_directories(swiftagent_deps INTERFACE src)
swiftagent_target_warnings(swiftagent_deps)

add_library(swiftagent_core STATIC
    src/core/cache.cpp
    src/core/context_manager.cpp
    src/core/dependency_graph.cpp
    src/core/digest.cpp
    src/core/fact_store.cpp
    src/core/model_cascade.cpp
    src/core/orchestrator.cpp
    src/core/replay.cpp
    src/core/registry.cpp
    src/core/side_effect.cpp
    src/core/telemetry.cpp
    src/core/tool_executor.cpp
    src/mcp/json_rpc.cpp
    src/mcp/mcp_host.cpp
    src/platform/posix_platform.cpp
    src/ui/cli.cpp
    src/ui/web.cpp
)
target_link_libraries(swiftagent_core PUBLIC swiftagent_deps)

if(WIN32)
    target_sources(swiftagent_core PRIVATE src/platform/windows_platform.cpp)
endif()

add_library(swiftagent_tools STATIC
    src/tools/builtin.cpp
    src/tools/registry.cpp
)
target_link_libraries(swiftagent_tools PUBLIC swiftagent_deps swiftagent_core)

add_executable(swiftagent src/main.cpp)
target_link_libraries(swiftagent PRIVATE swiftagent_core swiftagent_tools swiftagent_llm)

add_library(swiftagent_llm STATIC
    src/llm/fake_provider.cpp
    src/llm/openai_provider.cpp
    src/llm/provider_router.cpp
    src/llm/retrying_provider.cpp
)
target_link_libraries(swiftagent_llm PUBLIC swiftagent_deps swiftagent_core)

add_executable(unit_tests
    tests/unit/test_main.cpp
    tests/unit/test_skeleton.cpp
    tests/unit/test_core_types.cpp
    tests/unit/test_llm.cpp
    tests/unit/test_cascade.cpp
    tests/unit/test_fact_store.cpp
    tests/unit/test_digest.cpp
    tests/unit/test_context_manager.cpp
    tests/unit/test_dependency_graph.cpp
    tests/unit/test_side_effect.cpp
    tests/unit/test_executor.cpp
    tests/unit/test_cache.cpp
    tests/unit/test_platform.cpp
    tests/unit/test_replay.cpp
    tests/unit/test_telemetry.cpp
    tests/unit/test_orchestrator.cpp
    tests/unit/test_json_rpc.cpp
)
target_link_libraries(unit_tests PRIVATE Catch2::Catch2WithMain swiftagent_core swiftagent_tools swiftagent_llm)

include(Catch)
catch_discover_tests(unit_tests)

add_executable(integration_tests
    tests/integration/test_end_to_end.cpp
    tests/unit/test_main.cpp
)
target_link_libraries(integration_tests PRIVATE Catch2::Catch2WithMain swiftagent_core swiftagent_tools swiftagent_llm)
catch_discover_tests(integration_tests)

add_executable(bench_main
    tests/bench/bench_main.cpp
    tests/bench/benchwork.cpp
)
target_link_libraries(bench_main PRIVATE swiftagent_core swiftagent_tools swiftagent_llm)

if(SWIFTAGENT_BUILD_EXAMPLES)
    add_executable(example_minimal examples/cpp/01_minimal.cpp)
    target_link_libraries(example_minimal PRIVATE swiftagent_core swiftagent_tools swiftagent_llm)

    add_executable(example_custom_tool examples/cpp/02_custom_tool.cpp)
    target_link_libraries(example_custom_tool PRIVATE swiftagent_core swiftagent_tools swiftagent_llm)

    add_executable(example_telemetry examples/cpp/03_telemetry.cpp)
    target_link_libraries(example_telemetry PRIVATE swiftagent_core swiftagent_tools swiftagent_llm)

    add_executable(example_cache examples/cpp/04_cache.cpp)
    target_link_libraries(example_cache PRIVATE swiftagent_core swiftagent_tools swiftagent_llm)

    add_executable(example_mcp examples/cpp/05_mcp_stdio.cpp)
    if(UNIX)
        target_compile_definitions(example_mcp PRIVATE SWIFTAGENT_EXAMPLE_HAVE_POSIX_STDIO=1)
    endif()
    target_link_libraries(example_mcp PRIVATE swiftagent_core swiftagent_tools swiftagent_llm)
endif()
