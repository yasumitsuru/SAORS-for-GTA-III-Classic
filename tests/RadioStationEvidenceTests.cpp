#include "saors_gta3/RadioStationEvidence.hpp"
#include "saors_gta3/RadioStationMapRegistry.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {
saors::RadioStationEvidenceDocument document(const std::string& session, const std::string& source,
                                             int raw, saors::RadioStationIdentity identity,
                                             const std::string& label) {
    saors::RadioStationEvidenceDocument value;
    value.executableProfile = saors::ExecutableProfileId::gta3_classic_local_candidate;
    value.sessionId = session;
    value.sourceId = source;
    saors::RadioStationObservation observation;
    observation.sessionId = session;
    observation.executableProfile = value.executableProfile;
    observation.snapshotSequence = 20U;
    observation.ordinal = 1U;
    observation.rawValue = raw;
    observation.annotatedIdentity = identity;
    observation.visibleLabel = label;
    observation.stableFrames = 15U;
    observation.playerInVehicle = true;
    value.observations.push_back(observation);
    return value;
}
}

TEST_CASE("Evidence schema validates observations and preserves unknown raw values") {
    auto value = document("session-a", "source-a", 3, saors::RadioStationIdentity::riseFm, "RISE FM");
    const auto validation = saors::validateRadioStationEvidence(value);
    CHECK(validation.valid);
    value.observations.front().rawValue = 10;
    value.observations.front().annotatedIdentity.reset();
    value.observations.front().visibleLabel.reset();
    CHECK(saors::validateRadioStationEvidence(value).valid);
}

TEST_CASE("Evidence validation rejects duplicate ordinals, invalid raw, and label conflicts") {
    auto value = document("session-a", "source-a", 3, saors::RadioStationIdentity::riseFm, "RISE FM");
    value.observations.push_back(value.observations.front());
    value.observations.back().rawValue = 4;
    value.observations.back().visibleLabel = "LIPS 106";
    CHECK_FALSE(saors::validateRadioStationEvidence(value).valid);
    value.observations.back().ordinal = 2U;
    value.observations.back().rawValue = 256;
    CHECK_FALSE(saors::validateRadioStationEvidence(value).valid);
}

TEST_CASE("Evidence comparison detects conflicts and map levels use sessions and sources") {
    auto first = document("session-a", "source-a", 3, saors::RadioStationIdentity::riseFm, "RISE FM");
    auto second = document("session-b", "source-a", 3, saors::RadioStationIdentity::riseFm, "RISE FM");
    auto comparison = saors::compareRadioStationEvidence(first, second);
    CHECK(comparison.equivalent);
    auto map = saors::buildRadioStationMap(first.executableProfile, {first, second});
    REQUIRE(map.entries.size() == 1U);
    CHECK(map.entries.front().evidence == saors::RadioStationEvidenceLevel::locallyReproduced);
    second.sourceId = "source-b";
    map = saors::buildRadioStationMap(first.executableProfile, {first, second});
    CHECK(map.entries.front().evidence == saors::RadioStationEvidenceLevel::independentlyReproduced);
    second.observations.front().annotatedIdentity = saors::RadioStationIdentity::lips106;
    comparison = saors::compareRadioStationEvidence(first, second);
    CHECK_FALSE(comparison.equivalent);
}

TEST_CASE("Evidence JSON round trips and rejects forbidden sensitive fields") {
    const auto value = document("session-a", "source-a", 0, saors::RadioStationIdentity::headRadio, "HEAD RADIO");
    const auto json = saors::serializeRadioStationEvidenceJson(value);
    const auto parsed = saors::parseRadioStationEvidenceJson(json);
    REQUIRE(parsed.success);
    CHECK(parsed.document.observations.front().rawValue == 0);
    const auto rejected = saors::parseRadioStationEvidenceJson("{\"schemaVersion\":1,\"url\":\"https://private\"}");
    CHECK_FALSE(rejected.success);
}

