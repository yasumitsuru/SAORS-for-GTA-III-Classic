#include "saors_gta3/GameObserver.hpp"

#include "saors_gta3/Logger.hpp"
#include "saors_gta3/MemoryPatch.hpp"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace saors {
namespace {

class WindowsProcessMemory final : public MemoryAccessor {
  public:
    bool read(const std::uintptr_t address, std::byte* output,
              const std::size_t size) noexcept override {
        if (address == 0 || output == nullptr || size == 0) {
            return false;
        }
        SIZE_T bytesRead = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
                                 output, size, &bytesRead) != FALSE &&
               bytesRead == size;
    }

    bool write(const std::uintptr_t address, const std::byte* input,
               const std::size_t size) noexcept override {
        if (address == 0 || input == nullptr || size == 0) {
            return false;
        }

        DWORD originalProtection = 0;
        auto* destination = reinterpret_cast<void*>(address);
        if (VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &originalProtection) ==
            FALSE) {
            return false;
        }

        SIZE_T bytesWritten = 0;
        const bool wrote = WriteProcessMemory(GetCurrentProcess(), destination, input, size,
                                              &bytesWritten) != FALSE &&
                           bytesWritten == size;
        const bool flushed =
            wrote && FlushInstructionCache(GetCurrentProcess(), destination, size) != FALSE;

        DWORD ignoredProtection = 0;
        const bool restored =
            VirtualProtect(destination, size, originalProtection, &ignoredProtection) != FALSE;
        return wrote && flushed && restored;
    }
};

#if defined(_MSC_VER) && defined(_M_IX86)
bool callFindPlayerVehicleProtected(const std::uintptr_t address, std::uintptr_t& result) noexcept {
    using Function = void*(__cdecl*)();
    __try {
        result = reinterpret_cast<std::uintptr_t>(reinterpret_cast<Function>(address)());
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = 0;
        return false;
    }
}

bool callGetRadioInCarProtected(const std::uintptr_t address, const std::uintptr_t dmaAudio,
                                int& result) noexcept {
    using Function = unsigned char(__thiscall*)(void*);
    __try {
        result = static_cast<int>(
            reinterpret_cast<Function>(address)(reinterpret_cast<void*>(dmaAudio)));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = 0;
        return false;
    }
}
#endif

class PluginSdkRuntimeAccess final : public GameRuntimeAccess {
  public:
    explicit PluginSdkRuntimeAccess(std::shared_ptr<WindowsProcessMemory> memory)
        : memory_(std::move(memory)) {}

    std::optional<bool> readBoolean(const std::uintptr_t address) override {
        std::byte raw{};
        if (!memory_ || !memory_->read(address, &raw, 1U)) {
            return std::nullopt;
        }
        const auto value = std::to_integer<unsigned int>(raw);
        if (value > 1U) {
            return std::nullopt;
        }
        return value == 1U;
    }

    std::optional<int> readInteger(const std::uintptr_t address) override {
        int value = 0;
        if (!memory_ ||
            !memory_->read(address, reinterpret_cast<std::byte*>(&value), sizeof(value))) {
            return std::nullopt;
        }
        return value;
    }

    std::optional<std::uintptr_t> callFindPlayerVehicle(const std::uintptr_t address) override {
#if defined(_MSC_VER) && defined(_M_IX86)
        std::uintptr_t result = 0;
        if (callFindPlayerVehicleProtected(address, result)) {
            return result;
        }
#else
        static_cast<void>(address);
#endif
        return std::nullopt;
    }

    std::optional<int> callGetRadioInCar(const std::uintptr_t address,
                                         const std::uintptr_t dmaAudio) override {
#if defined(_MSC_VER) && defined(_M_IX86)
        int result = 0;
        if (callGetRadioInCarProtected(address, dmaAudio, result)) {
            return result;
        }
#else
        static_cast<void>(address);
        static_cast<void>(dmaAudio);
#endif
        return std::nullopt;
    }

  private:
    std::shared_ptr<WindowsProcessMemory> memory_;
};

class WindowsGameObserver;
using GameProcessFunction = void(__cdecl*)();

std::atomic<WindowsGameObserver*> activeObserver{nullptr};
std::atomic<GameProcessFunction> originalGameProcess{nullptr};

void __cdecl gameProcessObserverCallback() noexcept;

