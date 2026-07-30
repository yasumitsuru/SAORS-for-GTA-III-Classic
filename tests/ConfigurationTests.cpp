#include "saors_gta3/Configuration.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

class TemporaryIni {
  public:
    explicit TemporaryIni(const std::string& contents) {
        static std::atomic<unsigned long> sequence{0};
        path_ = std::filesystem::temp_directory_path() /
                ("saors-configuration-test-" + std::to_string(sequence.fetch_add(1)) + ".ini");
        std::ofstream output(path_, std::ios::binary);
        output << contents;
    }

    ~TemporaryIni() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    TemporaryIni(const TemporaryIni&) = delete;
    TemporaryIni& operator=(const TemporaryIni&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("Default configuration is safe and enabled") {
    const auto configuration = saors::Configuration::defaults();

    CHECK(configuration.general.enabled);
    CHECK(configuration.general.bufferMilliseconds == 3000);
    CHECK(configuration.general.reconnect);
    CHECK(configuration.general.reconnectDelayMilliseconds == 5000);
    CHECK(configuration.general.resolveRemotePlaylists);
    CHECK(configuration.general.playlistConnectTimeoutMilliseconds == 5000);
    CHECK(configuration.general.playlistReceiveTimeoutMilliseconds == 10000);
    CHECK(configuration.general.playlistMaximumBytes == 262144);
    CHECK(configuration.general.playlistMaximumEntries == 128);
    CHECK(configuration.general.playlistMaximumRedirects == 5);
    CHECK(configuration.general.playlistMaximumDepth == 3);
    CHECK(configuration.general.volumeMultiplier == Catch::Approx(1.0F));
    CHECK(configuration.general.logLevel == saors::LogLevel::info);
    CHECK_FALSE(configuration.experimental.enableGameObserver);
    CHECK(configuration.experimental.observerDryRun);
    CHECK(configuration.experimental.logStateTransitions);
    CHECK_FALSE(configuration.experimental.enableRadioController);
    CHECK(configuration.experimental.radioControllerDryRun);
    CHECK(configuration.experimental.logRadioDecisions);
    CHECK_FALSE(configuration.experimental.enableGameplayAudioExecutor);
    CHECK_FALSE(configuration.experimental.muteOriginalRadioDuringGameplayAudio);
    CHECK_FALSE(configuration.research.enableRadioStationMapRecorder);
    CHECK(configuration.research.radioStationMinimumStableFrames == 15U);
    CHECK_FALSE(configuration.research.logRadioStationObservations);
    CHECK(configuration.stations.empty());
}

TEST_CASE("Valid configuration loads general values and stations") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true
BufferMilliseconds=2500
Reconnect=false
ReconnectDelayMilliseconds=7500
ResolveRemotePlaylists=false
PlaylistConnectTimeoutMilliseconds=1000
PlaylistReceiveTimeoutMilliseconds=2000
PlaylistMaximumBytes=4096
PlaylistMaximumEntries=10
PlaylistMaximumRedirects=2
PlaylistMaximumDepth=2
VolumeMultiplier=0.75
LogLevel=debug

[Experimental]
EnableGameObserver=true
ObserverDryRun=false
LogStateTransitions=false
EnableRadioController=true
RadioControllerDryRun=true
LogRadioDecisions=false
EnableGameplayAudioExecutor=true
MuteOriginalRadioDuringGameplayAudio=true

[Station.HeadRadio]
Enabled=true
Name=Online Radio
URL=https://example.com/stream
AllowHttp=true
GameStationRaw=0
)ini"};

    const auto result = saors::Configuration::load(file.path());

    REQUIRE(result.success);
    CHECK(result.errors.empty());
    CHECK(result.value.general.enabled);
    CHECK(result.value.general.bufferMilliseconds == 2500);
    CHECK_FALSE(result.value.general.reconnect);
    CHECK(result.value.general.reconnectDelayMilliseconds == 7500);
    CHECK_FALSE(result.value.general.resolveRemotePlaylists);
    CHECK(result.value.general.playlistConnectTimeoutMilliseconds == 1000);
    CHECK(result.value.general.playlistReceiveTimeoutMilliseconds == 2000);
    CHECK(result.value.general.playlistMaximumBytes == 4096);
    CHECK(result.value.general.playlistMaximumEntries == 10);
    CHECK(result.value.general.playlistMaximumRedirects == 2);
    CHECK(result.value.general.playlistMaximumDepth == 2);
    CHECK(result.value.general.volumeMultiplier == Catch::Approx(0.75F));
    CHECK(result.value.general.logLevel == saors::LogLevel::debug);
    CHECK(result.value.experimental.enableGameObserver);
    CHECK_FALSE(result.value.experimental.observerDryRun);
    CHECK_FALSE(result.value.experimental.logStateTransitions);
    CHECK(result.value.experimental.enableRadioController);
    CHECK(result.value.experimental.radioControllerDryRun);
    CHECK_FALSE(result.value.experimental.logRadioDecisions);
    CHECK(result.value.experimental.enableGameplayAudioExecutor);
    CHECK(result.value.experimental.muteOriginalRadioDuringGameplayAudio);

