#pragma once

#include "saors_gta3/AudioBackend.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace saors {

class LibVlcAudioBackend final : public AudioBackend {
  public:
    ~LibVlcAudioBackend() override;

    LibVlcAudioBackend(const LibVlcAudioBackend&) = delete;
    LibVlcAudioBackend& operator=(const LibVlcAudioBackend&) = delete;

    [[nodiscard]] static std::unique_ptr<LibVlcAudioBackend>
    tryCreate(const std::filesystem::path& runtimeRoot, bool allowAdjacentRuntimeSearch,
              std::string& error) noexcept;

    bool open(const std::string& url) override;
    bool play() override;
    bool pause() override;
    void stop() noexcept override;
    bool setVolume(float volume) override;
    bool setBufferMilliseconds(std::uint32_t milliseconds) override;

    [[nodiscard]] AudioState state() const noexcept override;
    [[nodiscard]] std::string lastError() const override;
    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::string runtimeVersion() const;
    [[nodiscard]] std::filesystem::path runtimeRoot() const;

  private:
    struct Impl;

    explicit LibVlcAudioBackend(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace saors
