#include "saors_gta3/Configuration.hpp"
#include "saors_gta3/AudioBackend.hpp"
#include "saors_gta3/GameObserver.hpp"
#include "saors_gta3/HttpClient.hpp"
#include "saors_gta3/Logger.hpp"
#include "saors_gta3/PlaylistResolver.hpp"
#include "saors_gta3/RadioActionSink.hpp"
#include "saors_gta3/RadioController.hpp"
#include "saors_gta3/RadioStationMapRegistry.hpp"
#include "saors_gta3/RadioStationResolver.hpp"
#include "saors_gta3/RadioDecisionEngine.hpp"
#include "saors_gta3/StreamManager.hpp"

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
    state.bindingKind = saors::RadioStationBindingKind::raw;
    state.rawStation = 3;
    state.stationKey = "LocalTest";
    state.preferenceVolume = 0.5F;
    state.lastSnapshotSequence = 1U;
    return state;
}

constexpr auto supportedProfile = saors::ExecutableProfileId::gta3_classic_local_candidate;

const saors::RadioStationResolver& resolver() {
    return saors::defaultRadioStationResolver();
}

saors::RadioActionPlan evaluate(const saors::RadioDecisionEngine& engine,
                                const saors::GameStateSnapshot& snapshot,
                                const saors::ConfigurationData& settings,
                                const saors::SimulatedRadioState& previousState) {
    return engine.evaluate(snapshot, settings, supportedProfile, resolver(), previousState);
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

class RecordingPlaybackBackend final : public saors::AudioBackend {
  public:
    bool open(const std::string& url) override {
        ++openCount;
        openedUrls.push_back(url);
        if (throwOpen) {
            throw std::runtime_error("synthetic open exception");
        }
        if (failOpen) {
            error = "synthetic open failure";
            state_ = saors::AudioState::error;
            return false;
        }
        state_ = saors::AudioState::opening;
        return true;
    }

    bool play() override {
        ++playCount;
        if (failPlay) {
            error = "synthetic play failure";
            state_ = saors::AudioState::error;
            return false;
        }
        state_ = saors::AudioState::playing;
        return true;
    }

    bool pause() override {
        ++pauseCount;
        if (failPause) {
            error = "synthetic pause failure";
            state_ = saors::AudioState::error;
            return false;
        }
        state_ = saors::AudioState::paused;
        return true;
    }

    void stop() noexcept override {
        ++stopCount;
        state_ = saors::AudioState::stopped;
    }

    bool setVolume(const float volume) override {
        ++setVolumeCount;
        if (failVolume) {
            error = "synthetic volume failure";
            state_ = saors::AudioState::error;
            return false;
        }
        lastVolume = volume;
        return true;
    }

    bool setBufferMilliseconds(const std::uint32_t milliseconds) override {
        ++setBufferCount;
        lastBuffer = milliseconds;
        return milliseconds > 0;
    }

    [[nodiscard]] saors::AudioState state() const noexcept override {
        return state_;
    }

    [[nodiscard]] std::string lastError() const override {
        return error;
    }

    [[nodiscard]] const char* name() const noexcept override {
        return "recording-playback";
    }

    bool failOpen{false};
    bool throwOpen{false};
    bool failPlay{false};
    bool failPause{false};
    bool failVolume{false};
    int openCount{0};
    int playCount{0};
    int pauseCount{0};
    int stopCount{0};
    int setVolumeCount{0};
    int setBufferCount{0};
    float lastVolume{0.0F};
    std::uint32_t lastBuffer{0};
    std::vector<std::string> openedUrls;
    std::string error;

  private:
    saors::AudioState state_{saors::AudioState::stopped};
};

class FailingHttpClient final : public saors::HttpClient {
  public:
    [[nodiscard]] saors::Result<saors::HttpResponse>
    get(const std::string& url, const saors::HttpRequestOptions& options) noexcept override {
        static_cast<void>(url);
        static_cast<void>(options);
        ++requestCount;
        return saors::Result<saors::HttpResponse>::fail("synthetic connection failure");
    }

    std::size_t requestCount{0};
};

saors::RadioActionPlan playbackPlan(const saors::RadioActionKind action,
                                    const std::uint64_t sequence,
                                    const saors::StationConfiguration& station,
                                    const float volume = 0.5F) {
    saors::RadioActionPlan plan;
    plan.action = action;
    plan.reason = action == saors::RadioActionKind::wouldStop
                      ? saors::RadioDecisionReason::playerOnFoot
                      : saors::RadioDecisionReason::ready;
    plan.bindingKind = saors::RadioStationBindingKind::raw;
    plan.snapshotSequence = sequence;
    plan.rawStation = 3;
    plan.stationKey = "PublicStation";
    plan.preferenceVolume = volume;
    plan.selectedStation = station;
    plan.onlineAudioWouldBeActive = action != saors::RadioActionKind::wouldStop;
    return plan;
}

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
        const auto plan = evaluate(engine, snapshot, settings, active);
        CHECK(plan.action == saors::RadioActionKind::wouldStop);
        CHECK(plan.reason == saors::RadioDecisionReason::observerUnavailable);
    }

    SECTION("game not ready") {
        snapshot.gameReady = false;
        const auto plan = evaluate(engine, snapshot, settings, active);
        CHECK(plan.action == saors::RadioActionKind::wouldStop);
        CHECK(plan.reason == saors::RadioDecisionReason::gameNotReady);
    }

    SECTION("pause state unavailable") {
        snapshot.pauseMenuActive.reset();
        const auto plan = evaluate(engine, snapshot, settings, active);
        CHECK(plan.action == saors::RadioActionKind::wouldStop);
        CHECK(plan.reason == saors::RadioDecisionReason::pauseStateUnavailable);
    }

    SECTION("vehicle state unavailable") {
        snapshot.playerInVehicle.reset();
        const auto plan = evaluate(engine, snapshot, settings, active);
        CHECK(plan.action == saors::RadioActionKind::wouldStop);
        CHECK(plan.reason == saors::RadioDecisionReason::vehicleStateUnavailable);
    }

    SECTION("player on foot") {
        snapshot.playerInVehicle = false;
        const auto plan = evaluate(engine, snapshot, settings, active);
        CHECK(plan.action == saors::RadioActionKind::wouldStop);
        CHECK(plan.reason == saors::RadioDecisionReason::playerOnFoot);
    }

    SECTION("station unavailable") {
        snapshot.radioStationRaw.reset();
        const auto plan = evaluate(engine, snapshot, settings, active);
        CHECK(plan.action == saors::RadioActionKind::wouldStop);
        CHECK(plan.reason == saors::RadioDecisionReason::stationUnavailable);
    }
}

