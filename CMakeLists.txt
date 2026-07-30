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
option(SAORS_ENABLE_PLUGIN_SDK "Enable the optional plugin-sdk compile integration" OFF)
option(SAORS_FETCH_PLUGIN_SDK "Fetch the pinned plugin-sdk revision at configure time" OFF)
option(
    SAORS_ENABLE_EXPERIMENTAL_GAME_OBSERVER
    "Build the guarded experimental read-only game-state observer"
    OFF
)
option(
    SAORS_ENABLE_RADIO_CONTROLLER_DRY_RUN
    "Enable snapshot-driven radio decisions without a gameplay audio executor"
    OFF
)
option(
    SAORS_ENABLE_GAMEPLAY_STREAM_EXECUTOR
    "Enable the opt-in gameplay stream executor backed by libVLC and WinHTTP"
    OFF
)
set(SAORS_LIBVLC_ROOT "" CACHE PATH "Root of an externally supplied libVLC SDK/runtime")
set(SAORS_PLUGIN_SDK_ROOT "" CACHE PATH "Root of an external plugin-sdk checkout")
set(SAORS_TEST_STREAM_URL "" CACHE STRING "Private URL for opt-in network tests")
set(SAORS_PLUGIN_SUFFIX ".asi" CACHE STRING "Plugin suffix (override only for diagnostics)")
mark_as_advanced(SAORS_PLUGIN_SUFFIX SAORS_TEST_STREAM_URL)

if(SAORS_ENABLE_NETWORK_TESTS AND NOT SAORS_BUILD_TESTS)
    message(FATAL_ERROR "SAORS_ENABLE_NETWORK_TESTS requires SAORS_BUILD_TESTS=ON.")
endif()

if(SAORS_FETCH_PLUGIN_SDK AND NOT SAORS_ENABLE_PLUGIN_SDK)
    message(FATAL_ERROR "SAORS_FETCH_PLUGIN_SDK requires SAORS_ENABLE_PLUGIN_SDK=ON.")
endif()

if(SAORS_ENABLE_EXPERIMENTAL_GAME_OBSERVER)
    if(NOT SAORS_ENABLE_PLUGIN_SDK)
        message(
            FATAL_ERROR
            "SAORS_ENABLE_EXPERIMENTAL_GAME_OBSERVER requires SAORS_ENABLE_PLUGIN_SDK=ON."
        )
    endif()
    if(NOT SAORS_ENABLE_EXECUTABLE_FINGERPRINTING)
        message(
            FATAL_ERROR
            "The experimental observer requires executable fingerprinting."
        )
    endif()
    if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "The experimental observer currently requires MSVC Windows x86.")
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SAORS_PLUGIN_SDK_COMMIT "5da18b6f1956bb20bdfa39dcb07c44863ce26c81")
set(SAORS_RESOLVED_PLUGIN_SDK_ROOT "")
if(SAORS_ENABLE_PLUGIN_SDK)
    if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "plugin_III compile integration requires a Windows x86 target.")
    endif()

    if(SAORS_FETCH_PLUGIN_SDK)
        include(FetchContent)
        FetchContent_Declare(
            saors_plugin_sdk
            GIT_REPOSITORY https://github.com/DK22Pac/plugin-sdk.git
            GIT_TAG "${SAORS_PLUGIN_SDK_COMMIT}"
            GIT_SHALLOW FALSE
        )
        FetchContent_GetProperties(saors_plugin_sdk)
        if(NOT saors_plugin_sdk_POPULATED)
            FetchContent_Populate(saors_plugin_sdk)
        endif()
        set(SAORS_RESOLVED_PLUGIN_SDK_ROOT "${saors_plugin_sdk_SOURCE_DIR}")
    elseif(NOT SAORS_PLUGIN_SDK_ROOT STREQUAL "")
        get_filename_component(
            SAORS_RESOLVED_PLUGIN_SDK_ROOT
            "${SAORS_PLUGIN_SDK_ROOT}"
            ABSOLUTE
        )
    else()
        message(
            FATAL_ERROR
            "Set SAORS_PLUGIN_SDK_ROOT or enable SAORS_FETCH_PLUGIN_SDK."
        )
    endif()

    foreach(
        required_path
        LICENSE
        shared/PluginBase.h
        shared/Events.h
        plugin_III/plugin_III.h
        plugin_III/game_III/common.h
        plugin_III/game_III/CMenuManager.h
        plugin_III/game_III/cDMAudio.h
    )
        if(NOT EXISTS "${SAORS_RESOLVED_PLUGIN_SDK_ROOT}/${required_path}")
            message(FATAL_ERROR "The plugin-sdk checkout is missing ${required_path}.")
        endif()
    endforeach()

    find_package(Git REQUIRED)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SAORS_RESOLVED_PLUGIN_SDK_ROOT}" rev-parse HEAD
        OUTPUT_VARIABLE SAORS_PLUGIN_SDK_HEAD
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE SAORS_PLUGIN_SDK_GIT_RESULT
        ERROR_QUIET
    )
    if(
        NOT SAORS_PLUGIN_SDK_GIT_RESULT EQUAL 0
        OR NOT SAORS_PLUGIN_SDK_HEAD STREQUAL SAORS_PLUGIN_SDK_COMMIT
    )
        message(
            FATAL_ERROR
            "SAORS_PLUGIN_SDK_ROOT must be a Git checkout at ${SAORS_PLUGIN_SDK_COMMIT}."
        )
    endif()

    add_library(saors_plugin_sdk_reference INTERFACE)
    target_include_directories(
        saors_plugin_sdk_reference
        SYSTEM
        INTERFACE
            "${SAORS_RESOLVED_PLUGIN_SDK_ROOT}/shared"
            "${SAORS_RESOLVED_PLUGIN_SDK_ROOT}/shared/game"
            "${SAORS_RESOLVED_PLUGIN_SDK_ROOT}/shared/dxsdk"
            "${SAORS_RESOLVED_PLUGIN_SDK_ROOT}/plugin_III"
            "${SAORS_RESOLVED_PLUGIN_SDK_ROOT}/plugin_III/game_III"
            "${SAORS_RESOLVED_PLUGIN_SDK_ROOT}/plugin_III/game_III/enums"
            "${SAORS_RESOLVED_PLUGIN_SDK_ROOT}/plugin_III/game_III/rw"
    )
    target_compile_definitions(
        saors_plugin_sdk_reference
        INTERFACE
            GTA3
            PLUGIN_SGV_10EN
            RW
            _CRT_NON_CONFORMING_SWPRINTFS
            _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    )

    add_library(
        saors_plugin_sdk_compile_probe
        OBJECT
            cmake/PluginSdkCompileProbe.cpp
    )
    target_link_libraries(
        saors_plugin_sdk_compile_probe
        PRIVATE saors_plugin_sdk_reference
    )
    # This pinned plugin-sdk revision uses concepts and std::expected, matching
    # its upstream premake policy of C++latest. Keep that requirement isolated
    # from the C++17 project targets.
    target_compile_features(saors_plugin_sdk_compile_probe PRIVATE cxx_std_23)