std::optional<MemoryPatch> callbackPatch(const GameAddressProfile& profile) {
    if (!profile.gameProcessCallback) {
        return std::nullopt;
    }
    const auto callbackAddress = *profile.gameProcessCallback;
    const auto expected = [&]() -> const ExpectedCodeBytes* {
        for (const auto& candidate : profile.expectedCode) {
            if (candidate.symbol == GameSymbolId::gameProcessCallback &&
                candidate.address == callbackAddress && candidate.expected.size() == 5U &&
                candidate.mask.size() == 5U) {
                return &candidate;
            }
        }
        return nullptr;
    }();
    if (expected == nullptr || expected->expected.front() != std::byte{0xE8}) {
        return std::nullopt;
    }

    std::uint32_t originalDisplacement = 0;
    std::memcpy(&originalDisplacement, expected->expected.data() + 1U,
                sizeof(originalDisplacement));
    const auto nextInstruction = static_cast<std::uint32_t>(callbackAddress + 5U);
    const auto originalTarget = nextInstruction + originalDisplacement;
    originalGameProcess.store(
        reinterpret_cast<GameProcessFunction>(static_cast<std::uintptr_t>(originalTarget)),
        std::memory_order_release);

    const auto replacementTarget = reinterpret_cast<std::uintptr_t>(&gameProcessObserverCallback);
    if (replacementTarget > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    const auto replacementDisplacement =
        static_cast<std::uint32_t>(replacementTarget) - nextInstruction;

    MemoryPatch patch;
    patch.address = callbackAddress;
    patch.expected = expected->expected;
    patch.mask = expected->mask;
    patch.replacement.resize(5U);
    patch.replacement[0] = std::byte{0xE8};
    std::memcpy(patch.replacement.data() + 1U, &replacementDisplacement,
                sizeof(replacementDisplacement));
    return patch;
}

class WindowsGameObserver final : public GameObserver {
  public:
    WindowsGameObserver(GameAddressProfile profile, const bool logStateTransitions)
        : profile_(std::move(profile)), memory_(std::make_shared<WindowsProcessMemory>()),
          source_(profile_, std::make_shared<PluginSdkRuntimeAccess>(memory_)),
          logStateTransitions_(logStateTransitions) {}

    void setSnapshotListener(GameStateSnapshotListener* listener) noexcept override {
        listener_.store(listener, std::memory_order_release);
    }

    ObserverInstallResult start(const bool dryRun) noexcept override {
        ObserverInstallResult result;
        if (transaction_ && transaction_->installed()) {
            result.status = ObserverInstallStatus::installed;
            result.expectedBytesMatched = true;
            result.hookWritesPerformed = true;
            return result;
        }

        const auto expected = validateExpectedBytes(profile_, *memory_);
        result.expectedBytesMatched = expected.compatible();
        if (!expected.compatible()) {
            result.failedSymbol = expected.symbol;
            result.status = ObserverInstallStatus::dryRunIncompatible;
            return result;
        }

        try {
            const auto patch = callbackPatch(profile_);
            if (!patch) {
                result.status = ObserverInstallStatus::failed;
                return result;
            }
            transaction_ = std::make_unique<PatchTransaction>(std::vector<MemoryPatch>{*patch});
            const auto installed =
                transaction_->install(*memory_, profile_.imageBase, profile_.imageSize, dryRun);
            result.expectedBytesMatched = installed.expectedBytesMatched;
            result.hookWritesPerformed = installed.writesPerformed;
            result.rollbackSucceeded = installed.rollbackSucceeded;
            if (installed.status == PatchInstallStatus::dryRunCompatible) {
                result.status = ObserverInstallStatus::dryRunCompatible;
                return result;
            }
            if (installed.status != PatchInstallStatus::installed) {
                result.status = ObserverInstallStatus::failed;
                return result;
            }

            activeObserver.store(this, std::memory_order_release);
            result.status = ObserverInstallStatus::installed;
            return result;
        } catch (...) {
            result.status = ObserverInstallStatus::failed;
            if (transaction_) {
                result.rollbackSucceeded = transaction_->remove(*memory_);
            }
            return result;
        }
    }

    bool stop() noexcept override {
        listener_.store(nullptr, std::memory_order_release);
        auto* expected = this;
        static_cast<void>(
            activeObserver.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel));
        if (!transaction_) {
            return true;
        }
        return transaction_->remove(*memory_);
    }

    GameStateSnapshot snapshot() const noexcept override {
        return store_.snapshot();
    }

    void captureFrame() noexcept {
        auto snapshot = source_.capture();
        snapshot.sequence = ++sequence_;
        store_.publish(snapshot);

        auto* listener = listener_.load(std::memory_order_acquire);
        if (listener != nullptr) {
            try {
                listener->onGameStateSnapshot(snapshot);
            } catch (...) {
            }
        }

        if (!logStateTransitions_) {
            return;
        }
        const auto messages =
            transitionTracker_.transitions(snapshot, GameStateTransitionTracker::Clock::now());
        for (const auto& message : messages) {
            Logger::info(message);
        }
    }

  private:
    GameAddressProfile profile_;
    std::shared_ptr<WindowsProcessMemory> memory_;
    PluginSdkGameStateSource source_;
    std::unique_ptr<PatchTransaction> transaction_;
    GameStateStore store_;
    GameStateTransitionTracker transitionTracker_;
    std::atomic<GameStateSnapshotListener*> listener_{nullptr};
    std::uint64_t sequence_{0};
    bool logStateTransitions_{false};
};

void __cdecl gameProcessObserverCallback() noexcept {
    dispatchGameObserverFrame(
        [] {
            const auto original = originalGameProcess.load(std::memory_order_acquire);
            if (original != nullptr) {
                original();
            }
        },
        [] {
            auto* observer = activeObserver.load(std::memory_order_acquire);
            if (observer != nullptr) {
                observer->captureFrame();
            }
        });
}

} // namespace

std::unique_ptr<GameObserver> createExperimentalGameObserver(const GameAddressProfile& profile,
                                                             const bool logStateTransitions) {
    return std::make_unique<WindowsGameObserver>(profile, logStateTransitions);
}

} // namespace saors
