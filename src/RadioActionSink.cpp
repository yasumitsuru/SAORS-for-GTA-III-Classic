#include "saors_gta3/RadioActionSink.hpp"

#include "saors_gta3/HttpUrl.hpp"
#include "saors_gta3/Logger.hpp"
#include "saors_gta3/OriginalRadioController.hpp"
#include "saors_gta3/StreamManager.hpp"

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

GameplayRadioActionSink::GameplayRadioActionSink(StreamManager& streams,
                                                 const GeneralConfiguration& configuration,
                                                 OriginalRadioController* originalRadio,
                                                 const bool suppressOriginalRadio)
    : streams_(streams), configuration_(configuration), originalRadio_(originalRadio),
      suppressOriginalRadio_(suppressOriginalRadio && originalRadio != nullptr) {}

GameplayRadioActionSink::~GameplayRadioActionSink() {
    shutdown();
}

void GameplayRadioActionSink::submit(const RadioActionPlan& plan) noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (state_.lastSnapshotSequence != 0 && plan.snapshotSequence <= state_.lastSnapshotSequence) {
            return;
        }
        if (failedClosed_) {
            auto stopPlan = plan;
            stopPlan.action = RadioActionKind::wouldStop;
            stopPlan.onlineAudioWouldBeActive = false;
            applyRadioActionPlan(stopPlan, state_);
            return;
        }
        if (originalRadioMuted_ &&
            (originalRadio_ == nullptr || !originalRadio_->available() ||
             !originalRadio_->muted())) {
            failClosed(plan, "original radio suppression unavailable");
            return;
        }

        bool executionSucceeded = true;
        const char* failure = "execution failed";
        switch (plan.action) {
        case RadioActionKind::wouldStart:
        case RadioActionKind::wouldSwitch:
            executionSucceeded = startSelectedStation(plan);
            failure = "station start failed";
            if (executionSucceeded) {
                executionSucceeded = muteOriginalRadio();
                failure = "original radio suppression failed";
            }
            if (executionSucceeded) {
                logAction(plan,
                          plan.action == RadioActionKind::wouldSwitch ? "switched" : "started");
            }
            break;
        case RadioActionKind::wouldPause:
            if (!streams_.pause()) {
                executionSucceeded = false;
            } else {
                logAction(plan, "paused");
            }
            failure = "pause failed";
            break;
        case RadioActionKind::wouldResume:
            if (!streams_.resume()) {
                executionSucceeded = false;
            } else {
                logAction(plan, "resumed");
            }
            failure = "resume failed";
            break;
        case RadioActionKind::wouldSetVolume:
            if (!plan.preferenceVolume || !streams_.setVolume(*plan.preferenceVolume)) {
                executionSucceeded = false;
            } else {
                logAction(plan, "volume updated");
            }
            failure = "volume update failed";
            break;
        case RadioActionKind::wouldStop:
            streams_.stop();
            if (!restoreOriginalRadio()) {
                failedClosed_ = true;
                logSuppressionFailure();
                logFailure("original radio restoration failed");
            } else {
                logAction(plan, "stopped");
            }
            break;
        case RadioActionKind::none:
            if (!plan.onlineAudioWouldBeActive) {
                streams_.stop();
                if (!restoreOriginalRadio()) {
                    failedClosed_ = true;
                    logSuppressionFailure();
                    logFailure("original radio restoration failed");
                }
            }
            break;
        }

        if (executionSucceeded) {
            applyRadioActionPlan(plan, state_);
        } else {
            failClosed(plan, failure);
        }
    } catch (...) {
        try {
            const std::lock_guard<std::mutex> lock(mutex_);
            failClosed(plan, "execution failed");
        } catch (...) {
            streams_.stop();
            if (suppressOriginalRadio_) {
                static_cast<void>(restoreOriginalRadio());
                logSuppressionFailure();
            }
        }
    }
}

SimulatedRadioState GameplayRadioActionSink::simulatedState() const noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    } catch (...) {
        return {};
    }
}

void GameplayRadioActionSink::shutdown() noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        streams_.stop();
        if (!restoreOriginalRadio()) {
            logSuppressionFailure();
        }
    } catch (...) {
        streams_.stop();
        if (suppressOriginalRadio_) {
            static_cast<void>(restoreOriginalRadio());
            logSuppressionFailure();
        }
    }
}

