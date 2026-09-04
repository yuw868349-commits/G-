cmake_minimum_required(VERSION 3.24)
project(SwiftAgent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Dependencies.cmake)
include(cmake/CompilerWarnings.cmake)

enable_testing()

add_library(swiftagent_deps INTERFACE)
target_link_libraries(swiftagent_deps INTERFACE nlohmann_json::nlohmann_json httplib::httplib)
target_include_directories(swiftagent_deps INTERFACE src)
swiftagent_target_warnings(swiftagent_deps)

add_library(swiftagent_core STATIC
    src/core/model_cascade.cpp
    src/core/fact_store.cpp
    src/core/digest.cpp
    src/core/context_manager.cpp
    src/core/dependency_graph.cpp
    src/core/side_effect.cpp
    src/core/tool_executor.cpp
    src/core/cache.cpp
    src/core/replay.cpp
    src/core/telemetry.cpp
    src/core/orchestrator.cpp
    src/mcp/json_rpc.cpp
    src/mcp/mcp_host.cpp
    src/platform/posix_platform.cpp
    src/ui/cli.cpp
    src/ui/web.cpp
)
target_link_libraries(swiftagent_core PUBLIC swiftagent_deps)

add_library(swiftagent_tools STATIC
    src/tools/registry.cpp
    src/tools/builtin.cpp
)
target_link_libraries(swiftagent_tools PUBLIC swiftagent_deps)

add_executable(swiftagent src/main.cpp)
target_link_libraries(swiftagent PRIVATE swiftagent_core swiftagent_tools swiftagent_llm)

add_library(swiftagent_llm STATIC
    src/llm/fake_provider.cpp
    src/llm/openai_provider.cpp
)
target_link_libraries(swiftagent_llm PUBLIC swiftagent_deps)

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