    REQUIRE(result.value.stations.count("HeadRadio") == 1);
    const auto& station = result.value.stations.at("HeadRadio");
    CHECK(station.enabled);
    CHECK(station.name == "Online Radio");
    CHECK(station.url == "https://example.com/stream");
    CHECK(station.allowHttp);
    REQUIRE(station.gameStationRaw);
    CHECK(*station.gameStationRaw == 0);
}

TEST_CASE("Boolean configuration accepts common explicit forms") {
    const TemporaryIni file{R"ini(
[General]
Enabled=no
Reconnect=ON

[Experimental]
EnableGameObserver=0
ObserverDryRun=yes

[Station.HeadRadio]
Enabled=1
)ini"};

    const auto result = saors::Configuration::load(file.path());

    REQUIRE(result.success);
    CHECK_FALSE(result.value.general.enabled);
    CHECK(result.value.general.reconnect);
    CHECK_FALSE(result.value.experimental.enableGameObserver);
    CHECK(result.value.experimental.observerDryRun);
    CHECK(result.value.stations.at("HeadRadio").enabled);
}

TEST_CASE("Invalid numbers and booleans are reported and retain defaults") {
    const TemporaryIni file{R"ini(
[General]
Enabled=perhaps
BufferMilliseconds=-10
ReconnectDelayMilliseconds=lots
VolumeMultiplier=NaN

[Experimental]
EnableGameObserver=perhaps

[Station.HeadRadio]
Enabled=maybe
)ini"};

    const auto result = saors::Configuration::load(file.path());

    CHECK_FALSE(result.success);
    CHECK(result.errors.size() == 6);
    CHECK(result.value.general.enabled);
    CHECK(result.value.general.bufferMilliseconds == 3000);
    CHECK(result.value.general.reconnectDelayMilliseconds == 5000);
    CHECK(result.value.general.volumeMultiplier == Catch::Approx(1.0F));
    CHECK_FALSE(result.value.stations.at("HeadRadio").enabled);
}

TEST_CASE("Unknown experimental keys are ignored with a warning") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Experimental]
EnableGameObserver=false
UnknownObserverOption=true
)ini"};

    const auto result = saors::Configuration::load(file.path());

    REQUIRE(result.success);
    CHECK_FALSE(result.value.experimental.enableGameObserver);
    REQUIRE_FALSE(result.warnings.empty());
    CHECK(result.warnings.front().find("UnknownObserverOption") != std::string::npos);
}

TEST_CASE("Missing sections use defaults and produce warnings") {
    const TemporaryIni file{"[Unrelated]\nValue=ignored\n"};

    const auto result = saors::Configuration::load(file.path());

    REQUIRE(result.success);
    CHECK(result.value.general.bufferMilliseconds == 3000);
    CHECK(result.value.stations.empty());
    CHECK(result.warnings.size() >= 2);
}

TEST_CASE("Missing configuration file is an explicit error") {
    const auto path = std::filesystem::temp_directory_path() / "saors-file-that-does-not-exist.ini";
    std::error_code error;
    std::filesystem::remove(path, error);

    const auto result = saors::Configuration::load(path);

    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors.front().find("not found") != std::string::npos);
    CHECK(result.value.general.enabled);
}

TEST_CASE("Keys before a section are rejected") {
    const TemporaryIni file{"Enabled=true\n"};

    const auto result = saors::Configuration::load(file.path());

    CHECK_FALSE(result.success);
    REQUIRE_FALSE(result.errors.empty());
    CHECK(result.errors.front().find("before a section") != std::string::npos);
}

