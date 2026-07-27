#include "saors_gta3/AudioBackend.hpp"
#include "saors_gta3/HttpClient.hpp"
#include "saors_gta3/PlaylistResolver.hpp"
#include "saors_gta3/StreamManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <deque>
#include <memory>
#include <string>

namespace {

class RecordingBackend final : public saors::AudioBackend {
  public:
    bool open(const std::string& url) override {
        ++openCount;
        openedUrl = url;
        currentState = saors::AudioState::opening;
        return true;
    }

    bool play() override {
        currentState = saors::AudioState::playing;
        return true;
    }

    bool pause() override {
        currentState = saors::AudioState::paused;
        return true;
    }

    void stop() noexcept override {
        ++stopCount;
        currentState = saors::AudioState::stopped;
    }

    bool setVolume(const float newVolume) override {
        volume = newVolume;
        return true;
    }

    bool setBufferMilliseconds(const std::uint32_t milliseconds) override {
        buffer = milliseconds;
        return milliseconds > 0;
    }

    [[nodiscard]] saors::AudioState state() const noexcept override {
        return currentState;
    }

    [[nodiscard]] std::string lastError() const override {
        return {};
    }

    [[nodiscard]] const char* name() const noexcept override {
        return "recording";
    }

    int openCount{0};
    int stopCount{0};
    float volume{0.0F};
    std::uint32_t buffer{0};
    std::string openedUrl;
    saors::AudioState currentState{saors::AudioState::stopped};
};

class SequenceHttpClient final : public saors::HttpClient {
  public:
    saors::Result<saors::HttpResponse> get(const std::string&,
                                           const saors::HttpRequestOptions&) noexcept override {
        if (responses.empty()) {
            return saors::Result<saors::HttpResponse>::fail("unexpected fake HTTP request");
        }
        auto result = std::move(responses.front());
        responses.pop_front();
        return saors::Result<saors::HttpResponse>::ok(std::move(result));
    }

    std::deque<saors::HttpResponse> responses;
};

saors::HttpResponse httpResponse(const std::string& finalUrl, const std::string& contentType,
                                 const std::string& body = {}) {
    saors::HttpResponse result;
    result.statusCode = 200;
    result.finalUrl = finalUrl;
    result.contentType = contentType;
    result.body = body;
    return result;
}

} // namespace

TEST_CASE("Stream manager replaces the active stream instead of stacking streams") {
    auto backend = std::make_unique<RecordingBackend>();
    auto* recording = backend.get();
    saors::StreamManager manager(std::move(backend));

    REQUIRE(manager.setBufferMilliseconds(4500));
    REQUIRE(manager.start("https://one.example.com/live", 0.5F));
    REQUIRE(manager.start("https://two.example.com/live", 0.75F));

    CHECK(recording->openCount == 2);
    CHECK(recording->stopCount == 2);
    CHECK(recording->openedUrl == "https://two.example.com/live");
    CHECK(recording->volume == 0.75F);
    CHECK(recording->buffer == 4500);
    CHECK(manager.currentUrl() == "https://two.example.com/live");
    CHECK(manager.state() == saors::AudioState::playing);
}

TEST_CASE("Null backend fails explicitly without retaining an active URL") {
    saors::StreamManager manager(saors::createNullAudioBackend());

    CHECK_FALSE(manager.start("https://radio.example.com/live", 1.0F));
    CHECK(manager.currentUrl().empty());
    CHECK_FALSE(manager.lastError().empty());
    CHECK(manager.state() == saors::AudioState::stopped);
    CHECK(std::string(manager.backendName()) == "null");
}

TEST_CASE("Stream manager re-resolves a configured playlist during reconnect") {
    auto backend = std::make_unique<RecordingBackend>();
    auto* recording = backend.get();
    auto httpClient = std::make_shared<SequenceHttpClient>();
    httpClient->responses.push_back(httpResponse("https://radio.example.com/list.m3u",
                                                 "audio/x-mpegurl",
                                                 "https://media.example.com/first\n"));
    httpClient->responses.push_back(httpResponse("https://media.example.com/first", "audio/aac"));
    httpClient->responses.push_back(httpResponse("https://radio.example.com/list.m3u",
                                                 "audio/x-mpegurl",
                                                 "https://media.example.com/second\n"));
    httpClient->responses.push_back(httpResponse("https://media.example.com/second", "audio/aac"));
    auto resolver = std::make_shared<saors::PlaylistResolver>(httpClient);
    saors::StreamManager manager(std::move(backend), resolver);

    REQUIRE(manager.startConfiguredUrl("https://radio.example.com/list.m3u", 0.5F));
    CHECK(recording->openedUrl == "https://media.example.com/first");
    REQUIRE(manager.reconnect());

    CHECK(recording->openCount == 2);
    CHECK(recording->openedUrl == "https://media.example.com/second");
    CHECK(manager.currentUrl() == "https://media.example.com/second");
    CHECK(manager.configuredUrl() == "https://radio.example.com/list.m3u");
    REQUIRE(manager.lastResolution().has_value());
    CHECK(manager.lastResolution()->selectedEntryIndex == 0);
}
