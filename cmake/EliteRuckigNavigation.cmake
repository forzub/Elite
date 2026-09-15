include(FetchContent)

function(elite_add_ruckig_navigation TARGET_NAME ELITE_ROOT)
    if(TARGET ${TARGET_NAME})
        return()
    endif()

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

    # Ruckig v0.19.4 uses M_PI internally. MinGW strict C++20 does not expose
    # that macro unless the CRT math constants are explicitly enabled.
    if(MINGW)
        target_compile_definitions(ruckig PUBLIC _USE_MATH_DEFINES)
    endif()

    add_library(${TARGET_NAME} STATIC
        "${ELITE_ROOT}/src/game/navigation/RuckigTrajectorySolver.cpp"
    )
    target_include_directories(${TARGET_NAME} PUBLIC
        "${ELITE_ROOT}"
        "${ELITE_ROOT}/src"
    )
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
    target_link_libraries(${TARGET_NAME} PRIVATE ruckig::ruckig)

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
