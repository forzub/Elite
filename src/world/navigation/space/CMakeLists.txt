get_filename_component(
    ELITE_NAVIGATION_SPACE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../.."
    ABSOLUTE
)

add_library(EliteNavigationSpace STATIC
    NavigationSpace.cpp
    ../NavigationObstacleGeometry.cpp
)

target_include_directories(EliteNavigationSpace PUBLIC
    "${ELITE_NAVIGATION_SPACE_ROOT}"
    "${ELITE_NAVIGATION_SPACE_ROOT}/src"
)

target_compile_features(EliteNavigationSpace PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(EliteNavigationSpace PRIVATE /W4 /permissive-)
else()
    target_compile_options(EliteNavigationSpace PRIVATE -Wall -Wextra -Wpedantic)
endif()
