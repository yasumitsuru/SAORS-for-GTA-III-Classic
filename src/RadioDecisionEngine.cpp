#include "saors_gta3/RadioDecisionEngine.hpp"

#include <algorithm>
#include <cmath>

namespace saors {
namespace {

constexpr float preferenceVolumeTolerance = 0.01F;

using StationBinding = std::pair<const std::string, StationConfiguration>;

RadioActionPlan inactivePlan(const GameStateSnapshot& snapshot,
                             const SimulatedRadioState& previousState,
                             const RadioDecisionReason reason,
                             const std::optional<int> rawStation = std::nullopt,
                             const RadioStationBindingKind bindingKind =
                                 RadioStationBindingKind::none,
                             const std::optional<RadioStationIdentity> stationIdentity =
                                 std::nullopt) {
    RadioActionPlan plan;
    plan.action = previousState.active ? RadioActionKind::wouldStop : RadioActionKind::none;
    plan.reason = reason;
    plan.bindingKind = bindingKind;
    plan.snapshotSequence = snapshot.sequence;
    plan.rawStation = rawStation;
    plan.stationIdentity = stationIdentity;
    plan.onlineAudioWouldBeActive = false;
    return plan;
}

const StationBinding* findRawStationBinding(const ConfigurationData& configuration,
                                            const int rawStation) {
    const StationBinding* disabledBinding = nullptr;
    for (const auto& entry : configuration.stations) {
        if (!entry.second.gameStationRaw || *entry.second.gameStationRaw != rawStation) {
            continue;
        }
        if (entry.second.enabled) {
            return &entry;
        }
        if (disabledBinding == nullptr) {
            disabledBinding = &entry;
        }
    }
    return disabledBinding;
}

const StationBinding* findIdentityStationBinding(const ConfigurationData& configuration,
                                                 const RadioStationIdentity stationIdentity) {
    const StationBinding* disabledBinding = nullptr;
    for (const auto& entry : configuration.stations) {
        if (!entry.second.gameStationIdentity ||
            *entry.second.gameStationIdentity != stationIdentity) {
            continue;
        }
        if (entry.second.enabled) {
            return &entry;
        }
        if (disabledBinding == nullptr) {
            disabledBinding = &entry;
        }
    }
    return disabledBinding;
}

RadioActionPlan activePlanBase(const GameStateSnapshot& snapshot,
                               const SimulatedRadioState& previousState) {
    RadioActionPlan plan;
    plan.snapshotSequence = snapshot.sequence;
    plan.onlineAudioWouldBeActive = previousState.active;
    return plan;
}

} // namespace

RadioActionPlan
RadioDecisionEngine::evaluate(const GameStateSnapshot& snapshot,
                              const ConfigurationData& configuration,
                              const ExecutableProfileId executableProfile,
                              const RadioStationResolver& resolver,
                              const SimulatedRadioState& previousState) const noexcept {
    try {
        if (previousState.lastSnapshotSequence != 0 &&
            snapshot.sequence <= previousState.lastSnapshotSequence) {
            auto plan = activePlanBase(snapshot, previousState);
            plan.reason = RadioDecisionReason::snapshotOutOfOrder;
            return plan;
        }
        if (!configuration.general.enabled) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::projectDisabled);
        }
        if (!configuration.experimental.enableRadioController ||
            !configuration.experimental.radioControllerDryRun) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::controllerDisabled);
        }
        if (!snapshot.observerAvailable) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::observerUnavailable);
        }
        if (!snapshot.gameReady) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::gameNotReady);
        }
        if (!snapshot.pauseMenuActive) {
            return inactivePlan(snapshot, previousState,
                                RadioDecisionReason::pauseStateUnavailable);
        }
        if (!snapshot.playerInVehicle) {
            return inactivePlan(snapshot, previousState,
                                RadioDecisionReason::vehicleStateUnavailable);
        }
        if (!*snapshot.playerInVehicle) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::playerOnFoot);
        }
        if (!snapshot.radioStationRaw) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::stationUnavailable);
        }

        const int rawStation = *snapshot.radioStationRaw;
        if (rawStation < 0 || rawStation > 255) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::stationRawInvalid,
                                rawStation);
        }

        const StationBinding* binding = findRawStationBinding(configuration, rawStation);
        auto bindingKind = RadioStationBindingKind::none;
        std::optional<RadioStationIdentity> stationIdentity;

        if (binding != nullptr) {
            bindingKind = RadioStationBindingKind::raw;
        } else {
            const auto resolution = resolver.resolve(executableProfile, rawStation);
            switch (resolution.status) {
            case RadioStationResolutionStatus::invalidRaw:
                return inactivePlan(snapshot, previousState, RadioDecisionReason::stationRawInvalid,
                                    rawStation);
            case RadioStationResolutionStatus::unsupportedProfile:
                return inactivePlan(snapshot, previousState,
                                    RadioDecisionReason::stationProfileUnsupported, rawStation);
            case RadioStationResolutionStatus::unknownRaw:
                return inactivePlan(snapshot, previousState,
                                    RadioDecisionReason::stationIdentityUnknown, rawStation);
            case RadioStationResolutionStatus::resolved:
                stationIdentity = resolution.identity;
                if (!stationIdentity) {
                    return inactivePlan(snapshot, previousState,
                                        RadioDecisionReason::stationIdentityUnknown, rawStation);
                }
                binding = findIdentityStationBinding(configuration, *stationIdentity);
                if (binding == nullptr) {
                    return inactivePlan(snapshot, previousState,
                                        RadioDecisionReason::stationIdentityNotBound, rawStation,
                                        RadioStationBindingKind::none, stationIdentity);
                }
                bindingKind = RadioStationBindingKind::identity;
                break;
            }
        }

        if (!binding->second.enabled) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::stationDisabled,
                                rawStation, bindingKind, stationIdentity);
        }
        if (binding->second.url.empty()) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::stationUrlEmpty,
                                rawStation, bindingKind, stationIdentity);
        }
        if (!snapshot.radioVolume) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::volumeUnavailable,
                                rawStation, bindingKind, stationIdentity);
        }

        const auto preferenceVolume =
            std::clamp(*snapshot.radioVolume * configuration.general.volumeMultiplier, 0.0F, 1.0F);
        const bool keyIsSafe = isPublicStationKeySafe(binding->first);
        const auto publicStationKey =
            keyIsSafe ? binding->first : sanitizedStationKey(binding->first, rawStation);
        const bool sameStation =
            previousState.active && previousState.rawStation == rawStation &&
            previousState.bindingKind == bindingKind &&
            previousState.stationIdentity == stationIdentity && previousState.stationKey &&
            *previousState.stationKey == publicStationKey;

        auto plan = activePlanBase(snapshot, previousState);
        plan.bindingKind = bindingKind;
        plan.rawStation = rawStation;
        plan.stationIdentity = stationIdentity;
        plan.stationKey = publicStationKey;
        plan.preferenceVolume = preferenceVolume;
        plan.selectedStation = binding->second;
        if (*snapshot.pauseMenuActive) {
            plan.reason = RadioDecisionReason::paused;
            if (previousState.active && !previousState.paused && sameStation) {
                plan.action = RadioActionKind::wouldPause;
            }
            return plan;
        }

        plan.reason = RadioDecisionReason::ready;
        if (!previousState.active) {
            plan.action = RadioActionKind::wouldStart;
        } else if (!sameStation) {
            plan.action = RadioActionKind::wouldSwitch;
        } else if (previousState.paused) {
            plan.action = RadioActionKind::wouldResume;
        } else if (!previousState.preferenceVolume ||
                   std::fabs(*previousState.preferenceVolume - preferenceVolume) >
                       preferenceVolumeTolerance) {
            plan.action = RadioActionKind::wouldSetVolume;
        } else {
            plan.action = RadioActionKind::none;
            plan.reason = RadioDecisionReason::unchanged;
            return plan;
        }

        plan.onlineAudioWouldBeActive = true;
        return plan;
    } catch (...) {
        return inactivePlan(snapshot, previousState, RadioDecisionReason::observerUnavailable);
    }
}

} // namespace saors
