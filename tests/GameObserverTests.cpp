#include "saors_gta3/GameAddressProfile.hpp"
#include "saors_gta3/GameObserver.hpp"
#include "saors_gta3/GameState.hpp"
#include "saors_gta3/MemoryPatch.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {

class SyntheticMemory final : public saors::MemoryAccessor {
  public:
    SyntheticMemory(const std::uintptr_t base, const std::size_t size)
        : base_(base), bytes_(size) {}

    bool read(const std::uintptr_t address, std::byte* output,
              const std::size_t size) noexcept override {
        if (!range(address, size) || output == nullptr || failReads_) {
            return false;
        }
        const auto offset = static_cast<std::size_t>(address - base_);
        std::copy_n(bytes_.data() + offset, size, output);
        return true;
    }

    bool write(const std::uintptr_t address, const std::byte* input,
               const std::size_t size) noexcept override {
        ++writeAttempts_;
        if (!range(address, size) || input == nullptr ||
            (failWriteAttempt_ != 0U && writeAttempts_ == failWriteAttempt_)) {
            return false;
        }
        const auto offset = static_cast<std::size_t>(address - base_);
        std::copy_n(input, size, bytes_.data() + offset);
        ++successfulWrites_;
        if (corruptWriteAttempt_ != 0U && writeAttempts_ == corruptWriteAttempt_) {
            bytes_[offset] ^= std::byte{0xFF};
        }
        if (failAfterWriteAttempt_ != 0U && writeAttempts_ == failAfterWriteAttempt_) {
            return false;
        }
        return true;
    }

    void set(const std::uintptr_t address, const std::vector<std::byte>& values) {
        REQUIRE(range(address, values.size()));
        const auto offset = static_cast<std::size_t>(address - base_);
        std::copy(values.begin(), values.end(),
                  bytes_.begin() + static_cast<std::ptrdiff_t>(offset));
    }

    [[nodiscard]] std::vector<std::byte> get(const std::uintptr_t address, const std::size_t size) {
        REQUIRE(range(address, size));
        const auto offset = static_cast<std::size_t>(address - base_);
        return {bytes_.begin() + static_cast<std::ptrdiff_t>(offset),
                bytes_.begin() + static_cast<std::ptrdiff_t>(offset + size)};
    }

    void failWriteAttempt(const std::size_t attempt) noexcept {
        failWriteAttempt_ = attempt;
    }

    void failAfterWriteAttempt(const std::size_t attempt) noexcept {
        failAfterWriteAttempt_ = attempt;
    }

    void corruptWriteAttempt(const std::size_t attempt) noexcept {
        corruptWriteAttempt_ = attempt;
    }

    void failReads(const bool fail) noexcept {
        failReads_ = fail;
    }

    [[nodiscard]] std::size_t writeAttempts() const noexcept {
        return writeAttempts_;
    }

    [[nodiscard]] std::size_t successfulWrites() const noexcept {
        return successfulWrites_;
    }

  private:
    [[nodiscard]] bool range(const std::uintptr_t address, const std::size_t size) const noexcept {
        return size > 0 && address >= base_ && address - base_ <= bytes_.size() &&
               size <= bytes_.size() - (address - base_);
    }

    std::uintptr_t base_{0};
    std::vector<std::byte> bytes_;
    std::size_t failWriteAttempt_{0};
    std::size_t failAfterWriteAttempt_{0};
    std::size_t corruptWriteAttempt_{0};
    std::size_t writeAttempts_{0};
    std::size_t successfulWrites_{0};
    bool failReads_{false};
};

std::vector<std::byte> bytes(std::initializer_list<unsigned int> values) {
    std::vector<std::byte> result;
    for (const auto value : values) {
        result.push_back(std::byte{static_cast<unsigned char>(value)});
    }
    return result;
}

saors::MemoryPatch patch(const std::uintptr_t address, const std::vector<std::byte>& expected,
                         const std::vector<std::byte>& replacement) {
    return {address, expected, std::vector<std::byte>(expected.size(), std::byte{0xFF}),
            replacement};
}

