#pragma once

#include "saors_gta3/Configuration.hpp"
#include "saors_gta3/GameState.hpp"
#include "saors_gta3/RadioDecision.hpp"
#include "saors_gta3/RadioStationResolver.hpp"

namespace saors {

class RadioDecisionEngine {
  public:
    [[nodiscard]] RadioActionPlan evaluate(const GameStateSnapshot& snapshot,
                                           const ConfigurationData& configuration,
                                           ExecutableProfileId executableProfile,
                                           const RadioStationResolver& resolver,
                                           const SimulatedRadioState& previousState) const noexcept;
};

} // namespace saors
