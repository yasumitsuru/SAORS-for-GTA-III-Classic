#pragma once

#include "saors_gta3/AudioBackend.hpp"
#include "saors_gta3/PlaylistResolver.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace saors {

class StreamManager {
  public:
    explicit StreamManager(std::unique_ptr<AudioBackend> backend,
                           std::shared_ptr<PlaylistResolver> resolver = {});

    bool start(const std::string& url, float volume);
    bool startConfiguredUrl(const std::string& configuredUrl, float volume,
                            const ResolveOptions& options = {});
    bool pause();
    bool resume();
    void stop() noexcept;
    bool setVolume(float volume);
    bool reconnect();
    bool setBufferMilliseconds(std::uint32_t milliseconds);

    [[nodiscard]] AudioState state() const noexcept;
    [[nodiscard]] std::string lastError() const;
    [[nodiscard]] std::string currentUrl() const;
    [[nodiscard]] std::string configuredUrl() const;
    [[nodiscard]] std::optional<ResolvedStream> lastResolution() const;
    [[nodiscard]] std::uint32_t bufferMilliseconds() const noexcept;
    [[nodiscard]] const char* backendName() const noexcept;

  private:
    bool startMediaLocked(const std::string& url);

    mutable std::mutex mutex_;
    std::unique_ptr<AudioBackend> backend_;
    std::shared_ptr<PlaylistResolver> resolver_;
    std::string currentUrl_;
    std::string configuredUrl_;
    std::optional<ResolvedStream> lastResolution_;
    ResolveOptions resolveOptions_;
    float volume_{1.0F};
    std::uint32_t bufferMilliseconds_{3000};
    std::string lastError_;
};

} // namespace saors
