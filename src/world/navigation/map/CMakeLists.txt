get_filename_component(
    ELITE_NAVIGATION_MAP_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../.."
    ABSOLUTE
)

add_library(EliteNavigationMap STATIC
    NavigationMap.cpp
)

target_include_directories(EliteNavigationMap PUBLIC
    "${ELITE_NAVIGATION_MAP_ROOT}"
    "${ELITE_NAVIGATION_MAP_ROOT}/src"
)

target_compile_features(EliteNavigationMap PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(EliteNavigationMap PRIVATE /W4 /permissive-)
else()
    target_compile_options(EliteNavigationMap PRIVATE -Wall -Wextra -Wpedantic)
endif()
