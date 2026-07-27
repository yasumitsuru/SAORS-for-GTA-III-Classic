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

[Station.HeadRadio]
Enabled=true
Name=Online Radio
URL=https://example.com/stream
AllowHttp=true
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

    REQUIRE(result.value.stations.count("HeadRadio") == 1);
    const auto& station = result.value.stations.at("HeadRadio");
    CHECK(station.enabled);
    CHECK(station.name == "Online Radio");
    CHECK(station.url == "https://example.com/stream");
    CHECK(station.allowHttp);
}

TEST_CASE("Boolean configuration accepts common explicit forms") {
    const TemporaryIni file{R"ini(
[General]
Enabled=no
Reconnect=ON

[Station.HeadRadio]
Enabled=1
)ini"};

    const auto result = saors::Configuration::load(file.path());

    REQUIRE(result.success);
    CHECK_FALSE(result.value.general.enabled);
    CHECK(result.value.general.reconnect);
    CHECK(result.value.stations.at("HeadRadio").enabled);
}

TEST_CASE("Invalid numbers and booleans are reported and retain defaults") {
    const TemporaryIni file{R"ini(
[General]
Enabled=perhaps
BufferMilliseconds=-10
ReconnectDelayMilliseconds=lots
VolumeMultiplier=NaN

[Station.HeadRadio]
Enabled=maybe
)ini"};

    const auto result = saors::Configuration::load(file.path());

    CHECK_FALSE(result.success);
    CHECK(result.errors.size() == 5);
    CHECK(result.value.general.enabled);
    CHECK(result.value.general.bufferMilliseconds == 3000);
    CHECK(result.value.general.reconnectDelayMilliseconds == 5000);
    CHECK(result.value.general.volumeMultiplier == Catch::Approx(1.0F));
    CHECK_FALSE(result.value.stations.at("HeadRadio").enabled);
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
