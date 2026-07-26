#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
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
    float volumeMultiplier{1.0F};
    LogLevel logLevel{LogLevel::info};
};

struct StationConfiguration {
    bool enabled{false};
    std::string name;
    std::string url;
};

struct ConfigurationData {
    GeneralConfiguration general;
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
