#include "saors_gta3/AudioBackend.hpp"

#if SAORS_HAS_LIBVLC
#include "saors_gta3/LibVlcAudioBackend.hpp"
#endif

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <string>

TEST_CASE("Null backend validates volume bounds and repeated stop") {
    auto backend = saors::createNullAudioBackend();

    CHECK(backend->setVolume(0.0F));
    CHECK(backend->setVolume(1.0F));
    CHECK_FALSE(backend->setVolume(-0.01F));
    CHECK_FALSE(backend->setVolume(1.01F));
    REQUIRE_NOTHROW(backend->stop());
    REQUIRE_NOTHROW(backend->stop());
    CHECK(backend->state() == saors::AudioState::stopped);
}

TEST_CASE("Null backend rejects play without open and invalid URLs") {
    auto backend = saors::createNullAudioBackend();

    CHECK_FALSE(backend->play());
    CHECK(backend->state() == saors::AudioState::error);
    CHECK_FALSE(backend->open("file:///private/stream.mp3"));
    CHECK(backend->lastError().find("HTTP(S)") != std::string::npos);
}

#if SAORS_HAS_LIBVLC
namespace {

std::unique_ptr<saors::LibVlcAudioBackend> createLibVlcBackend() {
    std::string error;
    auto backend = saors::LibVlcAudioBackend::tryCreate(
        std::filesystem::path(SAORS_TEST_LIBVLC_ROOT), false, error);
    INFO(error);
    REQUIRE(backend);
    return backend;
}

} // namespace

TEST_CASE("libVLC backend rejects play without an opened stream") {
    auto backend = createLibVlcBackend();

    CHECK_FALSE(backend->play());
    CHECK(backend->state() == saors::AudioState::error);
    CHECK_FALSE(backend->lastError().empty());
}

TEST_CASE("libVLC backend validates volume and buffer bounds") {
    auto backend = createLibVlcBackend();

    CHECK(backend->setVolume(0.0F));
    CHECK(backend->setVolume(1.0F));
    CHECK_FALSE(backend->setVolume(-0.01F));
    CHECK_FALSE(backend->setVolume(1.01F));
    CHECK(backend->setBufferMilliseconds(1));
    CHECK_FALSE(backend->setBufferMilliseconds(0));
}

TEST_CASE("libVLC backend validates URLs without starting network activity") {
    auto backend = createLibVlcBackend();

    CHECK_FALSE(backend->open("relative/stream.mp3"));
    CHECK(backend->state() == saors::AudioState::error);
    CHECK(backend->lastError().find("HTTP(S)") != std::string::npos);
}

TEST_CASE("libVLC backend supports offline lifecycle transitions") {
    auto backend = createLibVlcBackend();

    REQUIRE(backend->open("https://example.invalid/stream"));
    CHECK(backend->state() == saors::AudioState::opening);
    REQUIRE(backend->pause());
    CHECK(backend->state() == saors::AudioState::paused);
    REQUIRE(backend->play());
    CHECK(backend->state() == saors::AudioState::opening);
    REQUIRE_NOTHROW(backend->stop());
    REQUIRE_NOTHROW(backend->stop());
    CHECK(backend->state() == saors::AudioState::stopped);
}

TEST_CASE("libVLC backend shutdown is safe while in an error state") {
    REQUIRE_NOTHROW([] {
        auto backend = createLibVlcBackend();
        static_cast<void>(backend->play());
    }());
}
#endif