saors::GameAddressProfile syntheticAddressProfile() {
    saors::GameAddressProfile profile;
    profile.executableProfile = saors::ExecutableProfileId::gta3_classic_local_candidate;
    profile.imageBase = 0x1000U;
    profile.imageSize = 0x100U;
    profile.gameProcessCallback = 0x1010U;
    profile.findPlayerVehicle = 0x1020U;
    profile.frontEndMenuManager = 0x1040U;
    profile.getRadioInCar = 0x1030U;
    profile.musicVolume = 0x1050U;
    profile.dmaAudio = 0x1060U;
    profile.pauseMenuActiveOffset = 1U;
    profile.gameNotLoadedOffset = 2U;
    profile.minimumRadioStation = 0;
    profile.maximumRadioStation = 11;
    profile.minimumMusicVolume = 0;
    profile.maximumMusicVolume = 127;
    profile.expectedCode.push_back({saors::GameSymbolId::gameProcessCallback, 0x1010U,
                                    bytes({0xE8U, 0x11U, 0x22U, 0x33U, 0x44U}),
                                    bytes({0xFFU, 0xFFU, 0x00U, 0x00U, 0x00U})});
    return profile;
}

class FakeRuntime final : public saors::GameRuntimeAccess {
  public:
    std::optional<bool> gameNotLoaded{false};
    std::optional<bool> pauseMenu{false};
    std::optional<int> volume{127};
    std::optional<std::uintptr_t> vehicle{0};
    std::optional<int> station{3};
    std::uintptr_t menuBase{0x1040U};
    std::size_t pauseOffset{1U};
    std::size_t loadedOffset{2U};
    bool throwVehicle{false};

    std::optional<bool> readBoolean(const std::uintptr_t address) override {
        if (address == menuBase + loadedOffset) {
            return gameNotLoaded;
        }
        if (address == menuBase + pauseOffset) {
            return pauseMenu;
        }
        return std::nullopt;
    }

    std::optional<int> readInteger(const std::uintptr_t address) override {
        return address == 0x1050U ? volume : std::nullopt;
    }

    std::optional<std::uintptr_t> callFindPlayerVehicle(const std::uintptr_t address) override {
        if (throwVehicle) {
            throw std::runtime_error("synthetic failure");
        }
        return address == 0x1020U ? vehicle : std::nullopt;
    }

    std::optional<int> callGetRadioInCar(const std::uintptr_t address,
                                         const std::uintptr_t dmaAudio) override {
        return address == 0x1030U && dmaAudio == 0x1060U ? station : std::nullopt;
    }
};

} // namespace

TEST_CASE("Expected-byte validation supports masks and detects mismatches") {
    auto profile = syntheticAddressProfile();
    SyntheticMemory memory(profile.imageBase, profile.imageSize);
    memory.set(0x1010U, bytes({0xE8U, 0x11U, 0xAAU, 0xBBU, 0xCCU}));

    CHECK(saors::validateExpectedBytes(profile, memory).compatible());

    memory.set(0x1010U, bytes({0x90U, 0x11U, 0xAAU, 0xBBU, 0xCCU}));
    const auto mismatch = saors::validateExpectedBytes(profile, memory);
    CHECK(mismatch.status == saors::ExpectedBytesStatus::mismatch);
    CHECK(mismatch.symbol == saors::GameSymbolId::gameProcessCallback);

    memory.failReads(true);
    CHECK(saors::validateExpectedBytes(profile, memory).status ==
          saors::ExpectedBytesStatus::readFailed);
}

TEST_CASE("Address validation rejects outside-image ranges and overflow") {
    CHECK(saors::addressRangeWithinImage(0x1010U, 5U, 0x1000U, 0x100U));
    CHECK_FALSE(saors::addressRangeWithinImage(0x0FFFU, 1U, 0x1000U, 0x100U));
    CHECK_FALSE(saors::addressRangeWithinImage(0x10FFU, 2U, 0x1000U, 0x100U));
    CHECK_FALSE(saors::addressRangeWithinImage(std::numeric_limits<std::uintptr_t>::max() - 1U, 4U,
                                               0x1000U, 0x100U));

    auto profile = syntheticAddressProfile();
    profile.expectedCode.front().address = 0x10FFU;
    SyntheticMemory memory(profile.imageBase, profile.imageSize);
    CHECK(saors::validateExpectedBytes(profile, memory).status ==
          saors::ExpectedBytesStatus::outsideImage);
}

