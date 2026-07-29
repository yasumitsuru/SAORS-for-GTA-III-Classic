#pragma once

#include "saors_gta3/ExecutableProfile.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace saors {

enum class RadioStationIdentity {
    unknown,
    headRadio,
    doubleClefFm,
    jahRadio,
    riseFm,
    lips106,
    gameFm,
    msxFm,
    flashback956,
    chatterbox109,
    mp3Player,
    policeRadio,
    radioOff,
};

enum class RadioStationEvidenceLevel {
    unverified,
    observedOnce,
    locallyReproduced,
    independentlyReproduced,
};

struct RadioStationObservation {
    std::string sessionId;
    ExecutableProfileId executableProfile{ExecutableProfileId::unsupported};
    std::uint64_t snapshotSequence{0};
    std::uint32_t ordinal{0};
    int rawValue{-1};
    std::optional<RadioStationIdentity> annotatedIdentity;
    std::optional<std::string> visibleLabel;
    std::uint32_t stableFrames{0};
    bool playerInVehicle{false};
};

struct RadioStationEvidenceDocument {
    int schemaVersion{1};
    ExecutableProfileId executableProfile{ExecutableProfileId::unsupported};
    std::string sessionId;
    std::string sourceId;
    std::vector<RadioStationObservation> observations;
};

struct RadioStationMapEntry {
    int rawValue{-1};
    RadioStationIdentity identity{RadioStationIdentity::unknown};
    RadioStationEvidenceLevel evidence{RadioStationEvidenceLevel::unverified};
    std::size_t localSessionCount{0};
    std::size_t independentSourceCount{0};
};

struct RadioStationMapProfile {
    ExecutableProfileId executableProfile{ExecutableProfileId::unsupported};
    std::vector<RadioStationMapEntry> entries;
    std::string evidenceRevision;
};

struct RadioStationEvidenceValidation {
    bool valid{false};
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

struct RadioStationEvidenceParseResult {
    RadioStationEvidenceDocument document;
    RadioStationEvidenceValidation validation;
    bool success{false};
};

struct RadioStationEvidenceComparison {
    bool equivalent{false};
    bool sameExecutableProfile{true};
    std::vector<std::string> conflicts;
    std::vector<std::string> warnings;
};

[[nodiscard]] const char* radioStationIdentityName(RadioStationIdentity identity) noexcept;
[[nodiscard]] std::optional<RadioStationIdentity>
radioStationIdentityFromName(std::string_view name) noexcept;
[[nodiscard]] const char* radioStationEvidenceLevelName(RadioStationEvidenceLevel level) noexcept;
[[nodiscard]] const char* executableProfileIdName(ExecutableProfileId profile) noexcept;
[[nodiscard]] std::optional<ExecutableProfileId>
executableProfileIdFromName(std::string_view name) noexcept;

[[nodiscard]] RadioStationEvidenceValidation
validateRadioStationEvidence(const RadioStationEvidenceDocument& document);

[[nodiscard]] RadioStationEvidenceParseResult
parseRadioStationEvidenceJson(std::string_view json);

[[nodiscard]] std::string
serializeRadioStationEvidenceJson(const RadioStationEvidenceDocument& document);

[[nodiscard]] RadioStationEvidenceComparison compareRadioStationEvidence(
    const RadioStationEvidenceDocument& first,
    const RadioStationEvidenceDocument& second);

[[nodiscard]] RadioStationMapProfile buildRadioStationMap(
    ExecutableProfileId profile,
    const std::vector<RadioStationEvidenceDocument>& documents,
    std::string evidenceRevision = "phase3d-1");

} // namespace saors
