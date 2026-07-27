#include "saors_gta3/GameIntegration.hpp"

#include "saors_gta3/Logger.hpp"

namespace saors {

GameIntegration::GameIntegration() {
#if SAORS_HAS_EXECUTABLE_FINGERPRINTING
    registry_ = &defaultExecutableProfileRegistry();
#endif
}

GameIntegration::GameIntegration(const ExecutableProfileRegistry& registry)
    : registry_(&registry) {}

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
    if (match_.exact() && match_.id == ExecutableProfileId::gta3_10_us_candidate) {
        detectedVersion_ = ExecutableVersion::gta3_10_us_unmapped;
    }
    return detectedVersion_;
}

bool GameIntegration::installHooks() {
#if defined(SAORS_ENABLE_EXPERIMENTAL_HOOKS)
    Logger::warning("Experimental hooks were requested, but no verified executable map exists; "
                    "hooks remain disabled");
#endif
    Logger::info("Hooks: disabled");
    hooksInstalled_ = false;
    return false;
}

void GameIntegration::removeHooks() noexcept {
    hooksInstalled_ = false;
}

int GameIntegration::currentStation() const noexcept {
    return -1;
}

bool GameIntegration::isPlayerInVehicle() const noexcept {
    return false;
}

bool GameIntegration::isPauseMenuActive() const noexcept {
    return false;
}

float GameIntegration::radioVolume() const noexcept {
    return 1.0F;
}

std::string GameIntegration::detectedExecutableDescription() const {
    switch (detectedVersion_) {
    case ExecutableVersion::gta3_10_us_unmapped:
        return "GTA III 1.0 US candidate";
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

bool GameIntegration::fileFingerprintMatch() const noexcept {
    return match_.fileFingerprintMatch;
}

bool GameIntegration::textFingerprintMatch() const noexcept {
    return match_.textFingerprintMatch;
}

} // namespace saors