endif()

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

if(TARGET saors_plugin_sdk_compile_probe)
    saors_enable_warnings(saors_plugin_sdk_compile_probe)
endif()

add_library(
    saors_core
    STATIC
        src/AudioBackend.cpp
        src/AudioBackendFactory.cpp
        src/Configuration.cpp
        src/GameAddressProfile.cpp
        src/GameIntegration.cpp
        src/GameObserver.cpp
        src/GameState.cpp
        src/HttpUrl.cpp
        src/Logger.cpp
        src/MemoryPatch.cpp
        src/PlaylistParser.cpp
        src/PlaylistResolver.cpp
        src/RadioActionSink.cpp
        src/RadioController.cpp
        src/RadioDecision.cpp
        src/RadioDecisionEngine.cpp
        src/RadioStationEvidence.cpp
        src/RadioStationMapRegistry.cpp
        src/RadioStationObservationRecorder.cpp
        src/RadioStationResolver.cpp
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

set(SAORS_HAS_EXECUTABLE_FINGERPRINTING 0)
if(SAORS_ENABLE_EXECUTABLE_FINGERPRINTING)
    target_sources(
        saors_core
        PRIVATE
            src/ExecutableFingerprint.cpp
            src/ExecutableProfileRegistry.cpp
            src/ExecutableReport.cpp
            src/PeImageReader.cpp
    )
    if(WIN32)
        target_sources(saors_core PRIVATE src/WindowsFileHasher.cpp)
        target_link_libraries(saors_core PUBLIC bcrypt)
    endif()
    set(SAORS_HAS_EXECUTABLE_FINGERPRINTING 1)
endif()

set(SAORS_HAS_EXPERIMENTAL_GAME_OBSERVER 0)
if(SAORS_ENABLE_EXPERIMENTAL_GAME_OBSERVER)
    target_sources(saors_core PRIVATE src/WindowsGameObserver.cpp)
    set(SAORS_HAS_EXPERIMENTAL_GAME_OBSERVER 1)
endif()

set(SAORS_HAS_RADIO_CONTROLLER_DRY_RUN 0)
if(SAORS_ENABLE_RADIO_CONTROLLER_DRY_RUN)
    set(SAORS_HAS_RADIO_CONTROLLER_DRY_RUN 1)
endif()

set(SAORS_HAS_GAMEPLAY_STREAM_EXECUTOR 0)
if(SAORS_ENABLE_GAMEPLAY_STREAM_EXECUTOR)
    if(NOT SAORS_ENABLE_RADIO_CONTROLLER_DRY_RUN)
        message(
            FATAL_ERROR
            "SAORS_ENABLE_GAMEPLAY_STREAM_EXECUTOR requires SAORS_ENABLE_RADIO_CONTROLLER_DRY_RUN=ON."
        )
    endif()
    if(NOT SAORS_ENABLE_EXPERIMENTAL_GAME_OBSERVER)
        message(
            FATAL_ERROR
            "SAORS_ENABLE_GAMEPLAY_STREAM_EXECUTOR requires SAORS_ENABLE_EXPERIMENTAL_GAME_OBSERVER=ON."
        )
    endif()
    if(NOT SAORS_ENABLE_LIBVLC)
        message(
            FATAL_ERROR
            "SAORS_ENABLE_GAMEPLAY_STREAM_EXECUTOR requires SAORS_ENABLE_LIBVLC=ON."
        )
    endif()
    if(NOT SAORS_ENABLE_WINHTTP)
        message(
            FATAL_ERROR
            "SAORS_ENABLE_GAMEPLAY_STREAM_EXECUTOR requires SAORS_ENABLE_WINHTTP=ON."
        )
    endif()
    set(SAORS_HAS_GAMEPLAY_STREAM_EXECUTOR 1)
endif()

target_compile_definitions(
    saors_core
    PUBLIC
        SAORS_HAS_EXECUTABLE_FINGERPRINTING=${SAORS_HAS_EXECUTABLE_FINGERPRINTING}
        SAORS_HAS_EXPERIMENTAL_GAME_OBSERVER=${SAORS_HAS_EXPERIMENTAL_GAME_OBSERVER}
        SAORS_HAS_GAMEPLAY_STREAM_EXECUTOR=${SAORS_HAS_GAMEPLAY_STREAM_EXECUTOR}
        SAORS_HAS_RADIO_CONTROLLER_DRY_RUN=${SAORS_HAS_RADIO_CONTROLLER_DRY_RUN}
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
            src/Main.cpp
    )
    target_link_libraries(SAORSForGTA3 PRIVATE saors_core)
    target_compile_definitions(
        SAORSForGTA3
        PRIVATE
            SAORS_PLUGIN_VERSION="${SAORS_VERSION}"
            WIN32_LEAN_AND_MEAN
            NOMINMAX
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

add_executable(
    saors_radio_map_tool
    tools/radio_map/Main.cpp
    src/RadioStationEvidence.cpp
)
target_include_directories(saors_radio_map_tool PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(saors_radio_map_tool PRIVATE cxx_std_17)
set_target_properties(
    saors_radio_map_tool
    PROPERTIES
        OUTPUT_NAME "saors_radio_map_tool"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
saors_enable_warnings(saors_radio_map_tool)

if(SAORS_BUILD_EXE_PROBE)
    if(NOT WIN32)
        message(FATAL_ERROR "SAORS_BUILD_EXE_PROBE requires a Windows target toolchain.")
    endif()
    if(NOT SAORS_ENABLE_EXECUTABLE_FINGERPRINTING)
        message(
            FATAL_ERROR
            "SAORS_BUILD_EXE_PROBE requires SAORS_ENABLE_EXECUTABLE_FINGERPRINTING=ON."
        )
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "saors_exe_probe must be built for Windows x86.")
    endif()
    add_executable(saors_exe_probe tools/exe_probe/Main.cpp)
    target_link_libraries(saors_exe_probe PRIVATE saors_core)
    target_compile_definitions(
        saors_exe_probe
        PRIVATE
            SAORS_EXE_PROBE_VERSION="${SAORS_VERSION}"
            WIN32_LEAN_AND_MEAN
            NOMINMAX
    )
    set_target_properties(
        saors_exe_probe
        PROPERTIES
            OUTPUT_NAME "saors_exe_probe"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    if(MINGW)
        target_link_options(saors_exe_probe PRIVATE -municode -static-libgcc -static-libstdc++)
    endif()
    saors_enable_warnings(saors_exe_probe)
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
        tests/GameObserverTests.cpp
        tests/HttpUrlTests.cpp
        tests/LibVlcAudioBackendTests.cpp
        tests/PlaylistParserTests.cpp
        tests/PlaylistResolverTests.cpp
        tests/RadioDecisionTests.cpp
        tests/RadioStationEvidenceTests.cpp
        tests/RadioStationObservationRecorderTests.cpp
        tests/RadioStationResolverTests.cpp
        tests/StreamManagerTests.cpp
        tests/UrlSanitizerTests.cpp
    )
    if(SAORS_ENABLE_EXECUTABLE_FINGERPRINTING)
        target_sources(
            saors_tests
            PRIVATE
                tests/ExecutableFingerprintTests.cpp
                tests/ExecutableProfileRegistryTests.cpp
                tests/PeImageReaderTests.cpp
                tests/WindowsFileHasherTests.cpp
        )
    endif()
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
    if(NOT MSVC)
        target_compile_options(saors_tests PRIVATE -Wno-c2y-extensions)
    endif()

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
