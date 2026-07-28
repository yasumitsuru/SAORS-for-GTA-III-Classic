#pragma once

#include "saors_gta3/GameAddressProfile.hpp"
#include "saors_gta3/GameState.hpp"

#include <memory>
#include <optional>

namespace saors {

enum class ObserverInstallStatus {
    unavailable,
    dryRunCompatible,
    dryRunIncompatible,
    installed,
    failed,
};

struct ObserverInstallResult {
    ObserverInstallStatus status{ObserverInstallStatus::unavailable};
    std::optional<GameSymbolId> failedSymbol;
    bool expectedBytesMatched{false};
    bool hookWritesPerformed{false};
    bool rollbackSucceeded{true};
};

class GameObserver {
  public:
    virtual ~GameObserver() = default;
    virtual void setSnapshotListener(GameStateSnapshotListener* listener) noexcept = 0;
    [[nodiscard]] virtual ObserverInstallResult start(bool dryRun) noexcept = 0;
    [[nodiscard]] virtual bool stop() noexcept = 0;
    [[nodiscard]] virtual GameStateSnapshot snapshot() const noexcept = 0;
};

class UnavailableGameObserver final : public GameObserver {
  public:
    void setSnapshotListener(GameStateSnapshotListener* listener) noexcept override;
    [[nodiscard]] ObserverInstallResult start(bool dryRun) noexcept override;
    [[nodiscard]] bool stop() noexcept override;
    [[nodiscard]] GameStateSnapshot snapshot() const noexcept override;
};

template <typename OriginalCall, typename CaptureCall>
void dispatchGameObserverFrame(OriginalCall&& originalCall, CaptureCall&& captureCall) noexcept {
    try {
        originalCall();
    } catch (...) {
    }
    try {
        captureCall();
    } catch (...) {
    }
}

[[nodiscard]] std::unique_ptr<GameObserver>
createExperimentalGameObserver(const GameAddressProfile& profile, bool logStateTransitions);

[[nodiscard]] const char* observerInstallStatusName(ObserverInstallStatus status) noexcept;

} // namespace saors
