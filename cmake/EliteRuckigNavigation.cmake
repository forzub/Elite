include(FetchContent)

function(elite_add_ruckig_navigation TARGET_NAME ELITE_ROOT)
    if(TARGET ${TARGET_NAME})
        return()
    endif()

    set(_ELITE_RUCKIG_OPTIONS
        BUILD_EXAMPLES
        BUILD_PYTHON_MODULE
        BUILD_CLOUD_CLIENT
        BUILD_TESTS
        BUILD_BENCHMARK
        BUILD_SHARED_LIBS
    )

    # Ruckig uses generic cache option names. Snapshot them so adding this
    # dependency cannot silently change unrelated Elite/dependency builds.
    foreach(_option IN LISTS _ELITE_RUCKIG_OPTIONS)
        if(DEFINED ${_option})
            set(_ELITE_RUCKIG_HAD_${_option} TRUE)
            set(_ELITE_RUCKIG_OLD_${_option} "${${_option}}")
        else()
            set(_ELITE_RUCKIG_HAD_${_option} FALSE)
        endif()
        set(${_option} OFF CACHE BOOL "" FORCE)
    endforeach()

    FetchContent_Declare(
        ruckig
        GIT_REPOSITORY https://github.com/pantor/ruckig.git
        GIT_TAG a8db97a4e9c55e5160a3855f739fa3b270df8e4c
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(ruckig)

    foreach(_option IN LISTS _ELITE_RUCKIG_OPTIONS)
        if(_ELITE_RUCKIG_HAD_${_option})
            set(${_option} "${_ELITE_RUCKIG_OLD_${_option}}" CACHE BOOL "" FORCE)
        else()
            unset(${_option} CACHE)
        endif()
    endforeach()

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
