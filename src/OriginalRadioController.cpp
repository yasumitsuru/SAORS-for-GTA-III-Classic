#include "saors_gta3/OriginalRadioController.hpp"

#include <memory>

namespace saors {

bool NullOriginalRadioController::available() const noexcept {
    return false;
}

bool NullOriginalRadioController::mute() noexcept {
    return false;
}

bool NullOriginalRadioController::restore() noexcept {
    return true;
}

bool NullOriginalRadioController::muted() const noexcept {
    return false;
}

bool shouldEnableOriginalRadioSuppression(const OriginalRadioSuppressionPolicy& policy,
                                          const OriginalRadioController& controller) noexcept {
    return policy.buildEnabled && policy.gameplayExecutorEnabled && policy.runtimeEnabled &&
           policy.executableProfile == ExecutableProfileId::gta3_classic_local_candidate &&
           controller.available();
}

std::unique_ptr<OriginalRadioController>
createOriginalRadioController(const ExecutableProfileId executableProfile) {
    static_cast<void>(executableProfile);

    // The pinned plugin-sdk names possible music-volume calls, but this project
    // has no validated entry bytes or proven radio-only restore semantics for
    // them. Until that evidence exists, every profile intentionally fails closed.
    return std::make_unique<NullOriginalRadioController>();
}

} // namespace saors
