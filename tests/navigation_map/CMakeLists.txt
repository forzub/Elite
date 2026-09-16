cmake_minimum_required(VERSION 3.20)

project(NavigationMapTests LANGUAGES CXX)

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
    "${ELITE_SOURCE_ROOT}/src/world/navigation/map"
    "${CMAKE_BINARY_DIR}/elite_navigation_map"
)

add_executable(
    navigation_map_tests
    NavigationMapContractTests.cpp
)

target_include_directories(
    navigation_map_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(navigation_map_tests PRIVATE cxx_std_17)
target_link_libraries(navigation_map_tests PRIVATE EliteNavigationMap)

if(MSVC)
    target_compile_options(navigation_map_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(navigation_map_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME navigation_map COMMAND navigation_map_tests)
set_tests_properties(
    navigation_map
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
