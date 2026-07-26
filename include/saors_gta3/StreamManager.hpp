#pragma once

#include "saors_gta3/AudioBackend.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace saors {

class StreamManager {
  public:
    explicit StreamManager(std::unique_ptr<AudioBackend> backend);

    bool start(const std::string& url, float volume);
    bool pause();
    bool resume();
    void stop() noexcept;
    bool reconnect();
    bool setBufferMilliseconds(std::uint32_t milliseconds);

    [[nodiscard]] AudioState state() const noexcept;
    [[nodiscard]] std::string lastError() const;
    [[nodiscard]] std::string currentUrl() const;
    [[nodiscard]] std::uint32_t bufferMilliseconds() const noexcept;
    [[nodiscard]] const char* backendName() const noexcept;

  private:
    mutable std::mutex mutex_;
    std::unique_ptr<AudioBackend> backend_;
    std::string currentUrl_;
    float volume_{1.0F};
    std::uint32_t bufferMilliseconds_{3000};
    std::string lastError_;
};

} // namespace saors