TEST_CASE("Patch transaction installs once and removes idempotently") {
    SyntheticMemory memory(0x1000U, 0x100U);
    const auto original = bytes({0x01U, 0x02U, 0x03U});
    const auto replacement = bytes({0x04U, 0x05U, 0x06U});
    memory.set(0x1010U, original);
    saors::PatchTransaction transaction({patch(0x1010U, original, replacement)});

    const auto installed = transaction.install(memory, 0x1000U, 0x100U, false);
    CHECK(installed.status == saors::PatchInstallStatus::installed);
    CHECK(installed.expectedBytesMatched);
    CHECK(installed.writesPerformed);
    CHECK(memory.get(0x1010U, 3U) == replacement);

    CHECK(transaction.install(memory, 0x1000U, 0x100U, false).status ==
          saors::PatchInstallStatus::alreadyInstalled);
    CHECK(transaction.remove(memory));
    CHECK(memory.get(0x1010U, 3U) == original);
    CHECK(transaction.remove(memory));
}

TEST_CASE("Patch transaction rolls back the first patch when the second write fails") {
    SyntheticMemory memory(0x1000U, 0x100U);
    const auto first = bytes({0x01U, 0x02U});
    const auto second = bytes({0x03U, 0x04U});
    memory.set(0x1010U, first);
    memory.set(0x1020U, second);
    memory.failWriteAttempt(2U);

    saors::PatchTransaction transaction({patch(0x1010U, first, bytes({0x11U, 0x12U})),
                                         patch(0x1020U, second, bytes({0x13U, 0x14U}))});
    const auto result = transaction.install(memory, 0x1000U, 0x100U, false);

    CHECK(result.status == saors::PatchInstallStatus::writeFailed);
    CHECK(result.writesPerformed);
    CHECK(result.rollbackSucceeded);
    CHECK_FALSE(transaction.installed());
    CHECK(memory.get(0x1010U, 2U) == first);
    CHECK(memory.get(0x1020U, 2U) == second);
}

TEST_CASE("Patch transaction performs no persistent change when the first write fails") {
    SyntheticMemory memory(0x1000U, 0x100U);
    const auto original = bytes({0x01U, 0x02U});
    memory.set(0x1010U, original);
    memory.failWriteAttempt(1U);
    saors::PatchTransaction transaction({patch(0x1010U, original, bytes({0x11U, 0x12U}))});

    const auto result = transaction.install(memory, 0x1000U, 0x100U, false);
    CHECK(result.status == saors::PatchInstallStatus::writeFailed);
    CHECK(result.expectedBytesMatched);
    CHECK_FALSE(result.writesPerformed);
    CHECK(result.rollbackSucceeded);
    CHECK_FALSE(transaction.installed());
    CHECK(memory.get(0x1010U, 2U) == original);
}

TEST_CASE("Patch transaction restores bytes when the memory API fails after writing") {
    SyntheticMemory memory(0x1000U, 0x100U);
    const auto original = bytes({0x01U, 0x02U});
    const auto replacement = bytes({0x11U, 0x12U});
    memory.set(0x1010U, original);
    memory.failAfterWriteAttempt(1U);

    saors::PatchTransaction transaction({patch(0x1010U, original, replacement)});
    const auto result = transaction.install(memory, 0x1000U, 0x100U, false);

    CHECK(result.status == saors::PatchInstallStatus::writeFailed);
    CHECK_FALSE(result.writesPerformed);
    CHECK(result.rollbackSucceeded);
    CHECK_FALSE(transaction.installed());
    CHECK(memory.get(0x1010U, 2U) == original);
}