TEST_CASE("Project and controller gates never produce start decisions") {
    const saors::RadioDecisionEngine engine;
    auto settings = configuration();
    const auto snapshot = readySnapshot(2U);

    settings.general.enabled = false;
    auto plan = evaluate(engine, snapshot, settings, {});
    CHECK(plan.action == saors::RadioActionKind::none);
    CHECK(plan.reason == saors::RadioDecisionReason::projectDisabled);

    settings.general.enabled = true;
    settings.experimental.enableRadioController = false;
    plan = evaluate(engine, snapshot, settings, activeState());
    CHECK(plan.action == saors::RadioActionKind::wouldStop);
    CHECK(plan.reason == saors::RadioDecisionReason::controllerDisabled);

    settings.experimental.enableRadioController = true;
    settings.experimental.radioControllerDryRun = false;
    plan = evaluate(engine, snapshot, settings, {});
    CHECK(plan.action == saors::RadioActionKind::none);
    CHECK(plan.reason == saors::RadioDecisionReason::controllerDisabled);
}

TEST_CASE("Explicit station binding failures preserve the original radio") {
    const saors::RadioDecisionEngine engine;
    auto settings = configuration();
    const auto snapshot = readySnapshot(2U);

    settings.stations.clear();
    auto plan = evaluate(engine, snapshot, settings, activeState());
    CHECK(plan.action == saors::RadioActionKind::wouldStop);
    CHECK(plan.reason == saors::RadioDecisionReason::stationIdentityNotBound);

    settings = configuration();
    settings.stations.at("LocalTest").enabled = false;
    plan = evaluate(engine, snapshot, settings, {});
    CHECK(plan.action == saors::RadioActionKind::none);
    CHECK(plan.reason == saors::RadioDecisionReason::stationDisabled);

    settings = configuration();
    settings.stations.at("LocalTest").url.clear();
    plan = evaluate(engine, snapshot, settings, activeState());
    CHECK(plan.action == saors::RadioActionKind::wouldStop);
    CHECK(plan.reason == saors::RadioDecisionReason::stationUrlEmpty);

    auto missingVolume = snapshot;
    missingVolume.radioVolume.reset();
    plan = evaluate(engine, missingVolume, configuration(), activeState());
    CHECK(plan.action == saors::RadioActionKind::wouldStop);
    CHECK(plan.reason == saors::RadioDecisionReason::volumeUnavailable);
}

