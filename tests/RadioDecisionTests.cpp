#include "saors_gta3/Configuration.hpp"
#include "saors_gta3/GameObserver.hpp"
#include "saors_gta3/Logger.hpp"
#include "saors_gta3/RadioActionSink.hpp"
#include "saors_gta3/RadioController.hpp"
#include "saors_gta3/RadioDecisionEngine.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {

saors::ConfigurationData configuration() {
    saors::ConfigurationData result;
    result.general.enabled = true;
    result.general.volumeMultiplier = 1.0F;
    result.experimental.enableRadioController = true;
    result.experimental.radioControllerDryRun = true;

    saors::StationConfiguration station;
    station.enabled = true;
    station.name = "Local Test";
    station.url = "https://example.invalid/private-token";
    station.gameStationRaw = 3;
    result.stations.emplace("LocalTest", std::move(station));
    return result;
}

saors::GameStateSnapshot readySnapshot(const std::uint64_t sequence = 1U) {
    saors::GameStateSnapshot snapshot;
    snapshot.observerAvailable = true;
    snapshot.gameReady = true;
    snapshot.pauseMenuActive = false;
    snapshot.playerInVehicle = true;
    snapshot.radioStationRaw = 3;
    snapshot.radioVolume = 0.5F;
    snapshot.sequence = sequence;
    return snapshot;
}

saors::SimulatedRadioState activeState() {
    saors::SimulatedRadioState state;
    state.active = true;
    state.rawStation = 3;
    state.stationKey = "LocalTest";
    state.preferenceVolume = 0.5F;
    state.lastSnapshotSequence = 1U;
    return state;
}

class FakeRadioActionSink final : public saors::RadioActionSink {
  public:
    void submit(const saors::RadioActionPlan& plan) noexcept override {
        plans.push_back(plan);
        saors::applyRadioActionPlan(plan, state);
    }

    [[nodiscard]] saors::SimulatedRadioState simulatedState() const noexcept override {
        return state;
    }

    saors::SimulatedRadioState state;
    std::vector<saors::RadioActionPlan> plans;
    std::size_t audioCalls{0};
    std::size_t networkCalls{0};
};

