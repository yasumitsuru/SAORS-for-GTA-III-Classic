#pragma once

#include "saors_gta3/GameState.hpp"
#include "saors_gta3/RadioStationEvidence.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace saors {

class RadioStationObservationRecorder final {
  public:
    explicit RadioStationObservationRecorder(
        ExecutableProfileId executableProfile = ExecutableProfileId::unsupported,
        std::uint32_t minimumStableFrames = 15U,
        std::size_t maximumObservations = 256U);

    [[nodiscard]] std::optional<RadioStationObservation>
    observe(const GameStateSnapshot& snapshot) noexcept;

    void reset(std::string sessionId) noexcept;
    void configure(std::uint32_t minimumStableFrames, std::size_t maximumObservations) noexcept;

    [[nodiscard]] std::size_t observationCount() const noexcept;
    [[nodiscard]] const std::string& sessionId() const noexcept;
    [[nodiscard]] const std::vector<RadioStationObservation>& observations() const noexcept;

  private:
    ExecutableProfileId executableProfile_;
    std::uint32_t minimumStableFrames_;
    std::size_t maximumObservations_;
    std::string sessionId_;
    std::optional<std::uint64_t> lastSequence_;
    std::optional<int> candidateRaw_;
    std::uint32_t candidateStableFrames_{0};
    std::optional<int> lastEmittedRaw_;
    std::uint32_t nextOrdinal_{1U};
    std::vector<RadioStationObservation> observations_;
};

} // namespace saors