TEST_CASE("Decision state machine produces every dry-run action deterministically") {
    const saors::RadioDecisionEngine engine;
    auto settings = configuration();
    auto snapshot = readySnapshot(1U);
    saors::SimulatedRadioState state;

    auto plan = evaluate(engine, snapshot, settings, state);
    REQUIRE(plan.action == saors::RadioActionKind::wouldStart);
    CHECK(plan.reason == saors::RadioDecisionReason::ready);
    REQUIRE(plan.selectedStation.has_value());
    CHECK(plan.selectedStation->url == settings.stations.at("LocalTest").url);
    CHECK(plan.stationKey == "LocalTest");
    CHECK(plan.rawStation == 3);
    CHECK(plan.preferenceVolume == Catch::Approx(0.5F));
    saors::applyRadioActionPlan(plan, state);
    CHECK(state.active);
    CHECK_FALSE(state.paused);

    snapshot.sequence = 2U;
    plan = evaluate(engine, snapshot, settings, state);
    CHECK(plan.action == saors::RadioActionKind::none);
    CHECK(plan.reason == saors::RadioDecisionReason::unchanged);
    saors::applyRadioActionPlan(plan, state);

    snapshot.sequence = 3U;
    snapshot.pauseMenuActive = true;
    plan = evaluate(engine, snapshot, settings, state);
    REQUIRE(plan.action == saors::RadioActionKind::wouldPause);
    saors::applyRadioActionPlan(plan, state);
    CHECK(state.paused);

    snapshot.sequence = 4U;
    snapshot.pauseMenuActive = false;
    plan = evaluate(engine, snapshot, settings, state);
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
    plan = evaluate(engine, snapshot, settings, state);
    REQUIRE(plan.action == saors::RadioActionKind::wouldSwitch);
    CHECK(plan.stationKey == "StationB");
    saors::applyRadioActionPlan(plan, state);
    CHECK(state.rawStation == 5);

    snapshot.sequence = 6U;
    snapshot.radioVolume = 0.75F;
    plan = evaluate(engine, snapshot, settings, state);
    REQUIRE(plan.action == saors::RadioActionKind::wouldSetVolume);
    CHECK(plan.preferenceVolume == Catch::Approx(0.75F));
    saors::applyRadioActionPlan(plan, state);
    CHECK(state.preferenceVolume == Catch::Approx(0.75F));

    snapshot.sequence = 7U;
    snapshot.playerInVehicle = false;
    plan = evaluate(engine, snapshot, settings, state);
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
    auto plan = evaluate(engine, snapshot, settings, state);
    CHECK(plan.action == saors::RadioActionKind::none);

    state.preferenceVolume = 0.245F;
    plan = evaluate(engine, snapshot, settings, state);
    CHECK(plan.action == saors::RadioActionKind::none);

    state.preferenceVolume = 0.23F;
    plan = evaluate(engine, snapshot, settings, state);
    CHECK(plan.action == saors::RadioActionKind::wouldSetVolume);
    CHECK(plan.preferenceVolume == Catch::Approx(0.25F));

    settings.general.volumeMultiplier = 4.0F;
    state.preferenceVolume = 0.25F;
    plan = evaluate(engine, snapshot, settings, state);
    REQUIRE(plan.action == saors::RadioActionKind::wouldSetVolume);
    CHECK(plan.preferenceVolume == Catch::Approx(1.0F));
}

