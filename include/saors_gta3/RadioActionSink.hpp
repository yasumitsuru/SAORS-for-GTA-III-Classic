#pragma once

#include "saors_gta3/RadioDecision.hpp"

#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>

namespace saors {

class RadioActionSink {
  public:
    virtual ~RadioActionSink() = default;
    virtual void submit(const RadioActionPlan& plan) noexcept = 0;
    [[nodiscard]] virtual SimulatedRadioState simulatedState() const noexcept = 0;
};

class NullRadioActionSink final : public RadioActionSink {
  public:
    void submit(const RadioActionPlan& plan) noexcept override;
    [[nodiscard]] SimulatedRadioState simulatedState() const noexcept override;
};

class DryRunRadioActionSink final : public RadioActionSink {
  public:
    explicit DryRunRadioActionSink(bool logDecisions, std::chrono::milliseconds minimumLogInterval =
                                                          std::chrono::milliseconds(250));

    void submit(const RadioActionPlan& plan) noexcept override;
    [[nodiscard]] SimulatedRadioState simulatedState() const noexcept override;
    [[nodiscard]] RadioActionPlan lastSubmittedPlan() const noexcept;
    [[nodiscard]] std::size_t loggedMessageCount() const noexcept;

  private:
    using Clock = std::chrono::steady_clock;

    void considerLogging(const RadioActionPlan& plan, Clock::time_point now);
    void logPlan(const RadioActionPlan& plan);

    mutable std::mutex mutex_;
    SimulatedRadioState state_;
    RadioActionPlan lastSubmitted_;
    std::optional<RadioActionPlan> lastLogged_;
    std::optional<RadioActionPlan> pendingLog_;
    Clock::time_point lastLogTime_{};
    std::chrono::milliseconds minimumLogInterval_;
    std::size_t loggedMessages_{0};
    bool logDecisions_{false};
};

} // namespace saors
