#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace saors {

enum class AudioState {
    stopped,
    opening,
    playing,
    paused,
    error,
};

class AudioBackend {
  public:
    virtual ~AudioBackend() = default;

    virtual bool open(const std::string& url) = 0;
    virtual bool play() = 0;
    virtual bool pause() = 0;
    virtual void stop() noexcept = 0;
    virtual bool setVolume(float volume) = 0;
    virtual bool setBufferMilliseconds(std::uint32_t milliseconds) = 0;

    [[nodiscard]] virtual AudioState state() const noexcept = 0;
    [[nodiscard]] virtual std::string lastError() const = 0;
    [[nodiscard]] virtual const char* name() const noexcept = 0;
};

[[nodiscard]] std::unique_ptr<AudioBackend> createNullAudioBackend();

} // namespace saors