TEST_CASE("Patch transaction rolls back a replacement that fails verification") {
    SyntheticMemory memory(0x1000U, 0x100U);
    const auto original = bytes({0x01U, 0x02U});
    memory.set(0x1010U, original);
    memory.corruptWriteAttempt(1U);
    saors::PatchTransaction transaction({patch(0x1010U, original, bytes({0x11U, 0x12U}))});

    const auto result = transaction.install(memory, 0x1000U, 0x100U, false);
    CHECK(result.status == saors::PatchInstallStatus::verificationFailed);
    CHECK(result.expectedBytesMatched);
    CHECK(result.writesPerformed);
    CHECK(result.rollbackSucceeded);
    CHECK_FALSE(transaction.installed());
    CHECK(memory.get(0x1010U, 2U) == original);
}

TEST_CASE("Dry-run validates every patch without writing") {
    SyntheticMemory memory(0x1000U, 0x100U);
    const auto original = bytes({0x01U, 0x02U});
    memory.set(0x1010U, original);
    saors::PatchTransaction transaction({patch(0x1010U, original, bytes({0x11U, 0x12U}))});

    const auto result = transaction.install(memory, 0x1000U, 0x100U, true);
    CHECK(result.status == saors::PatchInstallStatus::dryRunCompatible);
    CHECK(result.expectedBytesMatched);
    CHECK_FALSE(result.writesPerformed);
    CHECK(memory.writeAttempts() == 0U);
    CHECK(memory.get(0x1010U, 2U) == original);
}

TEST_CASE("Unavailable and fake sources preserve explicit availability") {
    saors::UnavailableGameStateSource unavailable;
    const auto missing = unavailable.capture();
    CHECK_FALSE(missing.observerAvailable);
    CHECK_FALSE(missing.pauseMenuActive);
    CHECK_FALSE(missing.playerInVehicle);
    CHECK_FALSE(missing.radioStationRaw);
    CHECK_FALSE(missing.radioVolume);

    saors::FakeGameStateSource fake;
    saors::GameStateSnapshot supplied;
    supplied.observerAvailable = true;
    supplied.gameReady = true;
    supplied.playerInVehicle = false;
    supplied.sequence = 9U;
    fake.setSnapshot(supplied);
    CHECK(fake.capture().sequence == 9U);
}

TEST_CASE("Plugin SDK source observes foot vehicle pause station and volume independently") {
    const auto runtime = std::make_shared<FakeRuntime>();
    saors::PluginSdkGameStateSource source(syntheticAddressProfile(), runtime);

    auto snapshot = source.capture();
    CHECK(snapshot.observerAvailable);
    CHECK(snapshot.gameReady);
    REQUIRE(snapshot.pauseMenuActive);
    CHECK_FALSE(*snapshot.pauseMenuActive);
    REQUIRE(snapshot.playerInVehicle);
    CHECK_FALSE(*snapshot.playerInVehicle);
    REQUIRE(snapshot.radioStationRaw);
    CHECK(*snapshot.radioStationRaw == 3);
    REQUIRE(snapshot.radioVolume);
    CHECK(*snapshot.radioVolume == Catch::Approx(1.0F));

    runtime->pauseMenu = true;
    runtime->vehicle = 0x20000U;
    runtime->station = 11;
    runtime->volume = 0;
    snapshot = source.capture();
    CHECK(*snapshot.pauseMenuActive);
    CHECK(*snapshot.playerInVehicle);
    CHECK(*snapshot.radioStationRaw == 11);
    CHECK(*snapshot.radioVolume == Catch::Approx(0.0F));
}

TEST_CASE("Game not loaded keeps gameplay fields unavailable") {
    const auto runtime = std::make_shared<FakeRuntime>();
    runtime->gameNotLoaded = true;
    runtime->pauseMenu = true;
    saors::PluginSdkGameStateSource source(syntheticAddressProfile(), runtime);

    const auto snapshot = source.capture();
    CHECK(snapshot.observerAvailable);
    CHECK_FALSE(snapshot.gameReady);
    REQUIRE(snapshot.pauseMenuActive);
    CHECK(*snapshot.pauseMenuActive);
    CHECK_FALSE(snapshot.playerInVehicle);
    CHECK_FALSE(snapshot.radioStationRaw);
    REQUIRE(snapshot.radioVolume);
    CHECK(*snapshot.radioVolume == Catch::Approx(1.0F));
}

