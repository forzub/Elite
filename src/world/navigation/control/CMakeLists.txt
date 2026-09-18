get_filename_component(
    ELITE_NAVIGATION_CONTROL_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../.."
    ABSOLUTE
)

add_library(EliteNavigationControl STATIC
    PilotSkillExecutor.cpp
)

target_include_directories(EliteNavigationControl PUBLIC
    "${ELITE_NAVIGATION_CONTROL_ROOT}"
    "${ELITE_NAVIGATION_CONTROL_ROOT}/src"
)

target_compile_features(EliteNavigationControl PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(EliteNavigationControl PRIVATE /W4 /permissive-)
else()
    target_compile_options(EliteNavigationControl PRIVATE -Wall -Wextra -Wpedantic)
endif()
