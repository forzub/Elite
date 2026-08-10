cmake_minimum_required(VERSION 3.20)
project(FeatureContractTests LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(
    ELITE_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

add_executable(
    feature_contract_tests
    DebugControlContractTests.cpp
)

target_include_directories(
    feature_contract_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(feature_contract_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(feature_contract_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(
    NAME feature_contracts
    COMMAND feature_contract_tests
)

set_tests_properties(
    feature_contracts
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