TEST_CASE("Invalid state values and one failing read do not invalidate other fields") {
    const auto runtime = std::make_shared<FakeRuntime>();
    runtime->station = 99;
    runtime->volume = 128;
    runtime->throwVehicle = true;
    saors::PluginSdkGameStateSource source(syntheticAddressProfile(), runtime);

    const auto snapshot = source.capture();
    CHECK(snapshot.observerAvailable);
    CHECK(snapshot.gameReady);
    REQUIRE(snapshot.pauseMenuActive);
    CHECK_FALSE(*snapshot.pauseMenuActive);
    CHECK_FALSE(snapshot.playerInVehicle);
    CHECK_FALSE(snapshot.radioStationRaw);
    CHECK_FALSE(snapshot.radioVolume);
}

TEST_CASE("Game-state store supports concurrent publication and immutable snapshots") {
    saors::GameStateStore store;
    std::atomic<bool> finished{false};
    std::thread writer([&] {
        for (std::uint64_t sequence = 1; sequence <= 1000U; ++sequence) {
            saors::GameStateSnapshot snapshot;
            snapshot.observerAvailable = true;
            snapshot.sequence = sequence;
            store.publish(snapshot);
        }
        finished.store(true, std::memory_order_release);
    });

    std::uint64_t observed = 0;
    while (!finished.load(std::memory_order_acquire)) {
        const auto snapshot = store.snapshot();
        CHECK(snapshot.sequence >= observed);
        observed = snapshot.sequence;
    }
    writer.join();
    CHECK(store.snapshot().sequence == 1000U);
}

TEST_CASE("Transition tracker emits only changes and rate limits rapid updates") {
    saors::GameStateTransitionTracker tracker;
    const auto start = saors::GameStateTransitionTracker::Clock::time_point{};
    saors::GameStateSnapshot snapshot;
    snapshot.observerAvailable = true;
    snapshot.gameReady = true;
    snapshot.pauseMenuActive = false;
    snapshot.playerInVehicle = false;
    snapshot.radioStationRaw = 3;
    snapshot.radioVolume = 1.0F;

    CHECK(tracker.transitions(snapshot, start).size() == 5U);

    snapshot.playerInVehicle = true;
    CHECK(tracker.transitions(snapshot, start + std::chrono::milliseconds(100)).empty());
    const auto changed = tracker.transitions(snapshot, start + std::chrono::milliseconds(300));
    REQUIRE(changed.size() == 1U);
    CHECK(changed.front() == "Player vehicle state: in vehicle");
    CHECK(tracker.transitions(snapshot, start + std::chrono::milliseconds(600)).empty());
}

TEST_CASE("Address profiles require an exact supported executable verification level") {
    CHECK(saors::findGameAddressProfile(saors::ExecutableProfileId::gta3_classic_local_candidate,
                                        saors::ExecutableVerificationStatus::locallyReproduced) !=
          nullptr);
    CHECK(saors::findGameAddressProfile(saors::ExecutableProfileId::gta3_classic_local_candidate,
                                        saors::ExecutableVerificationStatus::candidate) == nullptr);
    CHECK(saors::findGameAddressProfile(saors::ExecutableProfileId::gta3_10_us_candidate,
                                        saors::ExecutableVerificationStatus::verified) == nullptr);
}

TEST_CASE("Observer remains unavailable in the default build") {
    saors::UnavailableGameObserver observer;
    const auto result = observer.start(true);
    CHECK(result.status == saors::ObserverInstallStatus::unavailable);
    CHECK_FALSE(result.expectedBytesMatched);
    CHECK_FALSE(result.hookWritesPerformed);
    CHECK(observer.stop());
    CHECK_FALSE(observer.snapshot().observerAvailable);
}
