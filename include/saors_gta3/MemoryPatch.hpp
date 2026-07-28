#pragma once

#include "saors_gta3/GameAddressProfile.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace saors {

class MemoryAccessor {
  public:
    virtual ~MemoryAccessor() = default;
    [[nodiscard]] virtual bool read(std::uintptr_t address, std::byte* output,
                                    std::size_t size) noexcept = 0;
    [[nodiscard]] virtual bool write(std::uintptr_t address, const std::byte* input,
                                     std::size_t size) noexcept = 0;
};

enum class ExpectedBytesStatus {
    compatible,
    invalidDefinition,
    outsideImage,
    readFailed,
    mismatch,
};

struct ExpectedBytesResult {
    ExpectedBytesStatus status{ExpectedBytesStatus::invalidDefinition};
    GameSymbolId symbol{GameSymbolId::gameProcessCallback};

    [[nodiscard]] bool compatible() const noexcept {
        return status == ExpectedBytesStatus::compatible;
    }
};

[[nodiscard]] bool addressRangeWithinImage(std::uintptr_t address, std::size_t size,
                                           std::uintptr_t imageBase,
                                           std::size_t imageSize) noexcept;
[[nodiscard]] ExpectedBytesResult validateExpectedBytes(const GameAddressProfile& profile,
                                                        MemoryAccessor& memory) noexcept;

struct MemoryPatch {
    std::uintptr_t address{0};
    std::vector<std::byte> expected;
    std::vector<std::byte> mask;
    std::vector<std::byte> replacement;
};

enum class PatchInstallStatus {
    installed,
    dryRunCompatible,
    alreadyInstalled,
    invalidDefinition,
    outsideImage,
    readFailed,
    expectedBytesMismatch,
    writeFailed,
    verificationFailed,
    rollbackFailed,
};

struct PatchInstallResult {
    PatchInstallStatus status{PatchInstallStatus::invalidDefinition};
    bool expectedBytesMatched{false};
    bool writesPerformed{false};
    bool rollbackSucceeded{true};

    [[nodiscard]] bool succeeded() const noexcept {
        return status == PatchInstallStatus::installed ||
               status == PatchInstallStatus::dryRunCompatible ||
               status == PatchInstallStatus::alreadyInstalled;
    }
};

class PatchTransaction {
  public:
    PatchTransaction() = default;
    explicit PatchTransaction(std::vector<MemoryPatch> patches);

    [[nodiscard]] PatchInstallResult install(MemoryAccessor& memory, std::uintptr_t imageBase,
                                             std::size_t imageSize, bool dryRun) noexcept;
    [[nodiscard]] bool remove(MemoryAccessor& memory) noexcept;
    [[nodiscard]] bool installed() const noexcept;

  private:
    struct AppliedPatch {
        std::uintptr_t address{0};
        std::vector<std::byte> original;
    };

    std::vector<MemoryPatch> patches_;
    std::vector<AppliedPatch> applied_;
    bool installed_{false};
};

} // namespace saors
