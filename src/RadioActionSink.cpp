#include "saors_gta3/RadioActionSink.hpp"

#include "saors_gta3/Logger.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace saors {
namespace {

constexpr std::size_t maximumDecisionLogMessages = 256U;

std::string planLogMessage(const RadioActionPlan& plan) {
    std::ostringstream message;
    message << "Radio dry-run: ";
    switch (plan.action) {
    case RadioActionKind::wouldStart:
        message << "would start";
        break;
    case RadioActionKind::wouldPause:
        message << "would pause";
        break;
    case RadioActionKind::wouldResume:
        message << "would resume";
        break;
    case RadioActionKind::wouldSwitch:
        message << "would switch";
        break;
    case RadioActionKind::wouldSetVolume:
        message << "would set";
        break;
    case RadioActionKind::wouldStop:
        message << "would stop";
        break;
    case RadioActionKind::none:
        return {};
    }

    if ((plan.action == RadioActionKind::wouldStart ||
         plan.action == RadioActionKind::wouldSwitch) &&
        plan.stationKey) {
        message << " station key=" << sanitizedStationKey(*plan.stationKey, plan.rawStation);
    }
    if ((plan.action == RadioActionKind::wouldStart ||
         plan.action == RadioActionKind::wouldSwitch) &&
        plan.rawStation) {
        message << " raw=" << *plan.rawStation;
    }
    if ((plan.action == RadioActionKind::wouldStart ||
         plan.action == RadioActionKind::wouldSwitch ||
         plan.action == RadioActionKind::wouldSetVolume) &&
        plan.preferenceVolume) {
        message << " preference-volume=" << std::fixed << std::setprecision(2)
                << *plan.preferenceVolume;
    }
    if (plan.action == RadioActionKind::wouldStop) {
        message << " reason=" << radioDecisionReasonName(plan.reason);
    }
    return message.str();
}

} // namespace

void NullRadioActionSink::submit(const RadioActionPlan& plan) noexcept {
    static_cast<void>(plan);
}

SimulatedRadioState NullRadioActionSink::simulatedState() const noexcept {
    return {};
}

DryRunRadioActionSink::DryRunRadioActionSink(const bool logDecisions,
                                             const std::chrono::milliseconds minimumLogInterval)
    : minimumLogInterval_(minimumLogInterval), logDecisions_(logDecisions) {}

void DryRunRadioActionSink::submit(const RadioActionPlan& plan) noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        auto safePlan = plan;
        if (safePlan.stationKey) {
            safePlan.stationKey = sanitizedStationKey(*safePlan.stationKey, safePlan.rawStation);
        }
        applyRadioActionPlan(safePlan, state_);
        lastSubmitted_ = safePlan;
        considerLogging(safePlan, Clock::now());
    } catch (...) {
    }
}

SimulatedRadioState DryRunRadioActionSink::simulatedState() const noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    } catch (...) {
        return {};
    }
}

RadioActionPlan DryRunRadioActionSink::lastSubmittedPlan() const noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        return lastSubmitted_;
    } catch (...) {
        return {};
    }
}

std::size_t DryRunRadioActionSink::loggedMessageCount() const noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        return loggedMessages_;
    } catch (...) {
        return 0;
    }
}

void DryRunRadioActionSink::considerLogging(const RadioActionPlan& plan,
                                            const Clock::time_point now) {
    if (!logDecisions_ || loggedMessages_ >= maximumDecisionLogMessages) {
        return;
    }

    if (pendingLog_ && now - lastLogTime_ >= minimumLogInterval_) {
        logPlan(*pendingLog_);
        pendingLog_.reset();
    }
    if (plan.action == RadioActionKind::none ||
        (lastLogged_ && sameRadioActionTransition(plan, *lastLogged_))) {
        return;
    }
    if (lastLogged_ && now - lastLogTime_ < minimumLogInterval_) {
        pendingLog_ = plan;
        return;
    }
    logPlan(plan);
}

void DryRunRadioActionSink::logPlan(const RadioActionPlan& plan) {
    const auto message = planLogMessage(plan);
    if (message.empty() || loggedMessages_ >= maximumDecisionLogMessages) {
        return;
    }
    Logger::info(message);
    lastLogged_ = plan;
    lastLogTime_ = Clock::now();
    ++loggedMessages_;
}

} // namespace saors
