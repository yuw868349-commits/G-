cmake_minimum_required(VERSION 3.24)
project(SwiftAgent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Dependencies.cmake)
include(cmake/CompilerWarnings.cmake)

enable_testing()

add_library(swiftagent_core INTERFACE)
target_link_libraries(swiftagent_core INTERFACE nlohmann_json::nlohmann_json httplib::httplib)
target_include_directories(swiftagent_core INTERFACE src)
swiftagent_target_warnings(swiftagent_core)

add_executable(swiftagent src/main.cpp)
target_link_libraries(swiftagent PRIVATE swiftagent_core)

add_library(swiftagent_llm STATIC
    src/llm/fake_provider.cpp
    src/llm/openai_provider.cpp
)
target_link_libraries(swiftagent_llm PUBLIC swiftagent_core)

add_executable(unit_tests
    tests/unit/test_main.cpp
    tests/unit/test_skeleton.cpp
    tests/unit/test_core_types.cpp
    tests/unit/test_llm.cpp
)
target_link_libraries(unit_tests PRIVATE Catch2::Catch2WithMain swiftagent_core swiftagent_llm)

include(Catch)
catch_discover_tests(unit_tests)
