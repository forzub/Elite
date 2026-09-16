cmake_minimum_required(VERSION 3.20)

project(NavigationRuckigTests LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(
    ELITE_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

include("${ELITE_SOURCE_ROOT}/cmake/EliteRuckigNavigation.cmake")
elite_add_ruckig_navigation(EliteNavigationRuckig "${ELITE_SOURCE_ROOT}")

add_executable(
    navigation_ruckig_tests
    RuckigTrajectorySolverTests.cpp
)

target_include_directories(
    navigation_ruckig_tests
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(navigation_ruckig_tests PRIVATE cxx_std_17)
target_link_libraries(navigation_ruckig_tests PRIVATE EliteNavigationRuckig)

if(MSVC)
    target_compile_options(navigation_ruckig_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(navigation_ruckig_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME navigation_ruckig COMMAND navigation_ruckig_tests)
set_tests_properties(
    navigation_ruckig
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
