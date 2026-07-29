#pragma once

#include "saors_gta3/Configuration.hpp"
#include "saors_gta3/GameState.hpp"
#include "saors_gta3/RadioActionSink.hpp"
#include "saors_gta3/RadioDecisionEngine.hpp"
#include "saors_gta3/RadioStationResolver.hpp"

#include <mutex>

namespace saors {

class RadioController final : public GameStateSnapshotListener {
  public:
    RadioController(const ConfigurationData& configuration, ExecutableProfileId executableProfile,
                    const RadioStationResolver& resolver, RadioActionSink& sink);

    void update(const GameStateSnapshot& snapshot) noexcept;
    void onGameStateSnapshot(const GameStateSnapshot& snapshot) noexcept override;

    [[nodiscard]] SimulatedRadioState simulatedState() const noexcept;
    [[nodiscard]] RadioActionPlan lastPlan() const noexcept;

  private:
    ConfigurationData configuration_;
    ExecutableProfileId executableProfile_{ExecutableProfileId::unsupported};
    const RadioStationResolver& resolver_;
    RadioActionSink& sink_;
    RadioDecisionEngine engine_;
    mutable std::mutex stateMutex_;
    SimulatedRadioState state_;
    mutable std::mutex planMutex_;
    RadioActionPlan lastPlan_;
};

} // namespace saors
