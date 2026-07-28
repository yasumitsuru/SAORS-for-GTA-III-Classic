#pragma once

#include "saors_gta3/GameAddressProfile.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace saors {

struct GameStateSnapshot {
    bool observerAvailable{false};
    bool gameReady{false};

    std::optional<bool> pauseMenuActive;
    std::optional<bool> playerInVehicle;
    std::optional<int> radioStationRaw;
    std::optional<float> radioVolume;

    std::uint64_t sequence{0};
};

class GameStateSnapshotListener {
  public:
    virtual ~GameStateSnapshotListener() = default;
    virtual void onGameStateSnapshot(const GameStateSnapshot& snapshot) noexcept = 0;
};

class GameStateSource {
  public:
    virtual ~GameStateSource() = default;
    [[nodiscard]] virtual GameStateSnapshot capture() noexcept = 0;
};

class UnavailableGameStateSource final : public GameStateSource {
  public:
    [[nodiscard]] GameStateSnapshot capture() noexcept override;
};

class FakeGameStateSource final : public GameStateSource {
  public:
    void setSnapshot(GameStateSnapshot snapshot) noexcept;
    [[nodiscard]] GameStateSnapshot capture() noexcept override;

  private:
    std::mutex mutex_;
    GameStateSnapshot snapshot_;
};

class GameRuntimeAccess {
  public:
    virtual ~GameRuntimeAccess() = default;
    [[nodiscard]] virtual std::optional<bool> readBoolean(std::uintptr_t address) = 0;
    [[nodiscard]] virtual std::optional<int> readInteger(std::uintptr_t address) = 0;
    [[nodiscard]] virtual std::optional<std::uintptr_t>
    callFindPlayerVehicle(std::uintptr_t address) = 0;
    [[nodiscard]] virtual std::optional<int> callGetRadioInCar(std::uintptr_t address,
                                                               std::uintptr_t dmaAudio) = 0;
};

class PluginSdkGameStateSource final : public GameStateSource {
  public:
    PluginSdkGameStateSource(GameAddressProfile profile,
                             std::shared_ptr<GameRuntimeAccess> runtime);
    [[nodiscard]] GameStateSnapshot capture() noexcept override;

  private:
    GameAddressProfile profile_;
    std::shared_ptr<GameRuntimeAccess> runtime_;
};

class GameStateStore {
  public:
    void publish(GameStateSnapshot snapshot) noexcept;
    [[nodiscard]] GameStateSnapshot snapshot() const noexcept;

  private:
    mutable std::mutex mutex_;
    GameStateSnapshot snapshot_;
};

class GameStateTransitionTracker {
  public:
    using Clock = std::chrono::steady_clock;

    [[nodiscard]] std::vector<std::string> transitions(const GameStateSnapshot& snapshot,
                                                       Clock::time_point now) noexcept;

  private:
    std::optional<GameStateSnapshot> lastLogged_;
    Clock::time_point lastLogTime_{};
    std::size_t emittedMessages_{0};
};

} // namespace saors
