#include "saors_gta3/AudioBackendFactory.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

TEST_CASE("Factory selects the null backend when explicitly requested") {
    saors::AudioBackendFactoryOptions options;
    options.requested = saors::RequestedAudioBackend::nullBackend;

    const auto backend = saors::createConfiguredAudioBackend(options);

    REQUIRE(backend);
    CHECK(std::string(backend->name()) == "null");
}

TEST_CASE("Factory safely falls back when libVLC initialization fails") {
    saors::AudioBackendFactoryOptions options;
    options.requested = saors::RequestedAudioBackend::libVlc;
    options.libVlcRoot = std::filesystem::path("definitely-missing-libvlc-runtime");
    options.allowAdjacentRuntimeSearch = false;

    std::unique_ptr<saors::AudioBackend> backend;
    REQUIRE_NOTHROW(backend = saors::createConfiguredAudioBackend(options));
    REQUIRE(backend);
    CHECK(std::string(backend->name()) == "null");
}

#if SAORS_HAS_LIBVLC
TEST_CASE("Factory selects a supplied complete libVLC runtime") {
    saors::AudioBackendFactoryOptions options;
    options.requested = saors::RequestedAudioBackend::libVlc;
    options.libVlcRoot = SAORS_TEST_LIBVLC_ROOT;
    options.allowAdjacentRuntimeSearch = false;

    const auto backend = saors::createConfiguredAudioBackend(options);

    REQUIRE(backend);
    CHECK(std::string(backend->name()) == "libVLC");
}
#endif
