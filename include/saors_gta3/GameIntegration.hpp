#pragma once

#include "saors_gta3/ExecutableFingerprint.hpp"
#include "saors_gta3/ExecutableProfileRegistry.hpp"
#include "saors_gta3/GameObserver.hpp"

#include <memory>
#include <string>

namespace saors {

enum class ExecutableVersion {
    unsupported,
    gta3_classic_local_candidate,
    gta3_10_us_unmapped,
};

class GameIntegration {
  public:
    GameIntegration();
    explicit GameIntegration(const ExecutableProfileRegistry& registry);
    ~GameIntegration();

    GameIntegration(const GameIntegration&) = delete;
    GameIntegration& operator=(const GameIntegration&) = delete;

    [[nodiscard]] ExecutableVersion
    detectExecutableVersion(const ExecutableFingerprint& fingerprint);
    void configureObserver(bool enabled, bool dryRun, bool logStateTransitions) noexcept;
    void setSnapshotListener(GameStateSnapshotListener* listener) noexcept;
    [[nodiscard]] bool installHooks();
    void removeHooks() noexcept;

    [[nodiscard]] GameStateSnapshot gameStateSnapshot() const noexcept;
    [[nodiscard]] ObserverInstallResult observerInstallResult() const noexcept;
    [[nodiscard]] std::string observerStatusDescription() const;
    [[nodiscard]] std::string detectedExecutableDescription() const;
    [[nodiscard]] std::string fingerprintStatusDescription() const;
    [[nodiscard]] std::string verificationStatusDescription() const;
    [[nodiscard]] bool fileFingerprintMatch() const noexcept;
    [[nodiscard]] bool textFingerprintMatch() const noexcept;

  private:
    const ExecutableProfileRegistry* registry_{nullptr};
    ExecutableProfileMatch match_;
    ExecutableVersion detectedVersion_{ExecutableVersion::unsupported};
    std::unique_ptr<GameObserver> observer_;
    ObserverInstallResult observerResult_;
    bool observerEnabled_{false};
    bool observerDryRun_{true};
    bool logStateTransitions_{true};
    GameStateSnapshotListener* snapshotListener_{nullptr};
    bool hooksInstalled_{false};
};

} // namespace saors
