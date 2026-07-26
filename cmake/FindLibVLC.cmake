include(FindPackageHandleStandardArgs)

set(_LibVLC_roots)
if(SAORS_LIBVLC_ROOT)
    list(APPEND _LibVLC_roots "${SAORS_LIBVLC_ROOT}")
endif()
if(DEFINED ENV{SAORS_LIBVLC_ROOT} AND NOT "$ENV{SAORS_LIBVLC_ROOT}" STREQUAL "")
    list(APPEND _LibVLC_roots "$ENV{SAORS_LIBVLC_ROOT}")
endif()

find_path(
    LibVLC_INCLUDE_DIR
    NAMES vlc/vlc.h
    HINTS ${_LibVLC_roots}
    PATH_SUFFIXES sdk/include include
)

if(LibVLC_INCLUDE_DIR AND EXISTS "${LibVLC_INCLUDE_DIR}/vlc/libvlc_version.h")
    file(
        STRINGS
        "${LibVLC_INCLUDE_DIR}/vlc/libvlc_version.h"
        _LibVLC_version_definitions
        REGEX
            "^#[ \t]*define[ \t]+LIBVLC_VERSION_(MAJOR|MINOR|REVISION)[ \t]+\\(?[0-9]+\\)?"
    )
    foreach(_LibVLC_component MAJOR MINOR REVISION)
        foreach(_LibVLC_definition IN LISTS _LibVLC_version_definitions)
            if(_LibVLC_definition MATCHES
               "^#[ \t]*define[ \t]+LIBVLC_VERSION_${_LibVLC_component}[ \t]+\\(?([0-9]+)\\)?")
                set(_LibVLC_${_LibVLC_component} "${CMAKE_MATCH_1}")
            endif()
        endforeach()
    endforeach()
    if(DEFINED _LibVLC_MAJOR AND DEFINED _LibVLC_MINOR AND
       DEFINED _LibVLC_REVISION)
        set(
            LibVLC_VERSION
            "${_LibVLC_MAJOR}.${_LibVLC_MINOR}.${_LibVLC_REVISION}"
        )
    endif()
endif()

find_file(
    LibVLC_IMPORT_LIBRARY
    NAMES libvlc.lib
    HINTS ${_LibVLC_roots}
    PATH_SUFFIXES sdk/lib lib
)
find_file(
    LibVLC_DLL
    NAMES libvlc.dll
    HINTS ${_LibVLC_roots}
)
find_file(
    LibVLC_CORE_DLL
    NAMES libvlccore.dll
    HINTS ${_LibVLC_roots}
)
find_path(
    LibVLC_PLUGINS_DIR
    NAMES access/libhttp_plugin.dll
    HINTS ${_LibVLC_roots}
    PATH_SUFFIXES plugins
)

set(LibVLC_ARCHITECTURE_OK FALSE)
if(LibVLC_DLL)
    file(SIZE "${LibVLC_DLL}" _LibVLC_dll_size)
    if(_LibVLC_dll_size GREATER 64)
        file(READ "${LibVLC_DLL}" _LibVLC_mz OFFSET 0 LIMIT 2 HEX)
        string(TOLOWER "${_LibVLC_mz}" _LibVLC_mz)
        if(_LibVLC_mz STREQUAL "4d5a")
            file(READ "${LibVLC_DLL}" _LibVLC_pe_offset_bytes OFFSET 60 LIMIT 4 HEX)
            string(SUBSTRING "${_LibVLC_pe_offset_bytes}" 0 2 _LibVLC_pe_b0)
            string(SUBSTRING "${_LibVLC_pe_offset_bytes}" 2 2 _LibVLC_pe_b1)
            string(SUBSTRING "${_LibVLC_pe_offset_bytes}" 4 2 _LibVLC_pe_b2)
            string(SUBSTRING "${_LibVLC_pe_offset_bytes}" 6 2 _LibVLC_pe_b3)
            math(
                EXPR
                _LibVLC_pe_offset
                "0x${_LibVLC_pe_b3}${_LibVLC_pe_b2}${_LibVLC_pe_b1}${_LibVLC_pe_b0}"
            )
            math(EXPR _LibVLC_machine_offset "${_LibVLC_pe_offset} + 4")
            file(
                READ
                "${LibVLC_DLL}"
                _LibVLC_machine
                OFFSET "${_LibVLC_machine_offset}"
                LIMIT 2
                HEX
            )
            string(TOLOWER "${_LibVLC_machine}" _LibVLC_machine)
            if(_LibVLC_machine STREQUAL "4c01")
                set(LibVLC_ARCHITECTURE_OK TRUE)
            else()
                set(
                    LibVLC_ARCHITECTURE_ERROR
                    "libvlc.dll is not Windows x86 (PE machine bytes: ${_LibVLC_machine})"
                )
            endif()
        else()
            set(LibVLC_ARCHITECTURE_ERROR "libvlc.dll is not a PE file")
        endif()
    endif()
endif()

string(
    CONCAT
    _LibVLC_failure_message
    "A complete Windows x86 libVLC SDK/runtime was not found. "
    "Set SAORS_LIBVLC_ROOT to an extracted official Win32 archive containing "
    "sdk/include, sdk/lib/libvlc.lib, libvlc.dll, libvlccore.dll, and plugins/."
)
if(LibVLC_ARCHITECTURE_ERROR)
    string(
        APPEND
        _LibVLC_failure_message
        " Architecture validation failed: ${LibVLC_ARCHITECTURE_ERROR}."
    )
endif()

find_package_handle_standard_args(
    LibVLC
    REQUIRED_VARS
        LibVLC_INCLUDE_DIR
        LibVLC_IMPORT_LIBRARY
        LibVLC_DLL
        LibVLC_CORE_DLL
        LibVLC_PLUGINS_DIR
        LibVLC_ARCHITECTURE_OK
        LibVLC_VERSION
    VERSION_VAR LibVLC_VERSION
    FAIL_MESSAGE "${_LibVLC_failure_message}"
)

if(LibVLC_FOUND)
    get_filename_component(LibVLC_RUNTIME_ROOT "${LibVLC_DLL}" DIRECTORY)
    if(NOT TARGET LibVLC::LibVLC)
        add_library(LibVLC::LibVLC SHARED IMPORTED)
        set_target_properties(
            LibVLC::LibVLC
            PROPERTIES
                IMPORTED_IMPLIB "${LibVLC_IMPORT_LIBRARY}"
                IMPORTED_LOCATION "${LibVLC_DLL}"
                INTERFACE_INCLUDE_DIRECTORIES "${LibVLC_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(
    LibVLC_INCLUDE_DIR
    LibVLC_IMPORT_LIBRARY
    LibVLC_DLL
    LibVLC_CORE_DLL
    LibVLC_PLUGINS_DIR
)
