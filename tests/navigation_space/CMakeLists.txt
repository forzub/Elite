cmake_minimum_required(VERSION 3.20)

project(NavigationSpaceTests LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(
    ELITE_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

add_subdirectory(
    "${ELITE_SOURCE_ROOT}/src/world/navigation/space"
    "${CMAKE_BINARY_DIR}/elite_navigation_space"
)

add_executable(
    navigation_space_tests
    NavigationSpaceContractTests.cpp
)

target_include_directories(
    navigation_space_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(navigation_space_tests PRIVATE cxx_std_17)
target_link_libraries(navigation_space_tests PRIVATE EliteNavigationSpace)

if(MSVC)
    target_compile_options(navigation_space_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(navigation_space_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME navigation_space COMMAND navigation_space_tests)
set_tests_properties(
    navigation_space
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
