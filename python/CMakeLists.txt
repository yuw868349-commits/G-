cmake_minimum_required(VERSION 3.24)
project(swiftagent_python LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(pybind11 CONFIG REQUIRED)

set(SWIFTAGENT_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

pybind11_add_module(swiftagent_native MODULE
    bindings/engine.cpp
    "${SWIFTAGENT_SRC_DIR}/core/model_cascade.cpp"
    "${SWIFTAGENT_SRC_DIR}/core/fact_store.cpp"
    "${SWIFTAGENT_SRC_DIR}/core/digest.cpp"
    "${SWIFTAGENT_SRC_DIR}/core/context_manager.cpp"
    "${SWIFTAGENT_SRC_DIR}/core/dependency_graph.cpp"
    "${SWIFTAGENT_SRC_DIR}/core/side_effect.cpp"
    "${SWIFTAGENT_SRC_DIR}/core/tool_executor.cpp"
    "${SWIFTAGENT_SRC_DIR}/core/cache.cpp"
    "${SWIFTAGENT_SRC_DIR}/core/replay.cpp"
    "${SWIFTAGENT_SRC_DIR}/core/telemetry.cpp"
    "${SWIFTAGENT_SRC_DIR}/core/orchestrator.cpp"
    "${SWIFTAGENT_SRC_DIR}/mcp/json_rpc.cpp"
    "${SWIFTAGENT_SRC_DIR}/mcp/mcp_host.cpp"
    "${SWIFTAGENT_SRC_DIR}/platform/posix_platform.cpp"
    "${SWIFTAGENT_SRC_DIR}/ui/cli.cpp"
    "${SWIFTAGENT_SRC_DIR}/ui/web.cpp"
    "${SWIFTAGENT_SRC_DIR}/tools/registry.cpp"
    "${SWIFTAGENT_SRC_DIR}/tools/builtin.cpp"
    "${SWIFTAGENT_SRC_DIR}/llm/fake_provider.cpp"
    "${SWIFTAGENT_SRC_DIR}/llm/openai_provider.cpp"
)

target_include_directories(swiftagent_native PRIVATE
    "${SWIFTAGENT_SRC_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/httplib-src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/nlohmann_json-src/include"
)
target_link_libraries(swiftagent_native PRIVATE pybind11::pybind11)
target_compile_definitions(swiftagent_native PRIVATE
)
target_compile_options(swiftagent_native PRIVATE -O2)