TEST_CASE("Out-of-order snapshots cannot mutate simulated state") {
    const saors::RadioDecisionEngine engine;
    const auto settings = configuration();
    auto state = activeState();
    state.lastSnapshotSequence = 10U;
    const auto snapshot = readySnapshot(9U);

    const auto plan = evaluate(engine, snapshot, settings, state);
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

    const auto plan = evaluate(engine, readySnapshot(), settings, {});

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
    start.bindingKind = saors::RadioStationBindingKind::identity;
    start.snapshotSequence = 1U;
    start.rawStation = 3;
    start.stationIdentity = saors::RadioStationIdentity::riseFm;
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
    pause.stationIdentity.reset();
    pause.preferenceVolume.reset();
    sink.submit(pause);

    const auto state = sink.simulatedState();
    CHECK(state.active);
    CHECK(state.paused);
    CHECK(state.stationKey == "StationRaw3");
    CHECK(state.bindingKind == saors::RadioStationBindingKind::identity);
    CHECK(state.stationIdentity == saors::RadioStationIdentity::riseFm);
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
    saors::RadioController controller(settings, supportedProfile, resolver(), sink);

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
    saors::RadioController controller(configuration(), supportedProfile, resolver(), sink);
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

TEST_CASE("Gameplay executor performs each selected-station action without rebinding") {
    auto backend = std::make_unique<RecordingPlaybackBackend>();
    auto* recording = backend.get();
    saors::StreamManager streams(std::move(backend));
    auto settings = configuration();
    settings.general.resolveRemotePlaylists = false;
    settings.general.bufferMilliseconds = 4200U;
    saors::GameplayRadioActionSink sink(streams, settings.general);

    saors::StationConfiguration first;
    first.enabled = true;
    first.url = "https://one.example.invalid/live";
    saors::StationConfiguration second;
    second.enabled = true;
    second.url = "https://two.example.invalid/live";

    sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 1U, first));
    REQUIRE(recording->openedUrls.size() == 1U);
    CHECK(recording->openedUrls.front() == first.url);
    CHECK(recording->lastVolume == Catch::Approx(0.5F));
    CHECK(recording->lastBuffer == 4200U);
    CHECK(sink.simulatedState().active);

    sink.submit(playbackPlan(saors::RadioActionKind::wouldSwitch, 2U, second, 0.75F));
    REQUIRE(recording->openedUrls.size() == 2U);
    CHECK(recording->openedUrls.back() == second.url);
    CHECK(recording->lastVolume == Catch::Approx(0.75F));

    sink.submit(playbackPlan(saors::RadioActionKind::wouldPause, 3U, second));
    CHECK(recording->pauseCount == 1);
    CHECK(sink.simulatedState().paused);

    sink.submit(playbackPlan(saors::RadioActionKind::wouldResume, 4U, second));
    CHECK(recording->playCount == 3);
    CHECK_FALSE(sink.simulatedState().paused);

    sink.submit(playbackPlan(saors::RadioActionKind::wouldSetVolume, 5U, second, 0.25F));
    CHECK(recording->lastVolume == Catch::Approx(0.25F));

    sink.submit(playbackPlan(saors::RadioActionKind::wouldStop, 6U, second));
    CHECK(recording->stopCount >= 3);
    CHECK_FALSE(sink.simulatedState().active);
}

