#include "saors_gta3/AudioBackend.hpp"

#include "saors_gta3/PlaylistParser.hpp"

#include <cmath>
#include <utility>

namespace saors {
namespace {

class NullAudioBackend final : public AudioBackend {
  public:
    bool open(const std::string& url) override {
        if (!PlaylistParser::isSupportedUrl(url)) {
            lastError_ = "stream URL must be an absolute HTTP(S) URL";
        } else {
            lastError_ = "audio backend is not configured";
        }
        state_ = AudioState::error;
        return false;
    }

    bool play() override {
        lastError_ = "audio backend is not configured";
        state_ = AudioState::error;
        return false;
    }

    bool pause() override {
        lastError_ = "audio backend is not configured";
        state_ = AudioState::error;
        return false;
    }

    void stop() noexcept override {
        state_ = AudioState::stopped;
        lastError_.clear();
    }

    bool setVolume(const float volume) override {
        if (!std::isfinite(volume) || volume < 0.0F || volume > 1.0F) {
            lastError_ = "volume must be between 0.0 and 1.0";
            state_ = AudioState::error;
            return false;
        }
        return true;
    }

    bool setBufferMilliseconds(const std::uint32_t milliseconds) override {
        if (milliseconds == 0) {
            lastError_ = "buffer duration must be greater than zero";
            state_ = AudioState::error;
            return false;
        }
        return true;
    }

    [[nodiscard]] AudioState state() const noexcept override {
        return state_;
    }

    [[nodiscard]] std::string lastError() const override {
        return lastError_;
    }

    [[nodiscard]] const char* name() const noexcept override {
        return "null";
    }

  private:
    AudioState state_{AudioState::stopped};
    std::string lastError_;
};

} // namespace

std::unique_ptr<AudioBackend> createNullAudioBackend() {
    return std::make_unique<NullAudioBackend>();
}

} // namespace saors
