#include "saors_gta3/RadioStationObservationRecorder.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {
saors::GameStateSnapshot snapshot(std::uint64_t sequence, int raw) {
    saors::GameStateSnapshot value;
    value.observerAvailable = true;
    value.gameReady = true;
    value.playerInVehicle = true;
    value.radioStationRaw = raw;
    value.sequence = sequence;
    return value;
}
}

TEST_CASE("Recorder ignores unavailable, on-foot, invalid, and repeated frames") {
    saors::RadioStationObservationRecorder recorder(saors::ExecutableProfileId::gta3_classic_local_candidate, 3U);
    auto unavailable = snapshot(1U, 3);
    unavailable.observerAvailable = false;
    CHECK_FALSE(recorder.observe(unavailable));
    auto onFoot = snapshot(2U, 3);
    onFoot.playerInVehicle = false;
    CHECK_FALSE(recorder.observe(onFoot));
    auto invalid = snapshot(3U, 256);
    CHECK_FALSE(recorder.observe(invalid));
    CHECK_FALSE(recorder.observe(snapshot(4U, 3)));
    CHECK_FALSE(recorder.observe(snapshot(5U, 3)));
    const auto observation = recorder.observe(snapshot(6U, 3));
    REQUIRE(observation);
    CHECK(observation->rawValue == 3);
    CHECK(observation->ordinal == 1U);
    CHECK(observation->stableFrames == 3U);
    CHECK_FALSE(recorder.observe(snapshot(7U, 3)));
}

TEST_CASE("Recorder emits changes after stability and rejects out-of-order sequences") {
    saors::RadioStationObservationRecorder recorder(saors::ExecutableProfileId::gta3_classic_local_candidate, 2U);
    CHECK_FALSE(recorder.observe(snapshot(10U, 0)));
    REQUIRE(recorder.observe(snapshot(11U, 0)));
    CHECK_FALSE(recorder.observe(snapshot(9U, 1)));
    CHECK_FALSE(recorder.observe(snapshot(12U, 1)));
    const auto changed = recorder.observe(snapshot(13U, 1));
    REQUIRE(changed);
    CHECK(changed->ordinal == 2U);
    CHECK(changed->rawValue == 1);
}

TEST_CASE("Recorder reset starts a new session and enforces observation limit") {
    saors::RadioStationObservationRecorder recorder(saors::ExecutableProfileId::gta3_classic_local_candidate, 1U, 1U);
    recorder.reset("session-a");
    REQUIRE(recorder.observe(snapshot(1U, 0)));
    CHECK_FALSE(recorder.observe(snapshot(2U, 1)));
    recorder.reset("session-b");
    const auto observation = recorder.observe(snapshot(1U, 1));
    REQUIRE(observation);
    CHECK(observation->sessionId == "session-b");
    CHECK(observation->ordinal == 1U);
}
