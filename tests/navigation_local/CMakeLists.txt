cmake_minimum_required(VERSION 3.20)

project(NavigationLocalTests LANGUAGES CXX)

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
    "${ELITE_SOURCE_ROOT}/src/world/navigation/local"
    "${CMAKE_BINARY_DIR}/elite_navigation_local"
)

add_executable(
    navigation_local_tests
    NavigationLocalContractTests.cpp
)

target_include_directories(
    navigation_local_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(navigation_local_tests PRIVATE cxx_std_17)
target_link_libraries(navigation_local_tests PRIVATE EliteNavigationLocal)

add_executable(
    navigation_local_avoidance_tests
    NavigationLocalAvoidanceTests.cpp
)

target_include_directories(
    navigation_local_avoidance_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(navigation_local_avoidance_tests PRIVATE cxx_std_17)
target_link_libraries(navigation_local_avoidance_tests PRIVATE EliteNavigationLocal)

if(MSVC)
    target_compile_options(navigation_local_tests PRIVATE /W4 /permissive-)
    target_compile_options(navigation_local_avoidance_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(navigation_local_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(navigation_local_avoidance_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME navigation_local COMMAND navigation_local_tests)
add_test(NAME navigation_local_avoidance COMMAND navigation_local_avoidance_tests)

set_tests_properties(
    navigation_local
    navigation_local_avoidance
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