TEST_CASE("Gameplay executor fails closed without retrying after invalid input or backend failure") {
    auto backend = std::make_unique<RecordingPlaybackBackend>();
    auto* recording = backend.get();
    saors::StreamManager streams(std::move(backend));
    auto settings = configuration();
    settings.general.resolveRemotePlaylists = false;
    saors::GameplayRadioActionSink sink(streams, settings.general);

    saors::StationConfiguration station;
    station.enabled = true;
    station.url = "https://failed.example.invalid/live";

    SECTION("empty URL") {
        auto empty = station;
        empty.url.clear();
        CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 1U, empty)));
        CHECK(recording->openCount == 0);

        CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 2U, station)));
        CHECK(recording->openCount == 0);
    }

    SECTION("open failure") {
        recording->failOpen = true;
        CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 1U, station)));
        CHECK(recording->openCount == 1);

        recording->failOpen = false;
        CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 2U, station)));
        CHECK(recording->openCount == 1);
    }

    SECTION("open exception") {
        recording->throwOpen = true;
        CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 1U, station)));
        CHECK(recording->openCount == 1);

        recording->throwOpen = false;
        CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 2U, station)));
        CHECK(recording->openCount == 1);
    }

    SECTION("play failure") {
        recording->failPlay = true;
        CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 1U, station)));
        CHECK(recording->playCount == 1);

        recording->failPlay = false;
        CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 2U, station)));
        CHECK(recording->playCount == 1);
    }

    SECTION("pause failure") {
        sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 1U, station));
        REQUIRE(sink.simulatedState().active);
        recording->failPause = true;
        CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldPause, 2U, station)));
        CHECK(recording->pauseCount == 1);

        recording->failPause = false;
        CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 3U, station)));
        CHECK(recording->openCount == 1);
    }

    SECTION("volume failure") {
        sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 1U, station));
        REQUIRE(sink.simulatedState().active);
        recording->failVolume = true;
        CHECK_NOTHROW(
            sink.submit(playbackPlan(saors::RadioActionKind::wouldSetVolume, 2U, station, 0.25F)));
        CHECK(recording->setVolumeCount == 2);

        recording->failVolume = false;
        CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 3U, station)));
        CHECK(recording->openCount == 1);
    }

    SECTION("resume failure") {
        sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 1U, station));
        sink.submit(playbackPlan(saors::RadioActionKind::wouldPause, 2U, station));
        REQUIRE(sink.simulatedState().paused);
        recording->failPlay = true;
        CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldResume, 3U, station)));
        CHECK(recording->playCount == 2);

        recording->failPlay = false;
        CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 4U, station)));
        CHECK(recording->openCount == 1);
    }

    CHECK(streams.state() == saors::AudioState::stopped);
    CHECK_FALSE(sink.simulatedState().active);
}

TEST_CASE("Gameplay executor stops cleanly when stream resolution cannot connect") {
    auto backend = std::make_unique<RecordingPlaybackBackend>();
    auto* recording = backend.get();
    auto client = std::make_shared<FailingHttpClient>();
    auto resolver = std::make_shared<saors::PlaylistResolver>(client);
    saors::StreamManager streams(std::move(backend), std::move(resolver));
    auto settings = configuration();
    settings.general.resolveRemotePlaylists = true;
    saors::GameplayRadioActionSink sink(streams, settings.general);

    saors::StationConfiguration station;
    station.enabled = true;
    station.url = "https://connection.example.invalid/live";

    CHECK_NOTHROW(sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 1U, station)));
    CHECK(client->requestCount == 1U);
    CHECK(recording->openCount == 0);
    CHECK(streams.state() == saors::AudioState::stopped);
    CHECK_FALSE(sink.simulatedState().active);
}

TEST_CASE("Gameplay executor ignores out-of-order plans and stops during teardown") {
    auto backend = std::make_unique<RecordingPlaybackBackend>();
    auto* recording = backend.get();
    saors::StreamManager streams(std::move(backend));
    auto settings = configuration();
    settings.general.resolveRemotePlaylists = false;
    saors::StationConfiguration station;
    station.enabled = true;
    station.url = "https://teardown.example.invalid/live";

    {
        saors::GameplayRadioActionSink sink(streams, settings.general);
        sink.submit(playbackPlan(saors::RadioActionKind::wouldStart, 2U, station));
        sink.submit(playbackPlan(saors::RadioActionKind::wouldStop, 1U, station));
        CHECK(sink.simulatedState().active);
    }

    CHECK(recording->stopCount >= 2);
    CHECK(streams.state() == saors::AudioState::stopped);
}

