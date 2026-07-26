#include "saors_gta3/AudioBackend.hpp"
#include "saors_gta3/StreamManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
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
