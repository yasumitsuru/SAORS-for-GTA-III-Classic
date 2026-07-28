#include "saors_gta3/GameState.hpp"

#include <cmath>
#include <limits>
#include <utility>

namespace saors {
namespace {

constexpr auto minimumTransitionInterval = std::chrono::milliseconds(250);
constexpr std::size_t maximumTransitionMessages = 256U;

template <typename Function> auto safely(Function&& function) noexcept -> decltype(function()) {
    try {
        return function();
    } catch (...) {
        return {};
    }
}

bool plausibleVehiclePointer(const std::uintptr_t pointer) noexcept {
    constexpr std::uintptr_t minimumUserPointer = 0x00010000U;
    constexpr std::uintptr_t maximumX86Pointer = 0xFFFFFFFFU;
    return pointer >= minimumUserPointer && pointer <= maximumX86Pointer &&
           (pointer % alignof(std::uint32_t)) == 0;
}

std::string availability(const std::optional<bool>& value, const char* trueText,
                         const char* falseText, const char* unavailableText) {
    if (!value.has_value()) {
        return unavailableText;
    }
    return *value ? trueText : falseText;
}

template <typename Value>
bool changed(const std::optional<Value>& left, const std::optional<Value>& right) {
    return left != right;
}

} // namespace

GameStateSnapshot UnavailableGameStateSource::capture() noexcept {
    return {};
}

void FakeGameStateSource::setSnapshot(GameStateSnapshot snapshot) noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        snapshot_ = std::move(snapshot);
    } catch (...) {
    }
}

GameStateSnapshot FakeGameStateSource::capture() noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        return snapshot_;
    } catch (...) {
        return {};
    }
}

PluginSdkGameStateSource::PluginSdkGameStateSource(GameAddressProfile profile,
                                                   std::shared_ptr<GameRuntimeAccess> runtime)
    : profile_(std::move(profile)), runtime_(std::move(runtime)) {}

GameStateSnapshot PluginSdkGameStateSource::capture() noexcept {
    GameStateSnapshot snapshot;
    snapshot.observerAvailable = runtime_ != nullptr;
    if (runtime_ == nullptr || !profile_.frontEndMenuManager || !profile_.findPlayerVehicle ||
        !profile_.getRadioInCar || !profile_.musicVolume || !profile_.dmaAudio) {
        snapshot.observerAvailable = false;
        return snapshot;
    }

    const auto gameNotLoadedAddress = *profile_.frontEndMenuManager + profile_.gameNotLoadedOffset;
    const auto pauseMenuAddress = *profile_.frontEndMenuManager + profile_.pauseMenuActiveOffset;

    const auto gameNotLoaded = safely([&] { return runtime_->readBoolean(gameNotLoadedAddress); });
    snapshot.gameReady = gameNotLoaded.has_value() && !*gameNotLoaded;
    snapshot.pauseMenuActive = safely([&] { return runtime_->readBoolean(pauseMenuAddress); });

    if (snapshot.gameReady) {
        const auto vehicle =
            safely([&] { return runtime_->callFindPlayerVehicle(*profile_.findPlayerVehicle); });
        if (vehicle.has_value()) {
            if (*vehicle == 0) {
                snapshot.playerInVehicle = false;
            } else if (plausibleVehiclePointer(*vehicle)) {
                snapshot.playerInVehicle = true;
            }
        }

        const auto station = safely([&] {
            return runtime_->callGetRadioInCar(*profile_.getRadioInCar, *profile_.dmaAudio);
        });
        if (station.has_value() && *station >= profile_.minimumRadioStation &&
            *station <= profile_.maximumRadioStation) {
            snapshot.radioStationRaw = *station;
        }
    }

    const auto volume = safely([&] { return runtime_->readInteger(*profile_.musicVolume); });
    if (volume.has_value() && profile_.maximumMusicVolume > profile_.minimumMusicVolume &&
        *volume >= profile_.minimumMusicVolume && *volume <= profile_.maximumMusicVolume) {
        const auto numerator = static_cast<float>(*volume - profile_.minimumMusicVolume);
        const auto denominator =
            static_cast<float>(profile_.maximumMusicVolume - profile_.minimumMusicVolume);
        const auto normalized = numerator / denominator;
        if (std::isfinite(normalized)) {
            snapshot.radioVolume = normalized;
        }
    }
    return snapshot;
}

void GameStateStore::publish(GameStateSnapshot snapshot) noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        snapshot_ = std::move(snapshot);
    } catch (...) {
    }
}

GameStateSnapshot GameStateStore::snapshot() const noexcept {
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        return snapshot_;
    } catch (...) {
        return {};
    }
}

std::vector<std::string>
GameStateTransitionTracker::transitions(const GameStateSnapshot& snapshot,
                                        const Clock::time_point now) noexcept {
    std::vector<std::string> messages;
    try {
        if (emittedMessages_ >= maximumTransitionMessages) {
            return messages;
        }
        if (lastLogged_.has_value() && now - lastLogTime_ < minimumTransitionInterval) {
            return messages;
        }

        const bool first = !lastLogged_.has_value();
        const auto previous = lastLogged_.value_or(GameStateSnapshot{});
        if (first || previous.gameReady != snapshot.gameReady) {
            messages.push_back(std::string("Game ready: ") +
                               (snapshot.gameReady ? "true" : "false"));
        }
        if (first || changed(previous.pauseMenuActive, snapshot.pauseMenuActive)) {
            messages.push_back(std::string("Pause menu: ") + availability(snapshot.pauseMenuActive,
                                                                          "active", "inactive",
                                                                          "unavailable"));
        }
        if (first || changed(previous.playerInVehicle, snapshot.playerInVehicle)) {
            messages.push_back(
                std::string("Player vehicle state: ") +
                availability(snapshot.playerInVehicle, "in vehicle", "on foot", "unavailable"));
        }
        if (first || changed(previous.radioStationRaw, snapshot.radioStationRaw)) {
            messages.push_back(snapshot.radioStationRaw
                                   ? "Radio station raw: " +
                                         std::to_string(*snapshot.radioStationRaw)
                                   : "Radio station raw: unavailable");
        }
        if (first || changed(previous.radioVolume, snapshot.radioVolume)) {
            messages.push_back(std::string("Radio volume: ") +
                               (snapshot.radioVolume ? "available" : "unavailable"));
        }

        if (!messages.empty()) {
            if (messages.size() > maximumTransitionMessages - emittedMessages_) {
                messages.resize(maximumTransitionMessages - emittedMessages_);
            }
            emittedMessages_ += messages.size();
            lastLogged_ = snapshot;
            lastLogTime_ = now;
        }
    } catch (...) {
        messages.clear();
    }
    return messages;
}

} // namespace saors
