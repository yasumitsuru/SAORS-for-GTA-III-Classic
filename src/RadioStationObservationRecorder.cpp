#include "saors_gta3/RadioStationObservationRecorder.hpp"

#include <algorithm>
#include <utility>

namespace saors {
namespace {

constexpr std::uint32_t defaultMinimumStableFrames = 15U;
constexpr std::uint32_t maximumMinimumStableFrames = 600U;

std::uint32_t safeMinimumStableFrames(const std::uint32_t value) noexcept {
    return value == 0U ? defaultMinimumStableFrames
                       : std::min(value, maximumMinimumStableFrames);
}

} // namespace

RadioStationObservationRecorder::RadioStationObservationRecorder(
    const ExecutableProfileId executableProfile,
    const std::uint32_t minimumStableFrames,
    const std::size_t maximumObservations)
    : executableProfile_(executableProfile),
      minimumStableFrames_(safeMinimumStableFrames(minimumStableFrames)),
      maximumObservations_(maximumObservations),
      sessionId_("unassigned") {}

std::optional<RadioStationObservation>
RadioStationObservationRecorder::observe(const GameStateSnapshot& snapshot) noexcept {
    try {
        if (!snapshot.observerAvailable || !snapshot.gameReady ||
            !snapshot.playerInVehicle || !*snapshot.playerInVehicle || !snapshot.radioStationRaw ||
            *snapshot.radioStationRaw < 0 || *snapshot.radioStationRaw > 255 ||
            maximumObservations_ == 0U) {
            return std::nullopt;
        }
        if (lastSequence_ && snapshot.sequence <= *lastSequence_) {
            return std::nullopt;
        }
        lastSequence_ = snapshot.sequence;

        const auto raw = *snapshot.radioStationRaw;
        if (!candidateRaw_ || *candidateRaw_ != raw) {
            candidateRaw_ = raw;
            candidateStableFrames_ = 1U;
        } else if (candidateStableFrames_ < minimumStableFrames_) {
            ++candidateStableFrames_;
        }

        if (candidateStableFrames_ < minimumStableFrames_ ||
            (lastEmittedRaw_ && *lastEmittedRaw_ == raw) ||
            observations_.size() >= maximumObservations_) {
            return std::nullopt;
        }

        RadioStationObservation observation;
        observation.sessionId = sessionId_;
        observation.executableProfile = executableProfile_;
        observation.snapshotSequence = snapshot.sequence;
        observation.ordinal = nextOrdinal_++;
        observation.rawValue = raw;
        observation.stableFrames = candidateStableFrames_;
        observation.playerInVehicle = true;
        observations_.push_back(observation);
        lastEmittedRaw_ = raw;
        return observation;
    } catch (...) {
        return std::nullopt;
    }
}

void RadioStationObservationRecorder::reset(std::string sessionId) noexcept {
    try {
        sessionId_ = std::move(sessionId);
        if (sessionId_.empty()) {
            sessionId_ = "unassigned";
        }
        lastSequence_.reset();
        candidateRaw_.reset();
        candidateStableFrames_ = 0U;
        lastEmittedRaw_.reset();
        nextOrdinal_ = 1U;
        observations_.clear();
    } catch (...) {
        sessionId_ = "unassigned";
        lastSequence_.reset();
        candidateRaw_.reset();
        candidateStableFrames_ = 0U;
        lastEmittedRaw_.reset();
        nextOrdinal_ = 1U;
        observations_.clear();
    }
}

void RadioStationObservationRecorder::configure(const std::uint32_t minimumStableFrames,
                                                const std::size_t maximumObservations) noexcept {
    minimumStableFrames_ = safeMinimumStableFrames(minimumStableFrames);
    maximumObservations_ = maximumObservations;
}

std::size_t RadioStationObservationRecorder::observationCount() const noexcept {
    return observations_.size();
}

const std::string& RadioStationObservationRecorder::sessionId() const noexcept {
    return sessionId_;
}

const std::vector<RadioStationObservation>&
RadioStationObservationRecorder::observations() const noexcept {
    return observations_;
}

} // namespace saors