TEST_CASE("Playlist limits reject zero and retain safe defaults") {
    const TemporaryIni file{R"ini(
[General]
PlaylistConnectTimeoutMilliseconds=0
PlaylistReceiveTimeoutMilliseconds=0
PlaylistMaximumBytes=0
PlaylistMaximumEntries=0
PlaylistMaximumRedirects=0
PlaylistMaximumDepth=0

[Station.HeadRadio]
AllowHttp=perhaps
)ini"};

    const auto result = saors::Configuration::load(file.path());

    CHECK_FALSE(result.success);
    CHECK(result.errors.size() == 7);
    CHECK(result.value.general.playlistConnectTimeoutMilliseconds == 5000);
    CHECK(result.value.general.playlistReceiveTimeoutMilliseconds == 10000);
    CHECK(result.value.general.playlistMaximumBytes == 262144);
    CHECK(result.value.general.playlistMaximumEntries == 128);
    CHECK(result.value.general.playlistMaximumRedirects == 5);
    CHECK(result.value.general.playlistMaximumDepth == 3);
    CHECK_FALSE(result.value.stations.at("HeadRadio").allowHttp);
}

TEST_CASE("Raw station bindings accept boundaries and remain optional") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Station.Unbound]
Enabled=true

[Station.Zero]
Enabled=false
GameStationRaw=0

[Station.Maximum]
Enabled=true
GameStationRaw=255
)ini"};

    const auto result = saors::Configuration::load(file.path());

    REQUIRE(result.success);
    CHECK_FALSE(result.value.stations.at("Unbound").gameStationRaw);
    REQUIRE(result.value.stations.at("Zero").gameStationRaw);
    CHECK(*result.value.stations.at("Zero").gameStationRaw == 0);
    REQUIRE(result.value.stations.at("Maximum").gameStationRaw);
    CHECK(*result.value.stations.at("Maximum").gameStationRaw == 255);
}

TEST_CASE("Raw station bindings reject negative overflow and text") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Station.Negative]
GameStationRaw=-1

[Station.Overflow]
GameStationRaw=256

[Station.Text]
GameStationRaw=unknown
)ini"};

    const auto result = saors::Configuration::load(file.path());

    CHECK_FALSE(result.success);
    CHECK(result.errors.size() == 3U);
    CHECK_FALSE(result.value.stations.at("Negative").gameStationRaw);
    CHECK_FALSE(result.value.stations.at("Overflow").gameStationRaw);
    CHECK_FALSE(result.value.stations.at("Text").gameStationRaw);
}

TEST_CASE("Duplicate enabled raw bindings are configuration errors") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Station.First]
Enabled=true
GameStationRaw=7

[Station.Second]
Enabled=true
GameStationRaw=7
)ini"};

    const auto result = saors::Configuration::load(file.path());

    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1U);
    CHECK(result.errors.front() == "duplicate enabled GameStationRaw binding for raw 7");
}

TEST_CASE("Disabled stations do not create duplicate binding errors") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Station.First]
Enabled=true
GameStationRaw=7

[Station.Disabled]
Enabled=false
GameStationRaw=7
)ini"};

    const auto result = saors::Configuration::load(file.path());

    CHECK(result.success);
    CHECK(result.errors.empty());
}

TEST_CASE("Real radio execution requests are refused and remain dry-run") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Experimental]
EnableRadioController=true
RadioControllerDryRun=false
LogRadioDecisions=true
)ini"};

    const auto result = saors::Configuration::load(file.path());

    REQUIRE(result.success);
    CHECK(result.value.experimental.enableRadioController);
    CHECK(result.value.experimental.radioControllerDryRun);
    CHECK(result.value.experimental.logRadioDecisions);
    REQUIRE_FALSE(result.warnings.empty());
    CHECK(result.warnings.front().find("remains enabled") != std::string::npos);
}

TEST_CASE("Gameplay audio executor remains an explicit opt-in") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Experimental]
EnableGameplayAudioExecutor=true
MuteOriginalRadioDuringGameplayAudio=true
)ini"};

    const auto result = saors::Configuration::load(file.path());

    REQUIRE(result.success);
    CHECK(result.value.experimental.enableGameplayAudioExecutor);
    CHECK(result.value.experimental.muteOriginalRadioDuringGameplayAudio);
}

TEST_CASE("Legacy INI files remain compatible without raw bindings") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Station.Legacy]
Enabled=true
Name=Legacy station
URL=https://example.invalid/stream
)ini"};

    const auto result = saors::Configuration::load(file.path());

    REQUIRE(result.success);
    CHECK_FALSE(result.value.stations.at("Legacy").gameStationRaw);
    CHECK_FALSE(result.value.experimental.enableRadioController);
    CHECK(result.value.experimental.radioControllerDryRun);
}

TEST_CASE("Research configuration validates safe opt-in limits") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Research]
EnableRadioStationMapRecorder=true
RadioStationMinimumStableFrames=600
LogRadioStationObservations=true
)ini"};

    const auto result = saors::Configuration::load(file.path());

    REQUIRE(result.success);
    CHECK(result.value.research.enableRadioStationMapRecorder);
    CHECK(result.value.research.radioStationMinimumStableFrames == 600U);
    CHECK(result.value.research.logRadioStationObservations);
}

