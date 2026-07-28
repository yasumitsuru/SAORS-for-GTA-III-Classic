#include "saors_gta3/GameObserver.hpp"

#include <memory>

namespace saors {

ObserverInstallResult UnavailableGameObserver::start(const bool dryRun) noexcept {
    static_cast<void>(dryRun);
    return {};
}

bool UnavailableGameObserver::stop() noexcept {
    return true;
}

GameStateSnapshot UnavailableGameObserver::snapshot() const noexcept {
    return {};
}

#if !SAORS_HAS_EXPERIMENTAL_GAME_OBSERVER
std::unique_ptr<GameObserver> createExperimentalGameObserver(const GameAddressProfile& profile,
                                                             const bool logStateTransitions) {
    static_cast<void>(profile);
    static_cast<void>(logStateTransitions);
    return std::make_unique<UnavailableGameObserver>();
}
#endif

const char* observerInstallStatusName(const ObserverInstallStatus status) noexcept {
    switch (status) {
    case ObserverInstallStatus::unavailable:
        return "unavailable";
    case ObserverInstallStatus::dryRunCompatible:
        return "dry-run compatible";
    case ObserverInstallStatus::dryRunIncompatible:
        return "dry-run incompatible";
    case ObserverInstallStatus::installed:
        return "installed";
    case ObserverInstallStatus::failed:
        return "failed";
    }
    return "unavailable";
}

} // namespace saors