bool GameplayRadioActionSink::startSelectedStation(const RadioActionPlan& plan) {
    if (!plan.selectedStation || plan.selectedStation->url.empty() || !plan.preferenceVolume) {
        return false;
    }
    const auto& station = *plan.selectedStation;

    const auto parsedUrl = parseHttpUrl(station.url);
    if (!parsedUrl || (!station.allowHttp && !parsedUrl.value.secure()) ||
        !streams_.setBufferMilliseconds(configuration_.bufferMilliseconds)) {
        return false;
    }

    ResolveOptions options;
    options.allowHttpStreams = station.allowHttp;
    options.connectTimeoutMilliseconds = configuration_.playlistConnectTimeoutMilliseconds;
    options.receiveTimeoutMilliseconds = configuration_.playlistReceiveTimeoutMilliseconds;
    options.maximumPlaylistBytes = configuration_.playlistMaximumBytes;
    options.maximumEntries = configuration_.playlistMaximumEntries;
    options.maximumRedirects = configuration_.playlistMaximumRedirects;
    options.maximumDepth = configuration_.playlistMaximumDepth;

    const bool started = configuration_.resolveRemotePlaylists
                             ? streams_.startConfiguredUrl(station.url, *plan.preferenceVolume,
                                                          options)
                             : streams_.start(station.url, *plan.preferenceVolume);
    if (!started) {
        return false;
    }

    return true;
}

bool GameplayRadioActionSink::muteOriginalRadio() noexcept {
    if (!suppressOriginalRadio_) {
        return true;
    }
    if (originalRadio_ == nullptr || !originalRadio_->available()) {
        return false;
    }
    if (originalRadioMuted_) {
        return originalRadio_->muted();
    }

    originalRadioMuteAttempted_ = true;
    if (!originalRadio_->mute() || !originalRadio_->muted()) {
        return false;
    }
    originalRadioMuted_ = true;
    Logger::info("Original radio: muted");
    return true;
}

bool GameplayRadioActionSink::restoreOriginalRadio() noexcept {
    if (!suppressOriginalRadio_ || originalRadio_ == nullptr) {
        return true;
    }

    const bool requiresRestore =
        originalRadioMuteAttempted_ || originalRadioMuted_ || originalRadio_->muted();
    if (!requiresRestore) {
        return true;
    }

    if (!originalRadio_->restore() || originalRadio_->muted()) {
        return false;
    }
    originalRadioMuteAttempted_ = false;
    originalRadioMuted_ = false;
    Logger::info("Original radio: restored");
    return true;
}

void GameplayRadioActionSink::failClosed(const RadioActionPlan& plan,
                                         const char* action) noexcept {
    const bool suppressionAffected =
        suppressOriginalRadio_ &&
        (originalRadioMuteAttempted_ || originalRadioMuted_ ||
         (originalRadio_ != nullptr && originalRadio_->muted()));
    streams_.stop();
    static_cast<void>(restoreOriginalRadio());
    failedClosed_ = true;
    auto stopPlan = plan;
    stopPlan.action = RadioActionKind::wouldStop;
    stopPlan.onlineAudioWouldBeActive = false;
    applyRadioActionPlan(stopPlan, state_);
    if (suppressionAffected) {
        logSuppressionFailure();
    }
    logFailure(action);
}

void GameplayRadioActionSink::logSuppressionFailure() noexcept {
    try {
        Logger::warning("Original radio suppression: failed closed");
    } catch (...) {
    }
}

void GameplayRadioActionSink::logAction(const RadioActionPlan& plan, const char* action) noexcept {
    try {
        const auto stationKey = plan.stationKey
                                    ? sanitizedStationKey(*plan.stationKey, plan.rawStation)
                                    : std::string("Station");
        Logger::info(std::string("Gameplay audio: ") + action + " station key=" + stationKey);
    } catch (...) {
    }
}

void GameplayRadioActionSink::logFailure(const char* action) noexcept {
    try {
        Logger::warning(std::string("Gameplay audio: ") + action);
    } catch (...) {
    }
}

} // namespace saors