TEST_CASE("Raw bindings take precedence and remain authoritative when disabled") {
    const saors::RadioDecisionEngine engine;
    auto settings = configuration();

    saors::StationConfiguration identityBinding;
    identityBinding.enabled = true;
    identityBinding.url = "https://example.invalid/identity";
    identityBinding.gameStationIdentity = saors::RadioStationIdentity::riseFm;
    settings.stations.emplace("IdentityBinding", std::move(identityBinding));

    const auto rawPlan = evaluate(engine, readySnapshot(2U), settings, {});
    REQUIRE(rawPlan.action == saors::RadioActionKind::wouldStart);
    CHECK(rawPlan.bindingKind == saors::RadioStationBindingKind::raw);
    CHECK(rawPlan.stationKey == "LocalTest");
    CHECK_FALSE(rawPlan.stationIdentity);

    const auto unsupportedRawPlan = engine.evaluate(
        readySnapshot(2U), settings, saors::ExecutableProfileId::unsupported, resolver(), {});
    CHECK(unsupportedRawPlan.action == saors::RadioActionKind::wouldStart);
    CHECK(unsupportedRawPlan.bindingKind == saors::RadioStationBindingKind::raw);
    CHECK(unsupportedRawPlan.reason == saors::RadioDecisionReason::ready);

    settings.stations.at("LocalTest").enabled = false;
    const auto disabledRawPlan = evaluate(engine, readySnapshot(2U), settings, {});
    CHECK(disabledRawPlan.action == saors::RadioActionKind::none);
    CHECK(disabledRawPlan.reason == saors::RadioDecisionReason::stationDisabled);
    CHECK(disabledRawPlan.bindingKind == saors::RadioStationBindingKind::raw);
    CHECK_FALSE(disabledRawPlan.stationIdentity);
}

TEST_CASE("Identity bindings require exact resolver results without profile fallback") {
    const saors::RadioDecisionEngine engine;
    auto settings = configuration();
    settings.stations.clear();

    saors::StationConfiguration identityBinding;
    identityBinding.enabled = true;
    identityBinding.url = "https://example.invalid/identity";
    identityBinding.gameStationIdentity = saors::RadioStationIdentity::riseFm;
    settings.stations.emplace("RiseIdentity", std::move(identityBinding));

    const auto resolvedPlan = evaluate(engine, readySnapshot(2U), settings, {});
    REQUIRE(resolvedPlan.action == saors::RadioActionKind::wouldStart);
    CHECK(resolvedPlan.bindingKind == saors::RadioStationBindingKind::identity);
    CHECK(resolvedPlan.stationIdentity == saors::RadioStationIdentity::riseFm);
    CHECK(resolvedPlan.stationKey == "RiseIdentity");

    const auto unsupportedPlan = engine.evaluate(
        readySnapshot(2U), settings, saors::ExecutableProfileId::unsupported, resolver(), {});
    CHECK(unsupportedPlan.action == saors::RadioActionKind::none);
    CHECK(unsupportedPlan.reason == saors::RadioDecisionReason::stationProfileUnsupported);
    CHECK_FALSE(unsupportedPlan.stationIdentity);

    settings.stations.clear();
    const auto unboundPlan = evaluate(engine, readySnapshot(2U), settings, {});
    CHECK(unboundPlan.action == saors::RadioActionKind::none);
    CHECK(unboundPlan.reason == saors::RadioDecisionReason::stationIdentityNotBound);
    CHECK(unboundPlan.stationIdentity == saors::RadioStationIdentity::riseFm);

    settings = configuration();
    settings.stations.clear();
    identityBinding.enabled = false;
    settings.stations.emplace("DisabledIdentity", std::move(identityBinding));
    const auto disabledPlan = evaluate(engine, readySnapshot(2U), settings, {});
    CHECK(disabledPlan.action == saors::RadioActionKind::none);
    CHECK(disabledPlan.reason == saors::RadioDecisionReason::stationDisabled);
    CHECK(disabledPlan.bindingKind == saors::RadioStationBindingKind::identity);
    CHECK(disabledPlan.stationIdentity == saors::RadioStationIdentity::riseFm);

    settings.stations.at("DisabledIdentity").enabled = true;
    settings.stations.at("DisabledIdentity").url.clear();
    const auto urlPlan = evaluate(engine, readySnapshot(2U), settings, {});
    CHECK(urlPlan.action == saors::RadioActionKind::none);
    CHECK(urlPlan.reason == saors::RadioDecisionReason::stationUrlEmpty);
}

