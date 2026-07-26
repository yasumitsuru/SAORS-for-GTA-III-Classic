#pragma once

#include <string>

namespace saors {

enum class ExecutableVersion {
    unsupported,
    gta3_10_us_unmapped,
};

class GameIntegration {
  public:
    [[nodiscard]] ExecutableVersion detectExecutableVersion();
    [[nodiscard]] bool installHooks();
    void removeHooks() noexcept;

    [[nodiscard]] int currentStation() const noexcept;
    [[nodiscard]] bool isPlayerInVehicle() const noexcept;
    [[nodiscard]] bool isPauseMenuActive() const noexcept;
    [[nodiscard]] float radioVolume() const noexcept;
    [[nodiscard]] std::string detectedExecutableDescription() const;

  private:
    ExecutableVersion detectedVersion_{ExecutableVersion::unsupported};
    bool hooksInstalled_{false};
};

} // namespace saors
