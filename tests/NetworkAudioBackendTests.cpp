#include "saors_gta3/LibVlcAudioBackend.hpp"
#include "saors_gta3/PlaylistParser.hpp"
#include "saors_gta3/UrlSanitizer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

namespace {

std::string testStreamUrl() {
#if defined(_WIN32) && defined(_MSC_VER)
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "SAORS_TEST_STREAM_URL") != 0 || value == nullptr) {
        return {};
    }
    const std::string result(value);
    std::free(value);
    return result;
#else
    const auto* value = std::getenv("SAORS_TEST_STREAM_URL");
    return value == nullptr ? std::string{} : std::string(value);
#endif
}

} // namespace

TEST_CASE("Supplied private stream reaches playback with libVLC") {
    const auto streamUrl = testStreamUrl();
    REQUIRE_FALSE(streamUrl.empty());
    REQUIRE(saors::PlaylistParser::isSupportedUrl(streamUrl));

    std::string error;
    auto backend = saors::LibVlcAudioBackend::tryCreate(
        std::filesystem::path(SAORS_TEST_LIBVLC_ROOT), false, error);
    INFO(saors::sanitizeTextForLogging(error));
    REQUIRE(backend);
    REQUIRE(backend->open(streamUrl));
    REQUIRE(backend->play());

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < deadline &&
           backend->state() == saors::AudioState::opening) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    INFO(saors::sanitizeTextForLogging(backend->lastError()));
    CHECK(backend->state() == saors::AudioState::playing);
    backend->stop();
}
