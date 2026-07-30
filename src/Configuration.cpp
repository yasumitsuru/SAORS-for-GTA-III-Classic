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
    } else if (normalizedKey == "resolveremoteplaylists") {
        const auto parsed = parseBoolean(value);
        if (parsed.has_value()) {
            general.resolveRemotePlaylists = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "a boolean");
        }
    } else if (normalizedKey == "playlistconnecttimeoutmilliseconds") {
        const auto parsed = parseUnsigned(value);
        if (parsed.has_value() && *parsed > 0) {
            general.playlistConnectTimeoutMilliseconds = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "a positive integer");
        }
    } else if (normalizedKey == "playlistreceivetimeoutmilliseconds") {
        const auto parsed = parseUnsigned(value);
        if (parsed.has_value() && *parsed > 0) {
            general.playlistReceiveTimeoutMilliseconds = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "a positive integer");
        }
    } else if (normalizedKey == "playlistmaximumbytes") {
        const auto parsed = parseUnsigned(value);
        if (parsed.has_value() && *parsed > 0) {
            general.playlistMaximumBytes = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "a positive integer");
        }
    } else if (normalizedKey == "playlistmaximumentries") {
        const auto parsed = parseUnsigned(value);
        if (parsed.has_value() && *parsed > 0) {
            general.playlistMaximumEntries = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "a positive integer");
        }
    } else if (normalizedKey == "playlistmaximumredirects") {
        const auto parsed = parseUnsigned(value);
        if (parsed.has_value() && *parsed > 0) {
            general.playlistMaximumRedirects = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "a positive integer");
        }
    } else if (normalizedKey == "playlistmaximumdepth") {
        const auto parsed = parseUnsigned(value);
        if (parsed.has_value() && *parsed > 0) {
            general.playlistMaximumDepth = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "a positive integer");
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
    } else if (normalizedKey == "allowhttp") {
        const auto parsed = parseBoolean(value);
        if (parsed.has_value()) {
            station.allowHttp = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key, "a boolean");
        }
    } else if (normalizedKey == "gamestationraw") {
        const auto parsed = parseUnsigned(value);
        if (parsed.has_value() && *parsed <= 255U) {
            station.gameStationRaw = static_cast<int>(*parsed);
        } else {
            addInvalidValueError(result, lineNumber, key, "an integer from 0 through 255");
        }
    } else if (normalizedKey == "gamestationidentity") {
        const auto parsed = radioStationIdentityFromName(value);
        if (parsed.has_value() && *parsed != RadioStationIdentity::unknown) {
            station.gameStationIdentity = *parsed;
        } else {
            addInvalidValueError(result, lineNumber, key,
                                 "a known radio station identity other than unknown");
        }
    } else {
        result.warnings.push_back("line " + std::to_string(lineNumber) + ": unknown station key '" +
                                  key + "'");
    }
}

void assignExperimentalValue(ConfigurationResult& result, const std::string& key,
                             const std::string& value, const std::size_t lineNumber) {
    const auto normalizedKey = lowercase(key);
    auto& experimental = result.value.experimental;
    const auto parsed = parseBoolean(value);
    if (!parsed.has_value()) {
        addInvalidValueError(result, lineNumber, key, "a boolean");
        return;
    }

    if (normalizedKey == "enablegameobserver") {
        experimental.enableGameObserver = *parsed;
    } else if (normalizedKey == "observerdryrun") {
        experimental.observerDryRun = *parsed;
    } else if (normalizedKey == "logstatetransitions") {
        experimental.logStateTransitions = *parsed;
    } else if (normalizedKey == "enableradiocontroller") {
        experimental.enableRadioController = *parsed;
    } else if (normalizedKey == "radiocontrollerdryrun") {
        if (!*parsed) {
            result.warnings.emplace_back(
                "RadioControllerDryRun=false is unavailable in Phase 3C; dry-run remains enabled");
        }
        experimental.radioControllerDryRun = true;
    } else if (normalizedKey == "logradiodecisions") {
        experimental.logRadioDecisions = *parsed;
    } else if (normalizedKey == "enablegameplayaudioexecutor") {
        experimental.enableGameplayAudioExecutor = *parsed;
    } else {
        result.warnings.push_back("line " + std::to_string(lineNumber) +
                                  ": unknown Experimental key '" + key + "'");
    }
}

void assignResearchValue(ConfigurationResult& result, const std::string& key,
                           const std::string& value, const std::size_t lineNumber) {
    const auto normalizedKey = lowercase(key);
    auto& research = result.value.research;
    if (normalizedKey == "enableradiostationmaprecorder" ||
        normalizedKey == "logradiostationobservations") {
        const auto parsed = parseBoolean(value);
        if (!parsed.has_value()) {
            addInvalidValueError(result, lineNumber, key, "a boolean");
            return;
        }
        if (normalizedKey == "enableradiostationmaprecorder") {
            research.enableRadioStationMapRecorder = *parsed;
        } else {
            research.logRadioStationObservations = *parsed;
        }
        return;
    }
    if (normalizedKey == "radiostationminimumstableframes") {
        const auto parsed = parseUnsigned(value);
        if (!parsed.has_value() || *parsed < 1U || *parsed > 600U) {
            addInvalidValueError(result, lineNumber, key, "an integer from 1 through 600");
            return;
        }
        research.radioStationMinimumStableFrames = *parsed;
        return;
    }
    result.warnings.push_back("line " + std::to_string(lineNumber) +
                              ": unknown Research key '" + key + "'");
}

void validateStationBindings(ConfigurationResult& result) {
    std::map<int, std::size_t> enabledRawBindings;
    std::map<RadioStationIdentity, std::size_t> enabledIdentityBindings;
    for (const auto& entry : result.value.stations) {
        const auto& station = entry.second;
        if (station.gameStationRaw && station.gameStationIdentity) {
            result.errors.push_back("station '" + entry.first +
                                    "' cannot set both GameStationRaw and GameStationIdentity");
        }
        if (!station.enabled) {
            continue;
        }
        if (station.gameStationRaw) {
            const auto count = ++enabledRawBindings[*station.gameStationRaw];
            if (count == 2U) {
                result.errors.push_back("duplicate enabled GameStationRaw binding for raw " +
                                        std::to_string(*station.gameStationRaw));
            }
        }
        if (station.gameStationIdentity) {
            const auto count = ++enabledIdentityBindings[*station.gameStationIdentity];
            if (count == 2U) {
                result.errors.push_back(
                    "duplicate enabled GameStationIdentity binding for identity " +
                    std::string(radioStationIdentityName(*station.gameStationIdentity)));
            }
        }
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
        result.errors.emplace_back("configuration file not found");
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
        } else if (lowercase(currentSection) == "experimental") {
            assignExperimentalValue(result, key, value, lineNumber);
        } else if (lowercase(currentSection) == "research") {
            assignResearchValue(result, key, value, lineNumber);
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

    validateStationBindings(result);
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
