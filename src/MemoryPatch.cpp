#include "saors_gta3/MemoryPatch.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace saors {
namespace {

bool validExpectedBytes(const ExpectedCodeBytes& expected) noexcept {
    return expected.address != 0 && !expected.expected.empty() &&
           expected.expected.size() == expected.mask.size();
}

bool matches(const std::vector<std::byte>& actual, const std::vector<std::byte>& expected,
             const std::vector<std::byte>& mask) noexcept {
    if (actual.size() != expected.size() || actual.size() != mask.size()) {
        return false;
    }
    for (std::size_t index = 0; index < actual.size(); ++index) {
        if ((actual[index] & mask[index]) != (expected[index] & mask[index])) {
            return false;
        }
    }
    return true;
}

ExpectedBytesStatus patchValidationStatus(const MemoryPatch& patch, MemoryAccessor& memory,
                                          const std::uintptr_t imageBase,
                                          const std::size_t imageSize,
                                          std::vector<std::byte>& original) noexcept {
    if (patch.address == 0 || patch.expected.empty() ||
        patch.expected.size() != patch.mask.size() ||
        patch.expected.size() != patch.replacement.size()) {
        return ExpectedBytesStatus::invalidDefinition;
    }
    if (!addressRangeWithinImage(patch.address, patch.expected.size(), imageBase, imageSize)) {
        return ExpectedBytesStatus::outsideImage;
    }
    original.resize(patch.expected.size());
    if (!memory.read(patch.address, original.data(), original.size())) {
        return ExpectedBytesStatus::readFailed;
    }
    return matches(original, patch.expected, patch.mask) ? ExpectedBytesStatus::compatible
                                                         : ExpectedBytesStatus::mismatch;
}

PatchInstallStatus toPatchStatus(const ExpectedBytesStatus status) noexcept {
    switch (status) {
    case ExpectedBytesStatus::compatible:
        return PatchInstallStatus::dryRunCompatible;
    case ExpectedBytesStatus::invalidDefinition:
        return PatchInstallStatus::invalidDefinition;
    case ExpectedBytesStatus::outsideImage:
        return PatchInstallStatus::outsideImage;
    case ExpectedBytesStatus::readFailed:
        return PatchInstallStatus::readFailed;
    case ExpectedBytesStatus::mismatch:
        return PatchInstallStatus::expectedBytesMismatch;
    }
    return PatchInstallStatus::invalidDefinition;
}

} // namespace

bool addressRangeWithinImage(const std::uintptr_t address, const std::size_t size,
                             const std::uintptr_t imageBase, const std::size_t imageSize) noexcept {
    if (size == 0 || imageSize == 0 || address < imageBase) {
        return false;
    }
    if (imageBase > std::numeric_limits<std::uintptr_t>::max() - imageSize) {
        return false;
    }
    const auto imageEnd = imageBase + imageSize;
    if (address >= imageEnd || address > std::numeric_limits<std::uintptr_t>::max() - size) {
        return false;
    }
    return address + size <= imageEnd;
}

ExpectedBytesResult validateExpectedBytes(const GameAddressProfile& profile,
                                          MemoryAccessor& memory) noexcept {
    ExpectedBytesResult result;
    for (const auto& expected : profile.expectedCode) {
        result.symbol = expected.symbol;
        if (!validExpectedBytes(expected)) {
            result.status = ExpectedBytesStatus::invalidDefinition;
            return result;
        }
        if (!addressRangeWithinImage(expected.address, expected.expected.size(), profile.imageBase,
                                     profile.imageSize)) {
            result.status = ExpectedBytesStatus::outsideImage;
            return result;
        }

        std::vector<std::byte> actual(expected.expected.size());
        if (!memory.read(expected.address, actual.data(), actual.size())) {
            result.status = ExpectedBytesStatus::readFailed;
            return result;
        }
        if (!matches(actual, expected.expected, expected.mask)) {
            result.status = ExpectedBytesStatus::mismatch;
            return result;
        }
    }
    result.status = ExpectedBytesStatus::compatible;
    return result;
}

PatchTransaction::PatchTransaction(std::vector<MemoryPatch> patches)
    : patches_(std::move(patches)) {}

PatchInstallResult PatchTransaction::install(MemoryAccessor& memory, const std::uintptr_t imageBase,
                                             const std::size_t imageSize,
                                             const bool dryRun) noexcept {
    PatchInstallResult result;
    if (installed_) {
        result.status = PatchInstallStatus::alreadyInstalled;
        result.expectedBytesMatched = true;
        return result;
    }

    try {
        std::vector<AppliedPatch> prepared;
        prepared.reserve(patches_.size());
        if (patches_.empty()) {
            result.status = PatchInstallStatus::invalidDefinition;
            return result;
        }

        for (const auto& patch : patches_) {
            AppliedPatch item;
            item.address = patch.address;
            const auto status =
                patchValidationStatus(patch, memory, imageBase, imageSize, item.original);
            if (status != ExpectedBytesStatus::compatible) {
                result.status = toPatchStatus(status);
                return result;
            }
            prepared.push_back(std::move(item));
        }
        result.expectedBytesMatched = true;
        if (dryRun) {
            result.status = PatchInstallStatus::dryRunCompatible;
            return result;
        }

        applied_.clear();
        bool installationFailed = false;
        for (std::size_t index = 0; index < patches_.size(); ++index) {
            const auto& patch = patches_[index];
            applied_.push_back(std::move(prepared[index]));
            if (!memory.write(patch.address, patch.replacement.data(), patch.replacement.size())) {
                result.status = PatchInstallStatus::writeFailed;
                installationFailed = true;
                break;
            }
            result.writesPerformed = true;

            std::vector<std::byte> verification(patch.replacement.size());
            if (!memory.read(patch.address, verification.data(), verification.size()) ||
                verification != patch.replacement) {
                result.status = PatchInstallStatus::verificationFailed;
                installationFailed = true;
                break;
            }
        }

        if (!installationFailed && applied_.size() == patches_.size()) {
            installed_ = true;
            result.status = PatchInstallStatus::installed;
            return result;
        }

        result.rollbackSucceeded = remove(memory);
        if (!result.rollbackSucceeded) {
            result.status = PatchInstallStatus::rollbackFailed;
        }
        return result;
    } catch (...) {
        result.status = PatchInstallStatus::writeFailed;
        result.rollbackSucceeded = remove(memory);
        if (!result.rollbackSucceeded) {
            result.status = PatchInstallStatus::rollbackFailed;
        }
        return result;
    }
}

bool PatchTransaction::remove(MemoryAccessor& memory) noexcept {
    bool restored = true;
    for (auto item = applied_.rbegin(); item != applied_.rend(); ++item) {
        if (!memory.write(item->address, item->original.data(), item->original.size())) {
            restored = false;
        }
    }
    if (restored) {
        applied_.clear();
        installed_ = false;
    }
    return restored;
}

bool PatchTransaction::installed() const noexcept {
    return installed_;
}

} // namespace saors
