#include "saors_gta3/RadioDecisionEngine.hpp"

#include <algorithm>
#include <cmath>

namespace saors {
namespace {

constexpr float preferenceVolumeTolerance = 0.01F;

RadioActionPlan inactivePlan(const GameStateSnapshot& snapshot,
                             const SimulatedRadioState& previousState,
                             const RadioDecisionReason reason) {
    RadioActionPlan plan;
    plan.action = previousState.active ? RadioActionKind::wouldStop : RadioActionKind::none;
    plan.reason = reason;
    plan.snapshotSequence = snapshot.sequence;
    plan.onlineAudioWouldBeActive = false;
    return plan;
}

const std::pair<const std::string, StationConfiguration>*
findStationBinding(const ConfigurationData& configuration, const int rawStation) {
    const std::pair<const std::string, StationConfiguration>* disabledBinding = nullptr;
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

        const auto* binding = findStationBinding(configuration, *snapshot.radioStationRaw);
        if (binding == nullptr) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::stationNotBound);
        }
        if (!binding->second.enabled) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::stationDisabled);
        }
        if (binding->second.url.empty()) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::stationUrlEmpty);
        }
        if (!snapshot.radioVolume) {
            return inactivePlan(snapshot, previousState, RadioDecisionReason::volumeUnavailable);
        }

        const auto preferenceVolume =
            std::clamp(*snapshot.radioVolume * configuration.general.volumeMultiplier, 0.0F, 1.0F);
        const bool keyIsSafe = isPublicStationKeySafe(binding->first);
        const bool samePublicKey =
            previousState.stationKey &&
            (keyIsSafe ? *previousState.stationKey == binding->first
                       : *previousState.stationKey ==
                             sanitizedStationKey(binding->first, snapshot.radioStationRaw));
        const bool sameStation = previousState.active &&
                                 previousState.rawStation == snapshot.radioStationRaw &&
                                 samePublicKey;

        auto plan = activePlanBase(snapshot, previousState);
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

        plan.rawStation = snapshot.radioStationRaw;
        plan.stationKey = keyIsSafe ? binding->first
                                    : sanitizedStationKey(binding->first, snapshot.radioStationRaw);
        plan.preferenceVolume = preferenceVolume;
        plan.onlineAudioWouldBeActive = true;
        return plan;
    } catch (...) {
        return inactivePlan(snapshot, previousState, RadioDecisionReason::observerUnavailable);
    }
}

} // namespace saors
