#include "saors_gta3/Configuration.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <utility>

namespace saors {
namespace {

std::string trim(std::string value) {
    const auto notSpace = [](const unsigned char character) {
        return std::isspace(character) == 0;
    };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::optional<bool> parseBoolean(const std::string& text) {
    const auto value = lowercase(trim(text));
    if (value == "true" || value == "yes" || value == "on" || value == "1") {
        return true;
    }
    if (value == "false" || value == "no" || value == "off" || value == "0") {
        return false;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> parseUnsigned(const std::string& text) {
    const auto value = trim(text);
    if (value.empty() || value.front() == '-') {
        return std::nullopt;
    }

    try {
        std::size_t parsedCharacters = 0;
        const auto parsed = std::stoull(value, &parsedCharacters, 10);
        if (parsedCharacters != value.size() ||
            parsed > std::numeric_limits<std::uint32_t>::max()) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(parsed);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<float> parseFloat(const std::string& text) {
    const auto value = trim(text);
    try {
        std::size_t parsedCharacters = 0;
        const float parsed = std::stof(value, &parsedCharacters);
        if (parsedCharacters != value.size() || !std::isfinite(parsed) || parsed < 0.0F) {
            return std::nullopt;
        }
        return parsed;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<LogLevel> parseLogLevel(const std::string& text) {
    const auto value = lowercase(trim(text));
    if (value == "trace") {
        return LogLevel::trace;
    }
    if (value == "debug") {
        return LogLevel::debug;
    }
    if (value == "info") {
        return LogLevel::info;
    }
    if (value == "warning" || value == "warn") {
        return LogLevel::warning;
    }
    if (value == "error") {
        return LogLevel::error;
    }
    if (value == "off") {
        return LogLevel::off;
    }
    return std::nullopt;
}

void addInvalidValueError(ConfigurationResult& result, const std::size_t lineNumber,
                          const std::string& key, const std::string& expected) {
    result.errors.push_back("line " + std::to_string(lineNumber) + ": invalid value for '" + key +
                            "' (expected " + expected + ")");
}

void assignGeneralValue(ConfigurationResult& result, const std::string& key,
                        const std::string& value, const std::size_t lineNumber) {
    const auto normalizedKey = lowercase(key);
    auto& general = result.value.general;

    if (normalizedKey == "enabled") {
        const auto parsed = parseBoolean(value);
        if (parsed.has_value()) {
            general.enabled = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "a boolean");
        }
    } else if (normalizedKey == "buffermilliseconds") {
        const auto parsed = parseUnsigned(value);
        if (parsed.has_value()) {
            general.bufferMilliseconds = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "an unsigned 32-bit integer");
        }
    } else if (normalizedKey == "reconnect") {
        const auto parsed = parseBoolean(value);
        if (parsed.has_value()) {
            general.reconnect = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "a boolean");
        }
    } else if (normalizedKey == "reconnectdelaymilliseconds") {
        const auto parsed = parseUnsigned(value);
        if (parsed.has_value()) {
            general.reconnectDelayMilliseconds = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "an unsigned 32-bit integer");
        }
    } else if (normalizedKey == "volumemultiplier") {
        const auto parsed = parseFloat(value);
        if (parsed.has_value()) {
            general.volumeMultiplier = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "a non-negative number");
        }
    } else if (normalizedKey == "loglevel") {
        const auto parsed = parseLogLevel(value);
        if (parsed.has_value()) {
            general.logLevel = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key,
                                 "trace, debug, info, warning, error, or off");
        }
    } else {
        result.warnings.push_back("line " + std::to_string(lineNumber) + ": unknown General key '" +
                                  key + "'");
    }
}

void assignStationValue(ConfigurationResult& result, const std::string& stationKey,
                        const std::string& key, const std::string& value,
                        const std::size_t lineNumber) {
    auto& station = result.value.stations[stationKey];
    const auto normalizedKey = lowercase(key);

    if (normalizedKey == "enabled") {
        const auto parsed = parseBoolean(value);
        if (parsed.has_value()) {
            station.enabled = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "a boolean");
        }
    } else if (normalizedKey == "name") {
        station.name = value;
    } else if (normalizedKey == "url") {
        station.url = value;
    } else {
        result.warnings.push_back("line " + std::to_string(lineNumber) + ": unknown station key '" +
                                  key + "'");
    }
}

} // namespace

ConfigurationData Configuration::defaults() {
    return {};
}

ConfigurationResult Configuration::load(const std::filesystem::path& path) {
    ConfigurationResult result;
    result.value = defaults();

    std::ifstream input(path);
    if (!input) {
        result.errors.push_back("configuration file not found: " + path.string());
        return result;
    }

    std::string currentSection;
    bool foundGeneralSection = false;
    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        auto cleaned = trim(line);
        if (cleaned.empty() || cleaned.front() == ';' || cleaned.front() == '#') {
            continue;
        }

        if (cleaned.front() == '[' && cleaned.back() == ']') {
            currentSection = trim(cleaned.substr(1, cleaned.size() - 2));
            if (lowercase(currentSection) == "general") {
                foundGeneralSection = true;
            }
            continue;
        }

        const auto separator = cleaned.find('=');
        if (separator == std::string::npos) {
            result.errors.push_back("line " + std::to_string(lineNumber) + ": expected key=value");
            continue;
        }

        const auto key = trim(cleaned.substr(0, separator));
        const auto value = trim(cleaned.substr(separator + 1));
        if (key.empty()) {
            result.errors.push_back("line " + std::to_string(lineNumber) + ": key cannot be empty");
            continue;
        }

        if (lowercase(currentSection) == "general") {
            assignGeneralValue(result, key, value, lineNumber);
        } else if (currentSection.rfind("Station.", 0) == 0 && currentSection.size() > 8) {
            assignStationValue(result, currentSection.substr(8), key, value, lineNumber);
        } else if (currentSection.empty()) {
            result.errors.push_back("line " + std::to_string(lineNumber) +
                                    ": key appears before a section");
        } else {
            result.warnings.push_back("line " + std::to_string(lineNumber) +
                                      ": ignored unknown section [" + currentSection + "]");
        }
    }

    if (!foundGeneralSection) {
        result.warnings.emplace_back(
            "[General] section is missing; built-in defaults are being used");
    }
    if (result.value.stations.empty()) {
        result.warnings.emplace_back("no [Station.*] sections were configured");
    }

    result.success = result.errors.empty();
    return result;
}

const char* toString(const LogLevel level) noexcept {
    switch (level) {
    case LogLevel::trace:
        return "trace";
    case LogLevel::debug:
        return "debug";
    case LogLevel::info:
        return "info";
    case LogLevel::warning:
        return "warning";
    case LogLevel::error:
        return "error";
    case LogLevel::off:
        return "off";
    }
    return "unknown";
}

} // namespace saors
