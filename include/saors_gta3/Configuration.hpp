#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace saors {

enum class LogLevel {
    trace,
    debug,
    info,
    warning,
    error,
    off,
};

struct GeneralConfiguration {
    bool enabled{true};
    std::uint32_t bufferMilliseconds{3000};
    bool reconnect{true};
    std::uint32_t reconnectDelayMilliseconds{5000};
    bool resolveRemotePlaylists{true};
    std::uint32_t playlistConnectTimeoutMilliseconds{5000};
    std::uint32_t playlistReceiveTimeoutMilliseconds{10000};
    std::size_t playlistMaximumBytes{256U * 1024U};
    std::size_t playlistMaximumEntries{128};
    std::size_t playlistMaximumRedirects{5};
    std::size_t playlistMaximumDepth{3};
    float volumeMultiplier{1.0F};
    LogLevel logLevel{LogLevel::info};
};

struct StationConfiguration {
    bool enabled{false};
    std::string name;
    std::string url;
    bool allowHttp{false};
    std::optional<int> gameStationRaw;
};

struct ExperimentalConfiguration {
    bool enableGameObserver{false};
    bool observerDryRun{true};
    bool logStateTransitions{true};
    bool enableRadioController{false};
    bool radioControllerDryRun{true};
    bool logRadioDecisions{true};
};

struct ConfigurationData {
    GeneralConfiguration general;
    ExperimentalConfiguration experimental;
    std::map<std::string, StationConfiguration> stations;
};

struct ConfigurationResult {
    ConfigurationData value;
    bool success{false};
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

class Configuration {
  public:
    [[nodiscard]] static ConfigurationData defaults();
    [[nodiscard]] static ConfigurationResult load(const std::filesystem::path& path);
};

[[nodiscard]] const char* toString(LogLevel level) noexcept;

} // namespace saors
