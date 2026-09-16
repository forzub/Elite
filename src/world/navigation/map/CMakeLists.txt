add_library(EliteNavigationMap STATIC
    NavigationMap.cpp
)

target_include_directories(EliteNavigationMap PUBLIC
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/src"
)

target_compile_features(EliteNavigationMap PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(EliteNavigationMap PRIVATE /W4 /permissive-)
else()
    target_compile_options(EliteNavigationMap PRIVATE -Wall -Wextra -Wpedantic)
endif()
