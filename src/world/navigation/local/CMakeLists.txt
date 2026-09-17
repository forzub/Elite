get_filename_component(
    ELITE_NAVIGATION_LOCAL_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../.."
    ABSOLUTE
)

if(NOT TARGET EliteNavigationSpace)
    add_subdirectory(
        "${ELITE_NAVIGATION_LOCAL_ROOT}/src/world/navigation/space"
        "${CMAKE_CURRENT_BINARY_DIR}/navigation_space"
    )
endif()

add_library(EliteNavigationLocal STATIC
    LocalHorizonPlanner.cpp
    LocalAvoidancePlanner.cpp
)

target_include_directories(EliteNavigationLocal PUBLIC
    "${ELITE_NAVIGATION_LOCAL_ROOT}"
    "${ELITE_NAVIGATION_LOCAL_ROOT}/src"
)

target_compile_features(EliteNavigationLocal PUBLIC cxx_std_17)
target_link_libraries(EliteNavigationLocal PUBLIC EliteNavigationSpace)

if(MSVC)
    target_compile_options(EliteNavigationLocal PRIVATE /W4 /permissive-)
else()
    target_compile_options(EliteNavigationLocal PRIVATE -Wall -Wextra -Wpedantic)
endif()
