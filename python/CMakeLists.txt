cmake_minimum_required(VERSION 3.24)
project(praxis_python LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(pybind11 CONFIG REQUIRED)

set(PRAXIS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

pybind11_add_module(praxis_native MODULE
    bindings/engine.cpp
    "${PRAXIS_SRC_DIR}/core/model_cascade.cpp"
    "${PRAXIS_SRC_DIR}/core/fact_store.cpp"
    "${PRAXIS_SRC_DIR}/core/digest.cpp"
    "${PRAXIS_SRC_DIR}/core/context_manager.cpp"
    "${PRAXIS_SRC_DIR}/core/dependency_graph.cpp"
    "${PRAXIS_SRC_DIR}/core/side_effect.cpp"
    "${PRAXIS_SRC_DIR}/core/tool_executor.cpp"
    "${PRAXIS_SRC_DIR}/core/cache.cpp"
    "${PRAXIS_SRC_DIR}/core/replay.cpp"
    "${PRAXIS_SRC_DIR}/core/telemetry.cpp"
    "${PRAXIS_SRC_DIR}/core/orchestrator.cpp"
    "${PRAXIS_SRC_DIR}/mcp/json_rpc.cpp"
    "${PRAXIS_SRC_DIR}/mcp/mcp_host.cpp"
    "${PRAXIS_SRC_DIR}/platform/posix_platform.cpp"
    "${PRAXIS_SRC_DIR}/ui/cli.cpp"
    "${PRAXIS_SRC_DIR}/ui/web.cpp"
    "${PRAXIS_SRC_DIR}/tools/registry.cpp"
    "${PRAXIS_SRC_DIR}/tools/builtin.cpp"
    "${PRAXIS_SRC_DIR}/llm/fake_provider.cpp"
    "${PRAXIS_SRC_DIR}/llm/openai_provider.cpp"
)

target_include_directories(praxis_native PRIVATE
    "${PRAXIS_SRC_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/httplib-src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/nlohmann_json-src/include"
)
target_link_libraries(praxis_native PRIVATE pybind11::pybind11)
target_compile_definitions(praxis_native PRIVATE
)
target_compile_options(praxis_native PRIVATE -O2)
