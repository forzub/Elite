get_filename_component(
    ELITE_NAVIGATION_LOCAL_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../.."
    ABSOLUTE
)

add_library(EliteNavigationLocal STATIC
    LocalHorizonPlanner.cpp
)

target_include_directories(EliteNavigationLocal PUBLIC
    "${ELITE_NAVIGATION_LOCAL_ROOT}"
    "${ELITE_NAVIGATION_LOCAL_ROOT}/src"
)

target_compile_features(EliteNavigationLocal PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(EliteNavigationLocal PRIVATE /W4 /permissive-)
else()
    target_compile_options(EliteNavigationLocal PRIVATE -Wall -Wextra -Wpedantic)
endif()
