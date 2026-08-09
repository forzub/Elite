cmake_minimum_required(VERSION 3.20)
project(InteractionActivationTests LANGUAGES CXX)

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
    interaction_activation_tests
    InteractionActivationTests.cpp
)

target_include_directories(
    interaction_activation_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

if(MSVC)
    target_compile_options(interaction_activation_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(interaction_activation_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(
    NAME interaction_activation
    COMMAND interaction_activation_tests
)

set_tests_properties(
    interaction_activation
    PROPERTIES
        WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