TEST_CASE("Registry has no default inferred station map") {
    CHECK(saors::defaultRadioStationMapRegistry().find(saors::ExecutableProfileId::gta3_classic_local_candidate) == nullptr);
    saors::RadioStationMapProfile invalid;
    invalid.executableProfile = saors::ExecutableProfileId::gta3_classic_local_candidate;
    invalid.evidenceRevision = "test";
    invalid.entries.push_back({10, saors::RadioStationIdentity::unknown,
                               saors::RadioStationEvidenceLevel::unverified, 0U, 0U});
    CHECK_FALSE(saors::isRadioStationMapProfileValid(invalid));
}

TEST_CASE("Evidence validation rejects identity conflicts, unsafe labels, and personal fields") {
    auto value = document("session-a", "source-a", 3, saors::RadioStationIdentity::riseFm, "RISE FM");
    auto conflictingIdentity = value.observations.front();
    conflictingIdentity.ordinal = 2U;
    conflictingIdentity.annotatedIdentity = saors::RadioStationIdentity::lips106;
    value.observations.push_back(conflictingIdentity);
    auto validation = saors::validateRadioStationEvidence(value);
    CHECK_FALSE(validation.valid);

    value.observations.pop_back();
    value.observations.front().visibleLabel.reset();
    validation = saors::validateRadioStationEvidence(value);
    CHECK_FALSE(validation.valid);
    value.observations.front().annotatedIdentity.reset();
    CHECK(saors::validateRadioStationEvidence(value).valid);
    value.observations.front().annotatedIdentity = saors::RadioStationIdentity::riseFm;
    value.observations.front().visibleLabel = "C:\\Users\\private";
    validation = saors::validateRadioStationEvidence(value);
    CHECK_FALSE(validation.valid);

    const auto personal = saors::parseRadioStationEvidenceJson(
        R"json({"schemaVersion":1,"executableProfile":"gta3_classic_local_candidate","sessionId":"session-a","observations":[],"filePath":"C:\Users\private"})json");
    CHECK_FALSE(personal.success);
}

TEST_CASE("Evidence parser rejects unknown and truncated schemas") {
    const auto unknownSchema = saors::parseRadioStationEvidenceJson(
        R"json({"schemaVersion":2,"executableProfile":"gta3_classic_local_candidate","sessionId":"session-a","observations":[]})json");
    CHECK_FALSE(unknownSchema.success);
    const auto truncated = saors::parseRadioStationEvidenceJson(
        R"json({"schemaVersion":1,"executableProfile":"gta3_classic_local_candidate","sessionId":"session-a","observations":[)json");
    CHECK_FALSE(truncated.success);
}

TEST_CASE("Unknown identity and profiles remain explicit and unverified") {
    auto value = document("session-a", "source-a", 10, saors::RadioStationIdentity::unknown, "");
    value.observations.front().visibleLabel.reset();
    CHECK(saors::validateRadioStationEvidence(value).valid);
    const auto map = saors::buildRadioStationMap(value.executableProfile, {value});
    REQUIRE(map.entries.size() == 1U);
    CHECK(map.entries.front().rawValue == 10);
    CHECK(map.entries.front().identity == saors::RadioStationIdentity::unknown);
    CHECK(map.entries.front().evidence == saors::RadioStationEvidenceLevel::unverified);

    value.executableProfile = saors::ExecutableProfileId::gta3_10_us_candidate;
    value.observations.front().executableProfile = value.executableProfile;
    const auto ignored = saors::buildRadioStationMap(
        saors::ExecutableProfileId::gta3_classic_local_candidate, {value});
    CHECK(ignored.entries.empty());
}

TEST_CASE("Evidence comparison reports missing raw values without inventing identities") {
    auto first = document("session-a", "source-a", 3, saors::RadioStationIdentity::riseFm, "RISE FM");
    auto second = document("session-b", "source-a", 4, saors::RadioStationIdentity::lips106, "LIPS 106");
    const auto comparison = saors::compareRadioStationEvidence(first, second);
    CHECK(comparison.equivalent);
    REQUIRE(comparison.warnings.size() == 2U);
    CHECK(comparison.warnings.front().find("absent") != std::string::npos);
    CHECK(comparison.warnings.back().find("absent") != std::string::npos);
}
