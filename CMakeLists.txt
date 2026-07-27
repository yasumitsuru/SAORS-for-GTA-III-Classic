cmake_minimum_required(VERSION 3.24)

cmake_policy(SET CMP0091 NEW)

project(
    SAORSForGTA3
    VERSION 0.2.0
    DESCRIPTION "Clean-room online radio support for GTA III Classic"
    LANGUAGES CXX
)

set(SAORS_VERSION "${PROJECT_VERSION}-dev")

option(SAORS_BUILD_TESTS "Build host-runnable unit tests" ON)
option(SAORS_ENABLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(SAORS_BUILD_ASI "Build the Windows x86 ASI plugin" ${WIN32})
option(SAORS_BUILD_STREAM_PROBE "Build the standalone stream probe" ON)
option(SAORS_BUILD_EXE_PROBE "Build the standalone executable probe" ${WIN32})
option(SAORS_ENABLE_LIBVLC "Build the optional dynamic libVLC backend" OFF)
option(SAORS_ENABLE_WINHTTP "Build the Windows WinHTTP playlist client" ${WIN32})
option(
    SAORS_ENABLE_EXECUTABLE_FINGERPRINTING
    "Build defensive PE executable fingerprinting support"
    ON
)
option(SAORS_ENABLE_NETWORK_TESTS "Build opt-in network playback tests" OFF)
option(SAORS_ENABLE_EXPERIMENTAL_HOOKS "Enable unverified experimental game hooks" OFF)
set(SAORS_LIBVLC_ROOT "" CACHE PATH "Root of an externally supplied libVLC SDK/runtime")
set(SAORS_TEST_STREAM_URL "" CACHE STRING "Private URL for opt-in network tests")
set(SAORS_PLUGIN_SUFFIX ".asi" CACHE STRING "Plugin suffix (override only for diagnostics)")
mark_as_advanced(SAORS_PLUGIN_SUFFIX SAORS_TEST_STREAM_URL)

if(SAORS_ENABLE_NETWORK_TESTS AND NOT SAORS_BUILD_TESTS)
    message(FATAL_ERROR "SAORS_ENABLE_NETWORK_TESTS requires SAORS_BUILD_TESTS=ON.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

function(saors_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
        if(SAORS_ENABLE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(
            ${target}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
        )
        if(SAORS_ENABLE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(
    saors_core
    STATIC
        src/AudioBackend.cpp
        src/AudioBackendFactory.cpp
        src/Configuration.cpp
        src/HttpUrl.cpp
        src/Logger.cpp
        src/PlaylistParser.cpp
        src/PlaylistResolver.cpp
        src/PeImageReader.cpp
        src/RadioController.cpp
        src/StreamManager.cpp
        src/UrlSanitizer.cpp
)

target_include_directories(
    saors_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_compile_features(saors_core PUBLIC cxx_std_17)
target_compile_definitions(
    saors_core
    PRIVATE
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
        $<$<PLATFORM_ID:Windows>:NOMINMAX>
)

set(SAORS_HAS_LIBVLC 0)
if(SAORS_ENABLE_LIBVLC)
    if(NOT WIN32)
        message(FATAL_ERROR "The libVLC backend currently requires a Windows target.")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(
            FATAL_ERROR
            "The libVLC backend must be built for Windows x86. "
            "Use a Win32 MSVC generator or the i686 MinGW toolchain."
        )
    endif()

    find_package(LibVLC 3.0 REQUIRED)
    target_sources(saors_core PRIVATE src/LibVlcAudioBackend.cpp)
    target_include_directories(saors_core PRIVATE "${LibVLC_INCLUDE_DIR}")
    set(SAORS_HAS_LIBVLC 1)
endif()

set(SAORS_HAS_WINHTTP 0)
if(SAORS_ENABLE_WINHTTP)
    if(NOT WIN32)
        message(FATAL_ERROR "SAORS_ENABLE_WINHTTP requires a Windows target toolchain.")
    endif()
    target_sources(saors_core PRIVATE src/WinHttpClient.cpp)
    target_link_libraries(saors_core PUBLIC winhttp)
    set(SAORS_HAS_WINHTTP 1)
endif()

target_compile_definitions(
    saors_core
    PUBLIC
        SAORS_HAS_LIBVLC=${SAORS_HAS_LIBVLC}
        SAORS_HAS_WINHTTP=${SAORS_HAS_WINHTTP}
)
saors_enable_warnings(saors_core)

if(SAORS_BUILD_ASI)
    if(NOT WIN32)
        message(FATAL_ERROR "SAORS_BUILD_ASI requires a Windows target toolchain.")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(
            FATAL_ERROR
            "SAORSForGTA3.asi must be built for Windows x86 (32-bit). "
            "Use a Win32 MSVC preset or the i686 MinGW toolchain."
        )
    endif()

    add_library(
        SAORSForGTA3
        SHARED
            src/GameIntegration.cpp
            src/Main.cpp
    )
    target_link_libraries(SAORSForGTA3 PRIVATE saors_core)
    target_compile_definitions(
        SAORSForGTA3
        PRIVATE
            SAORS_PLUGIN_VERSION="${SAORS_VERSION}"
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            $<$<BOOL:${SAORS_ENABLE_EXPERIMENTAL_HOOKS}>:SAORS_ENABLE_EXPERIMENTAL_HOOKS=1>
    )
    set_target_properties(
        SAORSForGTA3
        PROPERTIES
            PREFIX ""
            SUFFIX "${SAORS_PLUGIN_SUFFIX}"
            OUTPUT_NAME "SAORSForGTA3"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    if(MINGW)
        target_link_options(
            SAORSForGTA3
            PRIVATE
                -static-libgcc
                -static-libstdc++
        )
    endif()
    saors_enable_warnings(SAORSForGTA3)
endif()

if(SAORS_BUILD_STREAM_PROBE)
    add_executable(saors_stream_probe tools/stream_probe/Main.cpp)
    target_link_libraries(saors_stream_probe PRIVATE saors_core)
    target_compile_definitions(
        saors_stream_probe
        PRIVATE
            SAORS_STREAM_PROBE_VERSION="${SAORS_VERSION}"
            WIN32_LEAN_AND_MEAN
            NOMINMAX
    )
    set_target_properties(
        saors_stream_probe
        PROPERTIES
            OUTPUT_NAME "saors_stream_probe"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    saors_enable_warnings(saors_stream_probe)
endif()

if(SAORS_BUILD_TESTS)
    include(CTest)
    include(FetchContent)

    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.8.1
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    add_executable(
        saors_tests
        tests/AudioBackendFactoryTests.cpp
        tests/ConfigurationTests.cpp
        tests/HttpUrlTests.cpp
        tests/LibVlcAudioBackendTests.cpp
        tests/PlaylistParserTests.cpp
        tests/PlaylistResolverTests.cpp
        tests/StreamManagerTests.cpp
        tests/UrlSanitizerTests.cpp
    )
    if(SAORS_ENABLE_WINHTTP)
        target_sources(
            saors_tests
            PRIVATE
                tests/WinHttpClientTests.cpp
                tests/WinHttpIntegrationTests.cpp
        )
        target_link_libraries(saors_tests PRIVATE ws2_32)
    endif()
    target_link_libraries(saors_tests PRIVATE saors_core Catch2::Catch2WithMain)
    if(SAORS_ENABLE_LIBVLC)
        file(TO_CMAKE_PATH "${LibVLC_RUNTIME_ROOT}" SAORS_TEST_LIBVLC_ROOT)
        target_compile_definitions(
            saors_tests
            PRIVATE SAORS_TEST_LIBVLC_ROOT="${SAORS_TEST_LIBVLC_ROOT}"
        )
    endif()
    saors_enable_warnings(saors_tests)

    include(Catch)
    catch_discover_tests(saors_tests)

    if(SAORS_ENABLE_NETWORK_TESTS)
        if(NOT SAORS_ENABLE_LIBVLC)
            message(FATAL_ERROR "SAORS_ENABLE_NETWORK_TESTS requires SAORS_ENABLE_LIBVLC=ON.")
        endif()
        if(SAORS_TEST_STREAM_URL STREQUAL "")
            message(
                FATAL_ERROR
                "SAORS_ENABLE_NETWORK_TESTS requires a private SAORS_TEST_STREAM_URL."
            )
        endif()
        add_executable(
            saors_network_tests
            tests/NetworkAudioBackendTests.cpp
        )
        target_link_libraries(
            saors_network_tests
            PRIVATE
                saors_core
                Catch2::Catch2WithMain
        )
        target_compile_definitions(
            saors_network_tests
            PRIVATE SAORS_TEST_LIBVLC_ROOT="${SAORS_TEST_LIBVLC_ROOT}"
        )
        saors_enable_warnings(saors_network_tests)
        add_test(NAME saors_network_tests COMMAND saors_network_tests)
        set_tests_properties(
            saors_network_tests
            PROPERTIES
                ENVIRONMENT "SAORS_TEST_STREAM_URL=${SAORS_TEST_STREAM_URL}"
                LABELS "network"
        )
    endif()
endif()