TEST_CASE("Unknown raw ten cannot activate police radio identity binding") {
    const saors::RadioDecisionEngine engine;
    auto settings = configuration();
    settings.stations.clear();

    saors::StationConfiguration policeBinding;
    policeBinding.enabled = true;
    policeBinding.url = "https://example.invalid/police";
    policeBinding.gameStationIdentity = saors::RadioStationIdentity::policeRadio;
    settings.stations.emplace("PoliceIdentity", std::move(policeBinding));

    auto snapshot = readySnapshot(2U);
    snapshot.radioStationRaw = 10;
    const auto plan = evaluate(engine, snapshot, settings, {});

    CHECK(plan.action == saors::RadioActionKind::none);
    CHECK(plan.reason == saors::RadioDecisionReason::stationIdentityUnknown);
    CHECK(plan.bindingKind == saors::RadioStationBindingKind::none);
    CHECK_FALSE(plan.stationIdentity);
    CHECK_FALSE(plan.stationKey);
}

TEST_CASE("Decision engine reports raw validity and supports injected registries") {
    const saors::RadioDecisionEngine engine;
    auto settings = configuration();

    auto invalidSnapshot = readySnapshot(2U);
    invalidSnapshot.radioStationRaw = 256;
    const auto invalidPlan = evaluate(engine, invalidSnapshot, settings, {});
    CHECK(invalidPlan.reason == saors::RadioDecisionReason::stationRawInvalid);
    CHECK(invalidPlan.rawStation == 256);

    settings.stations.clear();
    saors::StationConfiguration identityBinding;
    identityBinding.enabled = true;
    identityBinding.url = "https://example.invalid/custom";
    identityBinding.gameStationIdentity = saors::RadioStationIdentity::riseFm;
    settings.stations.emplace("CustomIdentity", std::move(identityBinding));

    const saors::RadioStationMapRegistry registry({
        {supportedProfile,
         {{42, saors::RadioStationIdentity::riseFm,
           saors::RadioStationEvidenceLevel::locallyReproduced, 1U, 1U}},
         "test-registry"},
    });
    const saors::RadioStationResolver injectedResolver(registry);
    auto customSnapshot = readySnapshot(2U);
    customSnapshot.radioStationRaw = 42;
    const auto injectedPlan =
        engine.evaluate(customSnapshot, settings, supportedProfile, injectedResolver, {});

    REQUIRE(injectedPlan.action == saors::RadioActionKind::wouldStart);
    CHECK(injectedPlan.bindingKind == saors::RadioStationBindingKind::identity);
    CHECK(injectedPlan.stationIdentity == saors::RadioStationIdentity::riseFm);
    CHECK(injectedPlan.stationKey == "CustomIdentity");
}

TEST_CASE("Binding metadata participates in simulated state and transition equality") {
    saors::RadioActionPlan plan;
    plan.action = saors::RadioActionKind::wouldStart;
    plan.reason = saors::RadioDecisionReason::ready;
    plan.bindingKind = saors::RadioStationBindingKind::identity;
    plan.snapshotSequence = 1U;
    plan.rawStation = 3;
    plan.stationIdentity = saors::RadioStationIdentity::riseFm;
    plan.stationKey = "RiseIdentity";
    plan.preferenceVolume = 0.5F;
    plan.onlineAudioWouldBeActive = true;

    auto differentBinding = plan;
    differentBinding.bindingKind = saors::RadioStationBindingKind::raw;
    CHECK_FALSE(saors::sameRadioActionTransition(plan, differentBinding));
    CHECK(std::string(saors::radioStationBindingKindName(plan.bindingKind)) == "identity");

    saors::SimulatedRadioState state;
    saors::applyRadioActionPlan(plan, state);
    CHECK(state.bindingKind == saors::RadioStationBindingKind::identity);
    CHECK(state.stationIdentity == saors::RadioStationIdentity::riseFm);

    auto stopPlan = plan;
    stopPlan.action = saors::RadioActionKind::wouldStop;
    stopPlan.snapshotSequence = 2U;
    saors::applyRadioActionPlan(stopPlan, state);
    CHECK(state.bindingKind == saors::RadioStationBindingKind::none);
    CHECK_FALSE(state.stationIdentity);
}
