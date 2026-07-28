#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace saors {

enum class RadioActionKind {
    none,
    wouldStart,
    wouldPause,
    wouldResume,
    wouldSwitch,
    wouldSetVolume,
    wouldStop,
};

enum class RadioDecisionReason {
    unchanged,
    projectDisabled,
    controllerDisabled,
    observerUnavailable,
    gameNotReady,
    pauseStateUnavailable,
    vehicleStateUnavailable,
    playerOnFoot,
    stationUnavailable,
    stationNotBound,
    stationDisabled,
    stationUrlEmpty,
    volumeUnavailable,
    snapshotOutOfOrder,
    paused,
    ready,
};

struct RadioActionPlan {
    RadioActionKind action{RadioActionKind::none};
    RadioDecisionReason reason{RadioDecisionReason::unchanged};
    std::uint64_t snapshotSequence{0};
    std::optional<int> rawStation;
    std::optional<std::string> stationKey;
    std::optional<float> preferenceVolume;
    bool onlineAudioWouldBeActive{false};
};

struct SimulatedRadioState {
    bool active{false};
    bool paused{false};
    std::optional<int> rawStation;
    std::optional<std::string> stationKey;
    std::optional<float> preferenceVolume;
    std::uint64_t lastSnapshotSequence{0};
};

[[nodiscard]] const char* radioActionKindName(RadioActionKind action) noexcept;
[[nodiscard]] const char* radioDecisionReasonName(RadioDecisionReason reason) noexcept;
[[nodiscard]] bool isPublicStationKeySafe(const std::string& stationKey) noexcept;
[[nodiscard]] std::string sanitizedStationKey(const std::string& stationKey,
                                              std::optional<int> rawStation);
[[nodiscard]] bool sameRadioActionTransition(const RadioActionPlan& left,
                                             const RadioActionPlan& right) noexcept;
void applyRadioActionPlan(const RadioActionPlan& plan, SimulatedRadioState& state) noexcept;

} // namespace saors
