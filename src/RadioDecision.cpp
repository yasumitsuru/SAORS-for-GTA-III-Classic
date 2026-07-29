#include "saors_gta3/RadioDecision.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace saors {
namespace {

bool sameOptionalFloat(const std::optional<float>& left,
                       const std::optional<float>& right) noexcept {
    constexpr float comparisonTolerance = 0.0001F;
    if (left.has_value() != right.has_value()) {
        return false;
    }
    return !left || std::fabs(*left - *right) <= comparisonTolerance;
}

void clearSimulatedActivity(SimulatedRadioState& state) {
    state.active = false;
    state.paused = false;
    state.bindingKind = RadioStationBindingKind::none;
    state.rawStation.reset();
    state.stationIdentity.reset();
    state.stationKey.reset();
    state.preferenceVolume.reset();
}

bool isBlockingReason(const RadioDecisionReason reason) noexcept {
    switch (reason) {
    case RadioDecisionReason::projectDisabled:
    case RadioDecisionReason::controllerDisabled:
    case RadioDecisionReason::observerUnavailable:
    case RadioDecisionReason::gameNotReady:
    case RadioDecisionReason::pauseStateUnavailable:
    case RadioDecisionReason::vehicleStateUnavailable:
    case RadioDecisionReason::playerOnFoot:
    case RadioDecisionReason::stationUnavailable:
    case RadioDecisionReason::stationRawInvalid:
    case RadioDecisionReason::stationProfileUnsupported:
    case RadioDecisionReason::stationIdentityUnknown:
    case RadioDecisionReason::stationIdentityNotBound:
    case RadioDecisionReason::stationNotBound:
    case RadioDecisionReason::stationDisabled:
    case RadioDecisionReason::stationUrlEmpty:
    case RadioDecisionReason::volumeUnavailable:
        return true;
    case RadioDecisionReason::unchanged:
    case RadioDecisionReason::snapshotOutOfOrder:
    case RadioDecisionReason::paused:
    case RadioDecisionReason::ready:
        return false;
    }
    return false;
}

} // namespace

const char* radioActionKindName(const RadioActionKind action) noexcept {
    switch (action) {
    case RadioActionKind::none:
        return "none";
    case RadioActionKind::wouldStart:
        return "would-start";
    case RadioActionKind::wouldPause:
        return "would-pause";
    case RadioActionKind::wouldResume:
        return "would-resume";
    case RadioActionKind::wouldSwitch:
        return "would-switch";
    case RadioActionKind::wouldSetVolume:
        return "would-set-volume";
    case RadioActionKind::wouldStop:
        return "would-stop";
    }
    return "none";
}

const char* radioStationBindingKindName(const RadioStationBindingKind bindingKind) noexcept {
    switch (bindingKind) {
    case RadioStationBindingKind::none:
        return "none";
    case RadioStationBindingKind::raw:
        return "raw";
    case RadioStationBindingKind::identity:
        return "identity";
    }
    return "none";
}

const char* radioDecisionReasonName(const RadioDecisionReason reason) noexcept {
    switch (reason) {
    case RadioDecisionReason::unchanged:
        return "unchanged";
    case RadioDecisionReason::projectDisabled:
        return "project-disabled";
    case RadioDecisionReason::controllerDisabled:
        return "controller-disabled";
    case RadioDecisionReason::observerUnavailable:
        return "observer-unavailable";
    case RadioDecisionReason::gameNotReady:
        return "game-not-ready";
    case RadioDecisionReason::pauseStateUnavailable:
        return "pause-state-unavailable";
    case RadioDecisionReason::vehicleStateUnavailable:
        return "vehicle-state-unavailable";
    case RadioDecisionReason::playerOnFoot:
        return "player-on-foot";
    case RadioDecisionReason::stationUnavailable:
        return "station-unavailable";
    case RadioDecisionReason::stationRawInvalid:
        return "station-raw-invalid";
    case RadioDecisionReason::stationProfileUnsupported:
        return "station-profile-unsupported";
    case RadioDecisionReason::stationIdentityUnknown:
        return "station-identity-unknown";
    case RadioDecisionReason::stationIdentityNotBound:
        return "station-identity-not-bound";
    case RadioDecisionReason::stationNotBound:
        return "station-not-bound";
    case RadioDecisionReason::stationDisabled:
        return "station-disabled";
    case RadioDecisionReason::stationUrlEmpty:
        return "station-url-empty";
    case RadioDecisionReason::volumeUnavailable:
        return "volume-unavailable";
    case RadioDecisionReason::snapshotOutOfOrder:
        return "snapshot-out-of-order";
    case RadioDecisionReason::paused:
        return "paused";
    case RadioDecisionReason::ready:
        return "ready";
    }
    return "unchanged";
}

bool isPublicStationKeySafe(const std::string& stationKey) noexcept {
    constexpr std::size_t maximumPublicKeyLength = 48U;
    return !stationKey.empty() && stationKey.size() <= maximumPublicKeyLength &&
           std::all_of(stationKey.begin(), stationKey.end(), [](const char character) {
               const auto byte = static_cast<unsigned char>(character);
               return std::isalnum(byte) != 0 || character == '_' || character == '-';
           });
}

std::string sanitizedStationKey(const std::string& stationKey,
                                const std::optional<int> rawStation) {
    if (isPublicStationKeySafe(stationKey)) {
        return stationKey;
    }
    return rawStation ? "StationRaw" + std::to_string(*rawStation) : "Station";
}

bool sameRadioActionTransition(const RadioActionPlan& left, const RadioActionPlan& right) noexcept {
    return left.action == right.action && left.reason == right.reason &&
           left.bindingKind == right.bindingKind && left.rawStation == right.rawStation &&
           left.stationIdentity == right.stationIdentity && left.stationKey == right.stationKey &&
           sameOptionalFloat(left.preferenceVolume, right.preferenceVolume) &&
           left.onlineAudioWouldBeActive == right.onlineAudioWouldBeActive;
}

void applyRadioActionPlan(const RadioActionPlan& plan, SimulatedRadioState& state) noexcept {
    try {
        if (state.lastSnapshotSequence != 0 &&
            plan.snapshotSequence <= state.lastSnapshotSequence) {
            return;
        }
        state.lastSnapshotSequence = plan.snapshotSequence;

        switch (plan.action) {
        case RadioActionKind::wouldStart:
        case RadioActionKind::wouldSwitch:
            state.active = true;
            state.paused = false;
            state.bindingKind = plan.bindingKind;
            state.rawStation = plan.rawStation;
            state.stationIdentity = plan.stationIdentity;
            state.stationKey = plan.stationKey;
            state.preferenceVolume = plan.preferenceVolume;
            break;
        case RadioActionKind::wouldPause:
            if (state.active) {
                state.paused = true;
            }
            break;
        case RadioActionKind::wouldResume:
            if (state.active) {
                state.paused = false;
            }
            break;
        case RadioActionKind::wouldSetVolume:
            if (state.active) {
                state.preferenceVolume = plan.preferenceVolume;
            }
            break;
        case RadioActionKind::wouldStop:
            clearSimulatedActivity(state);
            break;
        case RadioActionKind::none:
            if (!state.active && isBlockingReason(plan.reason)) {
                clearSimulatedActivity(state);
            }
            break;
        }
    } catch (...) {
    }
}

} // namespace saors