class TemporaryLog {
  public:
    TemporaryLog() {
        static std::atomic<unsigned long> sequence{0};
        path_ = std::filesystem::temp_directory_path() /
                ("saors-radio-decision-" + std::to_string(sequence.fetch_add(1)) + ".log");
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    ~TemporaryLog() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

    [[nodiscard]] std::string contents() const {
        std::ifstream input(path_, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

  private:
    std::filesystem::path path_;
};

class RecordingListener final : public saors::GameStateSnapshotListener {
  public:
    void onGameStateSnapshot(const saors::GameStateSnapshot& snapshot) noexcept override {
        ++calls;
        lastSequence = snapshot.sequence;
    }

    std::size_t calls{0};
    std::uint64_t lastSequence{0};
};

class FakeObserver final : public saors::GameObserver {
  public:
    void setSnapshotListener(saors::GameStateSnapshotListener* listener) noexcept override {
        listener_ = listener;
    }

    [[nodiscard]] saors::ObserverInstallResult start(bool dryRun) noexcept override {
        static_cast<void>(dryRun);
        return {};
    }

    [[nodiscard]] bool stop() noexcept override {
        listener_ = nullptr;
        return true;
    }

    [[nodiscard]] saors::GameStateSnapshot snapshot() const noexcept override {
        return current_;
    }

    void emit(saors::GameStateSnapshot snapshot) noexcept {
        current_ = std::move(snapshot);
        if (listener_ != nullptr) {
            listener_->onGameStateSnapshot(current_);
        }
    }

  private:
    saors::GameStateSnapshotListener* listener_{nullptr};
    saors::GameStateSnapshot current_;
};

} // namespace

TEST_CASE("Decision engine conservatively stops or remains inactive for unavailable state") {
    const saors::RadioDecisionEngine engine;
    const auto settings = configuration();
    auto snapshot = readySnapshot(2U);
    const auto active = activeState();

    SECTION("observer unavailable") {
        snapshot.observerAvailable = false;
        const auto plan = engine.evaluate(snapshot, settings, active);
        CHECK(plan.action == saors::RadioActionKind::wouldStop);
        CHECK(plan.reason == saors::RadioDecisionReason::observerUnavailable);
    }

    SECTION("game not ready") {
        snapshot.gameReady = false;
        const auto plan = engine.evaluate(snapshot, settings, active);
        CHECK(plan.action == saors::RadioActionKind::wouldStop);
        CHECK(plan.reason == saors::RadioDecisionReason::gameNotReady);
    }

    SECTION("pause state unavailable") {
        snapshot.pauseMenuActive.reset();
        const auto plan = engine.evaluate(snapshot, settings, active);
        CHECK(plan.action == saors::RadioActionKind::wouldStop);
        CHECK(plan.reason == saors::RadioDecisionReason::pauseStateUnavailable);
    }

    SECTION("vehicle state unavailable") {
        snapshot.playerInVehicle.reset();
        const auto plan = engine.evaluate(snapshot, settings, active);
        CHECK(plan.action == saors::RadioActionKind::wouldStop);
        CHECK(plan.reason == saors::RadioDecisionReason::vehicleStateUnavailable);
    }

    SECTION("player on foot") {
        snapshot.playerInVehicle = false;
        const auto plan = engine.evaluate(snapshot, settings, active);
        CHECK(plan.action == saors::RadioActionKind::wouldStop);
        CHECK(plan.reason == saors::RadioDecisionReason::playerOnFoot);
    }

    SECTION("station unavailable") {
        snapshot.radioStationRaw.reset();
        const auto plan = engine.evaluate(snapshot, settings, active);
        CHECK(plan.action == saors::RadioActionKind::wouldStop);
        CHECK(plan.reason == saors::RadioDecisionReason::stationUnavailable);
    }
}

TEST_CASE("Project and controller gates never produce start decisions") {
    const saors::RadioDecisionEngine engine;
    auto settings = configuration();
    const auto snapshot = readySnapshot(2U);

    settings.general.enabled = false;
    auto plan = engine.evaluate(snapshot, settings, {});
    CHECK(plan.action == saors::RadioActionKind::none);
    CHECK(plan.reason == saors::RadioDecisionReason::projectDisabled);

    settings.general.enabled = true;
    settings.experimental.enableRadioController = false;
    plan = engine.evaluate(snapshot, settings, activeState());
    CHECK(plan.action == saors::RadioActionKind::wouldStop);
    CHECK(plan.reason == saors::RadioDecisionReason::controllerDisabled);

    settings.experimental.enableRadioController = true;
    settings.experimental.radioControllerDryRun = false;
    plan = engine.evaluate(snapshot, settings, {});
    CHECK(plan.action == saors::RadioActionKind::none);
    CHECK(plan.reason == saors::RadioDecisionReason::controllerDisabled);
}

TEST_CASE("Explicit station binding failures preserve the original radio") {
    const saors::RadioDecisionEngine engine;
    auto settings = configuration();
    const auto snapshot = readySnapshot(2U);

    settings.stations.clear();
    auto plan = engine.evaluate(snapshot, settings, activeState());
    CHECK(plan.action == saors::RadioActionKind::wouldStop);
    CHECK(plan.reason == saors::RadioDecisionReason::stationNotBound);

    settings = configuration();
    settings.stations.at("LocalTest").enabled = false;
    plan = engine.evaluate(snapshot, settings, {});
    CHECK(plan.action == saors::RadioActionKind::none);
    CHECK(plan.reason == saors::RadioDecisionReason::stationDisabled);

    settings = configuration();
    settings.stations.at("LocalTest").url.clear();
    plan = engine.evaluate(snapshot, settings, activeState());
    CHECK(plan.action == saors::RadioActionKind::wouldStop);
    CHECK(plan.reason == saors::RadioDecisionReason::stationUrlEmpty);

    auto missingVolume = snapshot;
    missingVolume.radioVolume.reset();
    plan = engine.evaluate(missingVolume, configuration(), activeState());
    CHECK(plan.action == saors::RadioActionKind::wouldStop);
    CHECK(plan.reason == saors::RadioDecisionReason::volumeUnavailable);
}

TEST_CASE("Decision state machine produces every dry-run action deterministically") {
    const saors::RadioDecisionEngine engine;
    auto settings = configuration();
    auto snapshot = readySnapshot(1U);
    saors::SimulatedRadioState state;

    auto plan = engine.evaluate(snapshot, settings, state);
    REQUIRE(plan.action == saors::RadioActionKind::wouldStart);
    CHECK(plan.reason == saors::RadioDecisionReason::ready);
    CHECK(plan.stationKey == "LocalTest");
    CHECK(plan.rawStation == 3);
    CHECK(plan.preferenceVolume == Catch::Approx(0.5F));
    saors::applyRadioActionPlan(plan, state);
    CHECK(state.active);
    CHECK_FALSE(state.paused);

    snapshot.sequence = 2U;
    plan = engine.evaluate(snapshot, settings, state);
    CHECK(plan.action == saors::RadioActionKind::none);
    CHECK(plan.reason == saors::RadioDecisionReason::unchanged);
    saors::applyRadioActionPlan(plan, state);

    snapshot.sequence = 3U;
    snapshot.pauseMenuActive = true;
    plan = engine.evaluate(snapshot, settings, state);
    REQUIRE(plan.action == saors::RadioActionKind::wouldPause);
    saors::applyRadioActionPlan(plan, state);
    CHECK(state.paused);

    snapshot.sequence = 4U;
    snapshot.pauseMenuActive = false;
    plan = engine.evaluate(snapshot, settings, state);
    REQUIRE(plan.action == saors::RadioActionKind::wouldResume);
    saors::applyRadioActionPlan(plan, state);
    CHECK_FALSE(state.paused);

    saors::StationConfiguration other;
    other.enabled = true;
    other.url = "https://example.invalid/other";
    other.gameStationRaw = 5;
    settings.stations.emplace("StationB", std::move(other));
    snapshot.sequence = 5U;
    snapshot.radioStationRaw = 5;
    plan = engine.evaluate(snapshot, settings, state);
    REQUIRE(plan.action == saors::RadioActionKind::wouldSwitch);
    CHECK(plan.stationKey == "StationB");
    saors::applyRadioActionPlan(plan, state);
    CHECK(state.rawStation == 5);

    snapshot.sequence = 6U;
    snapshot.radioVolume = 0.75F;
    plan = engine.evaluate(snapshot, settings, state);
    REQUIRE(plan.action == saors::RadioActionKind::wouldSetVolume);
    CHECK(plan.preferenceVolume == Catch::Approx(0.75F));
    saors::applyRadioActionPlan(plan, state);
    CHECK(state.preferenceVolume == Catch::Approx(0.75F));

    snapshot.sequence = 7U;
    snapshot.playerInVehicle = false;
    plan = engine.evaluate(snapshot, settings, state);
    REQUIRE(plan.action == saors::RadioActionKind::wouldStop);
    CHECK(plan.reason == saors::RadioDecisionReason::playerOnFoot);
    saors::applyRadioActionPlan(plan, state);
    CHECK_FALSE(state.active);
}

TEST_CASE("Preference volume uses multiplier clamp and explicit tolerance") {
    const saors::RadioDecisionEngine engine;
    auto settings = configuration();
    auto snapshot = readySnapshot(2U);
    auto state = activeState();

    settings.general.volumeMultiplier = 0.5F;
    state.preferenceVolume = 0.25F;
    auto plan = engine.evaluate(snapshot, settings, state);
    CHECK(plan.action == saors::RadioActionKind::none);

    state.preferenceVolume = 0.245F;
    plan = engine.evaluate(snapshot, settings, state);
    CHECK(plan.action == saors::RadioActionKind::none);

    state.preferenceVolume = 0.23F;
    plan = engine.evaluate(snapshot, settings, state);
    CHECK(plan.action == saors::RadioActionKind::wouldSetVolume);
    CHECK(plan.preferenceVolume == Catch::Approx(0.25F));

    settings.general.volumeMultiplier = 4.0F;
    state.preferenceVolume = 0.25F;
    plan = engine.evaluate(snapshot, settings, state);
    REQUIRE(plan.action == saors::RadioActionKind::wouldSetVolume);
    CHECK(plan.preferenceVolume == Catch::Approx(1.0F));
}

TEST_CASE("Out-of-order snapshots cannot mutate simulated state") {
    const saors::RadioDecisionEngine engine;
    const auto settings = configuration();
    auto state = activeState();
    state.lastSnapshotSequence = 10U;
    const auto snapshot = readySnapshot(9U);

    const auto plan = engine.evaluate(snapshot, settings, state);
    CHECK(plan.action == saors::RadioActionKind::none);
    CHECK(plan.reason == saors::RadioDecisionReason::snapshotOutOfOrder);
    saors::applyRadioActionPlan(plan, state);
    CHECK(state.lastSnapshotSequence == 10U);
    CHECK(state.active);
}

TEST_CASE("Unsafe configuration keys never enter public plans") {
    const saors::RadioDecisionEngine engine;
    auto settings = configuration();
    auto station = settings.stations.extract("LocalTest");
    station.key() = "token=https://private.example/secret";
    settings.stations.insert(std::move(station));

    const auto plan = engine.evaluate(readySnapshot(), settings, {});

    REQUIRE(plan.action == saors::RadioActionKind::wouldStart);
    REQUIRE(plan.stationKey);
    CHECK(*plan.stationKey == "StationRaw3");
    CHECK(plan.stationKey->find("token") == std::string::npos);
    CHECK(plan.stationKey->find("private") == std::string::npos);
}

TEST_CASE("Dry-run sink updates only simulated state and logs deduplicated sanitized transitions") {
    const TemporaryLog log;
    saors::Logger::initialize(log.path(), saors::LogLevel::info);
    saors::DryRunRadioActionSink sink(true, std::chrono::milliseconds(0));

    saors::RadioActionPlan start;
    start.action = saors::RadioActionKind::wouldStart;
    start.reason = saors::RadioDecisionReason::ready;
    start.snapshotSequence = 1U;
    start.rawStation = 3;
    start.stationKey = "token=https://private.example/secret";
    start.preferenceVolume = 0.5F;
    start.onlineAudioWouldBeActive = true;

    sink.submit(start);
    start.snapshotSequence = 2U;
    sink.submit(start);

    auto pause = start;
    pause.action = saors::RadioActionKind::wouldPause;
    pause.reason = saors::RadioDecisionReason::paused;
    pause.snapshotSequence = 3U;
    pause.stationKey.reset();
    pause.rawStation.reset();
    pause.preferenceVolume.reset();
    sink.submit(pause);

    const auto state = sink.simulatedState();
    CHECK(state.active);
    CHECK(state.paused);
    CHECK(state.stationKey == "StationRaw3");
    CHECK(sink.loggedMessageCount() == 2U);

    const auto contents = log.contents();
    CHECK(contents.find("would start") != std::string::npos);
    CHECK(contents.find("would pause") != std::string::npos);
    CHECK(contents.find("StationRaw3") != std::string::npos);
    CHECK(contents.find("https://") == std::string::npos);
    CHECK(contents.find("token") == std::string::npos);
    CHECK(contents.find("private") == std::string::npos);
}

TEST_CASE("Radio controller consumes listener snapshots without audio or network dependencies") {
    auto settings = configuration();
    FakeRadioActionSink sink;
    saors::RadioController controller(settings, sink);

    controller.onGameStateSnapshot(readySnapshot(1U));
    REQUIRE(sink.plans.size() == 1U);
    CHECK(sink.plans.back().action == saors::RadioActionKind::wouldStart);
    CHECK(controller.simulatedState().active);
    CHECK(controller.lastPlan().snapshotSequence == 1U);
    CHECK(sink.audioCalls == 0U);
    CHECK(sink.networkCalls == 0U);

    controller.onGameStateSnapshot(readySnapshot(1U));
    CHECK(controller.lastPlan().reason == saors::RadioDecisionReason::snapshotOutOfOrder);
    CHECK(controller.simulatedState().lastSnapshotSequence == 1U);
}

TEST_CASE("Radio controller simulated state is thread-safe and sequence monotonic") {
    FakeRadioActionSink sink;
    saors::RadioController controller(configuration(), sink);
    std::atomic<bool> finished{false};

    std::thread writer([&] {
        for (std::uint64_t sequence = 1U; sequence <= 500U; ++sequence) {
            controller.update(readySnapshot(sequence));
        }
        finished.store(true, std::memory_order_release);
    });

    std::uint64_t observedSequence = 0;
    while (!finished.load(std::memory_order_acquire)) {
        const auto state = controller.simulatedState();
        CHECK(state.lastSnapshotSequence >= observedSequence);
        observedSequence = state.lastSnapshotSequence;
    }
    writer.join();
    CHECK(controller.simulatedState().lastSnapshotSequence == 500U);
}

TEST_CASE("Snapshot listener can be absent and is invalidated before observer stop") {
    FakeObserver observer;
    RecordingListener listener;

    observer.emit(readySnapshot(1U));
    CHECK(listener.calls == 0U);

    observer.setSnapshotListener(&listener);
    observer.emit(readySnapshot(2U));
    CHECK(listener.calls == 1U);
    CHECK(listener.lastSequence == 2U);

    CHECK(observer.stop());
    observer.emit(readySnapshot(3U));
    CHECK(listener.calls == 1U);
}

TEST_CASE("Observer frame dispatch always calls original first and contains exceptions") {
    std::vector<int> order;
    saors::dispatchGameObserverFrame([&] { order.push_back(1); }, [&] { order.push_back(2); });
    REQUIRE(order.size() == 2U);
    CHECK(order[0] == 1);
    CHECK(order[1] == 2);

    bool captureAfterFailure = false;
    CHECK_NOTHROW(saors::dispatchGameObserverFrame(
        [] { throw std::runtime_error("synthetic original failure"); },
        [&] {
            captureAfterFailure = true;
            throw std::runtime_error("synthetic listener failure");
        }));
    CHECK(captureAfterFailure);
}

TEST_CASE("Null action sink has no simulated execution state") {
    saors::NullRadioActionSink sink;
    saors::RadioActionPlan plan;
    plan.action = saors::RadioActionKind::wouldStart;
    plan.snapshotSequence = 1U;
    plan.onlineAudioWouldBeActive = true;

    sink.submit(plan);
    CHECK_FALSE(sink.simulatedState().active);
}
