#include "saors_gta3/GameIntegration.hpp"

#include "saors_gta3/GameAddressProfile.hpp"
#include "saors_gta3/Logger.hpp"

namespace saors {

GameIntegration::GameIntegration() {
#if SAORS_HAS_EXECUTABLE_FINGERPRINTING
    registry_ = &defaultExecutableProfileRegistry();
#endif
}

GameIntegration::GameIntegration(const ExecutableProfileRegistry& registry)
    : registry_(&registry) {}

GameIntegration::~GameIntegration() {
    removeHooks();
}

ExecutableVersion
GameIntegration::detectExecutableVersion(const ExecutableFingerprint& fingerprint) {
#if SAORS_HAS_EXECUTABLE_FINGERPRINTING
    if (registry_ != nullptr) {
        match_ = registry_->match(fingerprint);
    }
#else
    static_cast<void>(fingerprint);
#endif
    detectedVersion_ = ExecutableVersion::unsupported;
    if (match_.exact()) {
        switch (match_.id) {
        case ExecutableProfileId::gta3_classic_local_candidate:
            detectedVersion_ = ExecutableVersion::gta3_classic_local_candidate;
            break;
        case ExecutableProfileId::gta3_10_us_candidate:
            detectedVersion_ = ExecutableVersion::gta3_10_us_unmapped;
            break;
        case ExecutableProfileId::unsupported:
            break;
        }
    }
    return detectedVersion_;
}

void GameIntegration::configureObserver(const bool enabled, const bool dryRun,
                                        const bool logStateTransitions) noexcept {
    observerEnabled_ = enabled;
    observerDryRun_ = dryRun;
    logStateTransitions_ = logStateTransitions;
}

void GameIntegration::setSnapshotListener(GameStateSnapshotListener* listener) noexcept {
    snapshotListener_ = listener;
    if (observer_) {
        observer_->setSnapshotListener(listener);
    }
}

bool GameIntegration::installHooks() {
    hooksInstalled_ = false;
    observerResult_ = {};

    if (!observerEnabled_ && !observerDryRun_) {
        Logger::info("Observer: unavailable");
        Logger::info("Hooks: disabled");
        return false;
    }

#if SAORS_HAS_EXPERIMENTAL_GAME_OBSERVER
    if (!match_.exact() || !match_.verificationStatus) {
        Logger::warning("Observer: unavailable");
        Logger::info("Hooks: disabled");
        return false;
    }

    const auto* addressProfile = findGameAddressProfile(match_.id, *match_.verificationStatus);
    if (addressProfile == nullptr) {
        Logger::warning("Observer: unavailable");
        Logger::info("Hooks: disabled");
        return false;
    }
    Logger::info("Address profile: gta3_classic_local_candidate");

    observer_ = createExperimentalGameObserver(*addressProfile, logStateTransitions_);
    observer_->setSnapshotListener(snapshotListener_);
    const bool dryRun = observerDryRun_ || !observerEnabled_;
    observerResult_ = observer_->start(dryRun);
    if (observerResult_.status == ObserverInstallStatus::dryRunCompatible) {
        Logger::info("Observer dry-run: compatible");
        Logger::info("Expected bytes: matched");
        Logger::info("Hook writes performed: false");
        Logger::info("Hooks: disabled");
        return false;
    }
    if (observerResult_.status == ObserverInstallStatus::dryRunIncompatible) {
        Logger::warning("Observer dry-run: incompatible");
        Logger::warning("Expected bytes: not matched");
        if (observerResult_.failedSymbol) {
            Logger::warning(std::string("Observer validation failed: ") +
                            gameSymbolName(*observerResult_.failedSymbol));
        }
        Logger::info("Hook writes performed: false");
        Logger::info("Hooks: disabled");
        return false;
    }
    if (observerResult_.status == ObserverInstallStatus::installed) {
        hooksInstalled_ = true;
        Logger::info("Observer: installed");
        Logger::info("Expected bytes: matched");
        Logger::info("Hook writes performed: true");
        return true;
    }

    Logger::warning("Observer: failed");
    Logger::info(std::string("Hook writes performed: ") +
                 (observerResult_.hookWritesPerformed ? "true" : "false"));
    Logger::info("Hooks: disabled");
    return false;
#else
    Logger::warning("Observer: unavailable in this build");
    Logger::info("Hooks: disabled");
    return false;
#endif
}

void GameIntegration::removeHooks() noexcept {
    if (observer_) {
        observer_->setSnapshotListener(nullptr);
        static_cast<void>(observer_->stop());
    }
    snapshotListener_ = nullptr;
    hooksInstalled_ = false;
}

GameStateSnapshot GameIntegration::gameStateSnapshot() const noexcept {
    return observer_ ? observer_->snapshot() : GameStateSnapshot{};
}

ObserverInstallResult GameIntegration::observerInstallResult() const noexcept {
    return observerResult_;
}

std::string GameIntegration::observerStatusDescription() const {
    return observerInstallStatusName(observerResult_.status);
}

ExecutableProfileId GameIntegration::detectedExecutableProfile() const noexcept {
#if SAORS_HAS_EXECUTABLE_FINGERPRINTING
    return match_.exact() ? match_.id : ExecutableProfileId::unsupported;
#else
    return ExecutableProfileId::unsupported;
#endif
}

std::string GameIntegration::detectedExecutableDescription() const {
    switch (detectedVersion_) {
    case ExecutableVersion::gta3_classic_local_candidate:
    case ExecutableVersion::gta3_10_us_unmapped:
        return match_.profileName;
    case ExecutableVersion::unsupported:
        return "unsupported";
    }
    return "unsupported";
}

std::string GameIntegration::fingerprintStatusDescription() const {
#if SAORS_HAS_EXECUTABLE_FINGERPRINTING
    return executableFingerprintMatchKindName(match_.kind);
#else
    return "no exact profile match";
#endif
}

std::string GameIntegration::verificationStatusDescription() const {
#if SAORS_HAS_EXECUTABLE_FINGERPRINTING
    if (match_.verificationStatus) {
        return executableVerificationStatusName(*match_.verificationStatus);
    }
#endif
    return "not registered";
}

bool GameIntegration::fileFingerprintMatch() const noexcept {
    return match_.fileFingerprintMatch;
}

bool GameIntegration::textFingerprintMatch() const noexcept {
    return match_.textFingerprintMatch;
}

} // namespace saors
