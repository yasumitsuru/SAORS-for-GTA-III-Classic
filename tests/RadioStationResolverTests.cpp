#include "saors_gta3/RadioStationResolver.hpp"

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

namespace {

void checkAbsentIdentityAndEvidence(const saors::RadioStationResolution& result) {
    CHECK_FALSE(result.identity.has_value());
    CHECK_FALSE(result.evidence.has_value());
}

} // namespace

TEST_CASE("Default radio station resolver resolves only the reviewed exact profile") {
    const auto& resolver = saors::defaultRadioStationResolver();
    const std::vector<std::pair<int, saors::RadioStationIdentity>> expected{
        {0, saors::RadioStationIdentity::headRadio},
        {1, saors::RadioStationIdentity::doubleClefFm},
        {2, saors::RadioStationIdentity::jahRadio},
        {3, saors::RadioStationIdentity::riseFm},
        {4, saors::RadioStationIdentity::lips106},
        {5, saors::RadioStationIdentity::gameFm},
        {6, saors::RadioStationIdentity::msxFm},
        {7, saors::RadioStationIdentity::flashback956},
        {8, saors::RadioStationIdentity::chatterbox109},
        {9, saors::RadioStationIdentity::mp3Player},
        {11, saors::RadioStationIdentity::radioOff},
    };

    for (const auto& mapping : expected) {
        const auto result = resolver.resolve(saors::ExecutableProfileId::gta3_classic_local_candidate,
                                             mapping.first);
        CHECK(result.status == saors::RadioStationResolutionStatus::resolved);
        CHECK(result.executableProfile == saors::ExecutableProfileId::gta3_classic_local_candidate);
        CHECK(result.rawValue == mapping.first);
        REQUIRE(result.identity);
        REQUIRE(result.evidence);
        CHECK(*result.identity == mapping.second);
        CHECK(*result.evidence == saors::RadioStationEvidenceLevel::locallyReproduced);
    }
}

TEST_CASE("Radio station resolver keeps valid unmapped raws explicitly unknown") {
    const auto& resolver = saors::defaultRadioStationResolver();
    for (const auto rawValue : {10, 12, 255}) {
        const auto result = resolver.resolve(saors::ExecutableProfileId::gta3_classic_local_candidate,
                                             rawValue);
        CHECK(result.status == saors::RadioStationResolutionStatus::unknownRaw);
        CHECK(result.executableProfile == saors::ExecutableProfileId::gta3_classic_local_candidate);
        CHECK(result.rawValue == rawValue);
        checkAbsentIdentityAndEvidence(result);
        CHECK(result.identity != saors::RadioStationIdentity::policeRadio);
        CHECK(result.identity != saors::RadioStationIdentity::radioOff);
    }
}

TEST_CASE("Radio station resolver rejects invalid raws before profile lookup") {
    const auto& resolver = saors::defaultRadioStationResolver();
    for (const auto rawValue : {-1, 256}) {
        const auto result = resolver.resolve(saors::ExecutableProfileId::unsupported, rawValue);
        CHECK(result.status == saors::RadioStationResolutionStatus::invalidRaw);
        CHECK(result.executableProfile == saors::ExecutableProfileId::unsupported);
        CHECK(result.rawValue == rawValue);
        checkAbsentIdentityAndEvidence(result);
    }
}

TEST_CASE("Radio station resolver has no profile fallback") {
    const auto& resolver = saors::defaultRadioStationResolver();
    for (const auto profile : {saors::ExecutableProfileId::gta3_10_us_candidate,
                               saors::ExecutableProfileId::unsupported}) {
        const auto result = resolver.resolve(profile, 0);
        CHECK(result.status == saors::RadioStationResolutionStatus::unsupportedProfile);
        CHECK(result.executableProfile == profile);
        CHECK(result.rawValue == 0);
        checkAbsentIdentityAndEvidence(result);
    }
}

TEST_CASE("Radio station resolver uses only its supplied registry and is deterministic") {
    const saors::RadioStationMapRegistry emptyRegistry;
    const saors::RadioStationResolver emptyResolver(emptyRegistry);
    const auto emptyResult = emptyResolver.resolve(saors::ExecutableProfileId::gta3_classic_local_candidate,
                                                   0);
    CHECK(emptyResult.status == saors::RadioStationResolutionStatus::unsupportedProfile);
    checkAbsentIdentityAndEvidence(emptyResult);

    const saors::RadioStationMapRegistry customRegistry({
        {saors::ExecutableProfileId::gta3_10_us_candidate,
         {{42, saors::RadioStationIdentity::gameFm,
           saors::RadioStationEvidenceLevel::observedOnce, 1U, 1U}},
         "controlled-test-map"},
    });
    const saors::RadioStationResolver customResolver(customRegistry);
    const auto noFallback = customResolver.resolve(
        saors::ExecutableProfileId::gta3_classic_local_candidate, 42);
    CHECK(noFallback.status == saors::RadioStationResolutionStatus::unsupportedProfile);
    checkAbsentIdentityAndEvidence(noFallback);

    const auto resolved = customResolver.resolve(saors::ExecutableProfileId::gta3_10_us_candidate, 42);
    CHECK(resolved.status == saors::RadioStationResolutionStatus::resolved);
    REQUIRE(resolved.identity);
    REQUIRE(resolved.evidence);
    CHECK(*resolved.identity == saors::RadioStationIdentity::gameFm);
    CHECK(*resolved.evidence == saors::RadioStationEvidenceLevel::observedOnce);

    const auto first = customResolver.resolve(saors::ExecutableProfileId::gta3_10_us_candidate, 42);
    const auto second = customResolver.resolve(saors::ExecutableProfileId::gta3_10_us_candidate, 42);
    CHECK(second.status == first.status);
    CHECK(second.executableProfile == first.executableProfile);
    CHECK(second.rawValue == first.rawValue);
    CHECK(second.identity == first.identity);
    CHECK(second.evidence == first.evidence);
    CHECK(customRegistry.profiles().size() == 1U);
}
