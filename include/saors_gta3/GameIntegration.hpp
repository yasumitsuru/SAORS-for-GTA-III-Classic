#pragma once

#include "saors_gta3/ExecutableFingerprint.hpp"
#include "saors_gta3/ExecutableProfileRegistry.hpp"

#include <string>

namespace saors {

enum class ExecutableVersion {
    unsupported,
    gta3_10_us_unmapped,
};

class GameIntegration {
  public:
    GameIntegration();
    explicit GameIntegration(const ExecutableProfileRegistry& registry);

    [[nodiscard]] ExecutableVersion
    detectExecutableVersion(const ExecutableFingerprint& fingerprint);
    [[nodiscard]] bool installHooks();
    void removeHooks() noexcept;

    [[nodiscard]] int currentStation() const noexcept;
    [[nodiscard]] bool isPlayerInVehicle() const noexcept;
    [[nodiscard]] bool isPauseMenuActive() const noexcept;
    [[nodiscard]] float radioVolume() const noexcept;
    [[nodiscard]] std::string detectedExecutableDescription() const;
    [[nodiscard]] std::string fingerprintStatusDescription() const;
    [[nodiscard]] bool fileFingerprintMatch() const noexcept;
    [[nodiscard]] bool textFingerprintMatch() const noexcept;

  private:
    const ExecutableProfileRegistry* registry_{nullptr};
    ExecutableProfileMatch match_;
    ExecutableVersion detectedVersion_{ExecutableVersion::unsupported};
    bool hooksInstalled_{false};
};

} // namespace saors
