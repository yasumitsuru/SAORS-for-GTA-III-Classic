#include "saors_gta3/GameIntegration.hpp"

#include "saors_gta3/Logger.hpp"

namespace saors {

ExecutableVersion GameIntegration::detectExecutableVersion() {
    // No hashes or addresses are accepted until independently verified against a
    // legally obtained GTA III 1.0 US executable. Filename/version-resource checks
    // alone are not sufficient evidence for installing binary hooks.
    detectedVersion_ = ExecutableVersion::unsupported;
    return detectedVersion_;
}

bool GameIntegration::installHooks() {
#if defined(SAORS_ENABLE_EXPERIMENTAL_HOOKS)
    Logger::warning("Experimental hooks were requested, but no verified executable map exists; "
                    "hooks remain disabled");
#else
    Logger::info("Game hooks are disabled (safe default)");
#endif
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
        return "GTA III 1.0 US candidate (not mapped)";
    case ExecutableVersion::unsupported:
        return "unsupported or not yet mapped";
    }
    return "unsupported or not yet mapped";
}

} // namespace saors
