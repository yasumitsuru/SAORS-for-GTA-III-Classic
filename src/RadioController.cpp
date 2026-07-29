#include "saors_gta3/RadioController.hpp"

#include <utility>

namespace saors {

RadioController::RadioController(const ConfigurationData& configuration,
                                 const ExecutableProfileId executableProfile,
                                 const RadioStationResolver& resolver, RadioActionSink& sink)
    : configuration_(configuration), executableProfile_(executableProfile), resolver_(resolver),
      sink_(sink) {}

void RadioController::update(const GameStateSnapshot& snapshot) noexcept {
    try {
        RadioActionPlan plan;
        {
            const std::lock_guard<std::mutex> lock(stateMutex_);
            plan = engine_.evaluate(snapshot, configuration_, executableProfile_, resolver_, state_);
            applyRadioActionPlan(plan, state_);
        }
        sink_.submit(plan);
        const std::lock_guard<std::mutex> lock(planMutex_);
        lastPlan_ = std::move(plan);
    } catch (...) {
    }
}

void RadioController::onGameStateSnapshot(const GameStateSnapshot& snapshot) noexcept {
    update(snapshot);
}

SimulatedRadioState RadioController::simulatedState() const noexcept {
    try {
        const std::lock_guard<std::mutex> lock(stateMutex_);
        return state_;
    } catch (...) {
        return {};
    }
}

RadioActionPlan RadioController::lastPlan() const noexcept {
    try {
        const std::lock_guard<std::mutex> lock(planMutex_);
        return lastPlan_;
    } catch (...) {
        return {};
    }
}

} // namespace saors
