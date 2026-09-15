cmake_minimum_required(VERSION 3.20)

project(NavigationRuckigTests LANGUAGES CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep the spike fully local/offline and minimal. In particular, community
# waypoint cloud support is deliberately disabled.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON_MODULE OFF CACHE BOOL "" FORCE)
set(BUILD_CLOUD_CLIENT OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    ruckig
    GIT_REPOSITORY https://github.com/pantor/ruckig.git
    GIT_TAG a8db97a4e9c55e5160a3855f739fa3b270df8e4c
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(ruckig)

# Ruckig v0.19.4 uses the M_PI macro in its public/internal headers. MinGW's
# strict -std=c++20 mode hides that non-standard macro unless this compatibility
# switch is enabled. Upstream already does the equivalent for MSVC; keep the
# workaround target-local so the rest of Elite remains unaffected.
if(MINGW)
    target_compile_definitions(ruckig PUBLIC _USE_MATH_DEFINES)
endif()

get_filename_component(
    ELITE_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

add_library(
    elite_ruckig_navigation
    STATIC
    "${ELITE_SOURCE_ROOT}/src/game/navigation/RuckigTrajectorySolver.cpp"
)

target_include_directories(
    elite_ruckig_navigation
    PRIVATE
        "${ELITE_SOURCE_ROOT}"
        "${ELITE_SOURCE_ROOT}/src"
)

target_compile_features(elite_ruckig_navigation PRIVATE cxx_std_20)
target_link_libraries(elite_ruckig_navigation PRIVATE ruckig::ruckig)

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
target_link_libraries(navigation_ruckig_tests PRIVATE elite_ruckig_navigation)

if(MSVC)
    target_compile_options(elite_ruckig_navigation PRIVATE /W4 /permissive-)
    target_compile_options(navigation_ruckig_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(elite_ruckig_navigation PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(navigation_ruckig_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME navigation_ruckig COMMAND navigation_ruckig_tests)
set_tests_properties(
    navigation_ruckig
    PROPERTIES WORKING_DIRECTORY "${ELITE_SOURCE_ROOT}"
)
