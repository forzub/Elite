# Stage non-system DLL dependencies next to a MinGW-built executable.
#
# This script is intentionally toolchain-driven rather than a hard-coded list
# of libstdc++/libgcc/etc.  It scans PE imports recursively with the same
# objdump that belongs to the configured compiler and copies dependencies found
# in the MinGW toolchain bin directory. Windows system DLLs are left to the OS.

foreach(_required_var IN ITEMS
    ELITE_RUNTIME_TARGET_FILE
    ELITE_RUNTIME_TARGET_DIR
    ELITE_RUNTIME_TOOLCHAIN_BIN
    ELITE_RUNTIME_OBJDUMP
)
    if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
        message(FATAL_ERROR "StageMinGWRuntime.cmake: missing ${_required_var}")
    endif()
endforeach()

if(NOT EXISTS "${ELITE_RUNTIME_TARGET_FILE}")
    message(FATAL_ERROR
        "StageMinGWRuntime.cmake: target does not exist: ${ELITE_RUNTIME_TARGET_FILE}")
endif()

if(NOT EXISTS "${ELITE_RUNTIME_OBJDUMP}")
    message(FATAL_ERROR
        "StageMinGWRuntime.cmake: objdump does not exist: ${ELITE_RUNTIME_OBJDUMP}")
endif()

file(MAKE_DIRECTORY "${ELITE_RUNTIME_TARGET_DIR}")

set(_search_dirs
    "${ELITE_RUNTIME_TARGET_DIR}"
    "${ELITE_RUNTIME_TOOLCHAIN_BIN}"
)

# Some MinGW package layouts expose additional runtime directories through
# PATH. Only admit MinGW/MSYS directories; never copy arbitrary host DLLs.
file(TO_CMAKE_PATH "$ENV{PATH}" _environment_path)
foreach(_path_dir IN LISTS _environment_path)
    string(TOLOWER "${_path_dir}" _path_dir_lower)
    if(_path_dir_lower MATCHES "(mingw|msys)")
        list(APPEND _search_dirs "${_path_dir}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _search_dirs)

set(_scan_queue "${ELITE_RUNTIME_TARGET_FILE}")
set(_scanned_files)
set(_seen_import_names)
set(_copied_runtime_files)
set(_unresolved_imports)

while(_scan_queue)
    list(POP_FRONT _scan_queue _current_file)

    if(NOT EXISTS "${_current_file}")
        continue()
    endif()

    get_filename_component(_current_real "${_current_file}" REALPATH)
    list(FIND _scanned_files "${_current_real}" _already_scanned)
    if(NOT _already_scanned EQUAL -1)
        continue()
    endif()
    list(APPEND _scanned_files "${_current_real}")

    execute_process(
        COMMAND "${ELITE_RUNTIME_OBJDUMP}" -p "${_current_real}"
        RESULT_VARIABLE _objdump_result
        OUTPUT_VARIABLE _objdump_output
        ERROR_VARIABLE _objdump_error
    )

    if(NOT _objdump_result EQUAL 0)
        message(FATAL_ERROR
            "StageMinGWRuntime.cmake: objdump failed for ${_current_real}: ${_objdump_error}")
    endif()

    string(REGEX MATCHALL "DLL Name:[ \t]*[^\r\n]+" _import_lines "${_objdump_output}")

    foreach(_import_line IN LISTS _import_lines)
        string(REGEX REPLACE "^DLL Name:[ \t]*" "" _dll_name "${_import_line}")
        string(STRIP "${_dll_name}" _dll_name)
        string(TOLOWER "${_dll_name}" _dll_key)

        list(FIND _seen_import_names "${_dll_key}" _already_seen_name)
        if(NOT _already_seen_name EQUAL -1)
            continue()
        endif()
        list(APPEND _seen_import_names "${_dll_key}")

        set(_resolved_dll "")
        foreach(_search_dir IN LISTS _search_dirs)
            if(EXISTS "${_search_dir}/${_dll_name}")
                set(_resolved_dll "${_search_dir}/${_dll_name}")
                break()
            endif()
        endforeach()

        if(_resolved_dll STREQUAL "")
            # Imports not present in the MinGW runtime locations are assumed to
            # be Windows system DLLs. The clean-PATH acceptance test below is
            # the executable proof that no required third-party DLL was missed.
            list(APPEND _unresolved_imports "${_dll_name}")
            continue()
        endif()

        get_filename_component(_resolved_real "${_resolved_dll}" REALPATH)
        get_filename_component(_resolved_dir "${_resolved_real}" DIRECTORY)
        get_filename_component(_target_dir_real "${ELITE_RUNTIME_TARGET_DIR}" REALPATH)

        if(NOT _resolved_dir STREQUAL _target_dir_real)
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${_resolved_real}"
                    "${ELITE_RUNTIME_TARGET_DIR}/${_dll_name}"
                RESULT_VARIABLE _copy_result
            )
            if(NOT _copy_result EQUAL 0)
                message(FATAL_ERROR
                    "StageMinGWRuntime.cmake: failed to copy ${_resolved_real}")
            endif()
            list(APPEND _copied_runtime_files "${_dll_name}")
        endif()

        # Scan the source copy recursively. This discovers transitive runtime
        # dependencies such as freetype -> harfbuzz/png/zlib without naming
        # any particular third-party package in the build rules.
        list(APPEND _scan_queue "${_resolved_real}")
    endforeach()
endwhile()

list(REMOVE_DUPLICATES _copied_runtime_files)
list(SORT _copied_runtime_files)

if(_copied_runtime_files)
    string(JOIN ", " _copied_summary ${_copied_runtime_files})
    message(STATUS
        "Staged MinGW runtime for ${ELITE_RUNTIME_TARGET_FILE}: ${_copied_summary}")
else()
    message(STATUS
        "MinGW runtime already staged for ${ELITE_RUNTIME_TARGET_FILE}")
endif()