TEST_CASE("Research configuration rejects zero, overflow, and invalid booleans") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Research]
EnableRadioStationMapRecorder=maybe
RadioStationMinimumStableFrames=0
LogRadioStationObservations=unknown
)ini"};

    const auto result = saors::Configuration::load(file.path());

    CHECK_FALSE(result.success);
    CHECK(result.errors.size() == 3U);
    CHECK_FALSE(result.value.research.enableRadioStationMapRecorder);
    CHECK(result.value.research.radioStationMinimumStableFrames == 15U);
    CHECK_FALSE(result.value.research.logRadioStationObservations);
}

TEST_CASE("Research configuration rejects values above the stable-frame limit") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Research]
RadioStationMinimumStableFrames=601
)ini"};

    const auto result = saors::Configuration::load(file.path());

    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1U);
    CHECK(result.value.research.radioStationMinimumStableFrames == 15U);
}

TEST_CASE("Identity station binding parses normalized values and surrounding spaces") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Station.Rise]
Enabled=true
GameStationIdentity=   riseFm
)ini"};

    const auto result = saors::Configuration::load(file.path());

    REQUIRE(result.success);
    const auto& station = result.value.stations.at("Rise");
    REQUIRE(station.gameStationIdentity);
    CHECK(*station.gameStationIdentity == saors::RadioStationIdentity::riseFm);
    CHECK_FALSE(station.gameStationRaw);
}

TEST_CASE("Identity station bindings reject invalid and unknown values") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Station.Invalid]
GameStationIdentity=notAStation

[Station.Unknown]
GameStationIdentity=unknown
)ini"};

    const auto result = saors::Configuration::load(file.path());

    CHECK_FALSE(result.success);
    CHECK(result.errors.size() == 2U);
    CHECK_FALSE(result.value.stations.at("Invalid").gameStationIdentity);
    CHECK_FALSE(result.value.stations.at("Unknown").gameStationIdentity);
}

TEST_CASE("Enabled identity station bindings must be unique") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Station.First]
Enabled=true
GameStationIdentity=riseFm

[Station.Second]
Enabled=true
GameStationIdentity=riseFm
)ini"};

    const auto result = saors::Configuration::load(file.path());

    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1U);
    CHECK(result.errors.front() ==
          "duplicate enabled GameStationIdentity binding for identity riseFm");
}

TEST_CASE("Disabled identity station bindings may duplicate enabled bindings") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Station.First]
Enabled=true
GameStationIdentity=riseFm

[Station.Disabled]
Enabled=false
GameStationIdentity=riseFm
)ini"};

    const auto result = saors::Configuration::load(file.path());

    CHECK(result.success);
    CHECK(result.errors.empty());
}

TEST_CASE("Raw and identity bindings are mutually exclusive per station") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Station.Invalid]
Enabled=false
GameStationRaw=3
GameStationIdentity=riseFm
)ini"};

    const auto result = saors::Configuration::load(file.path());

    CHECK_FALSE(result.success);
    REQUIRE(result.errors.size() == 1U);
    CHECK(result.errors.front() ==
          "station 'Invalid' cannot set both GameStationRaw and GameStationIdentity");
}

TEST_CASE("Raw only and unbound station configurations remain compatible") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Station.RawOnly]
Enabled=true
GameStationRaw=3

[Station.Unbound]
Enabled=true
)ini"};

    const auto result = saors::Configuration::load(file.path());

    REQUIRE(result.success);
    CHECK(result.value.stations.at("RawOnly").gameStationRaw == 3);
    CHECK_FALSE(result.value.stations.at("RawOnly").gameStationIdentity);
    CHECK_FALSE(result.value.stations.at("Unbound").gameStationRaw);
    CHECK_FALSE(result.value.stations.at("Unbound").gameStationIdentity);
}

TEST_CASE("Police radio identity parsing does not create a raw ten binding") {
    const TemporaryIni file{R"ini(
[General]
Enabled=true

[Station.Police]
Enabled=true
GameStationIdentity=policeRadio
)ini"};

    const auto result = saors::Configuration::load(file.path());

    REQUIRE(result.success);
    const auto& station = result.value.stations.at("Police");
    REQUIRE(station.gameStationIdentity);
    CHECK(*station.gameStationIdentity == saors::RadioStationIdentity::policeRadio);
    CHECK_FALSE(station.gameStationRaw);
}
