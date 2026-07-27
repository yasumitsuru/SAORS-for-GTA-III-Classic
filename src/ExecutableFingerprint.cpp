#include "saors_gta3/ExecutableFingerprint.hpp"

#include <algorithm>
#include <utility>

namespace saors {
namespace {

bool sameImageLayout(const PeImageMetadata& left, const PeImageMetadata& right) {
    if (left.machine != right.machine ||
        left.optionalHeaderMagic != right.optionalHeaderMagic ||
        left.timeDateStamp != right.timeDateStamp ||
        left.entryPointRva != right.entryPointRva ||
        left.sizeOfImage != right.sizeOfImage || left.checksum != right.checksum ||
        left.fileSize != right.fileSize || left.sections.size() != right.sections.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.sections.size(); ++index) {
        const auto& leftSection = left.sections[index];
        const auto& rightSection = right.sections[index];
        if (leftSection.name != rightSection.name ||
            leftSection.virtualAddress != rightSection.virtualAddress ||
            leftSection.virtualSize != rightSection.virtualSize ||
            leftSection.rawOffset != rightSection.rawOffset ||
            leftSection.rawSize != rightSection.rawSize) {
            return false;
        }
    }
    return true;
}

} // namespace

Result<ExecutableFingerprint> fingerprintExecutable(const std::filesystem::path& path,
                                                    const PeImageReader& reader,
                                                    const FileHasher& hasher) {
    const auto image = reader.read(path);
    if (!image) {
        return Result<ExecutableFingerprint>::fail(image.error);
    }

    const auto fileHash = hasher.sha256File(path);
    if (!fileHash) {
        return Result<ExecutableFingerprint>::fail(fileHash.error);
    }

    ExecutableFingerprint fingerprint;
    fingerprint.fileSha256 = fileHash.value;
    fingerprint.machine = image.value.machine;
    fingerprint.optionalHeaderMagic = image.value.optionalHeaderMagic;
    fingerprint.timeDateStamp = image.value.timeDateStamp;
    fingerprint.entryPointRva = image.value.entryPointRva;
    fingerprint.sizeOfImage = image.value.sizeOfImage;
    fingerprint.checksum = image.value.checksum;
    fingerprint.fileSize = image.value.fileSize;
    fingerprint.sections.reserve(image.value.sections.size());

    for (const auto& section : image.value.sections) {
        PeSectionFingerprint sectionFingerprint;
        sectionFingerprint.name = section.name;
        sectionFingerprint.virtualAddress = section.virtualAddress;
        sectionFingerprint.virtualSize = section.virtualSize;
        sectionFingerprint.rawOffset = section.rawOffset;
        sectionFingerprint.rawSize = section.rawSize;

        if (section.rawSize > 0U) {
            const auto sectionHash =
                hasher.sha256Range(path, section.rawOffset, section.rawSize);
            if (!sectionHash) {
                return Result<ExecutableFingerprint>::fail(sectionHash.error);
            }
            sectionFingerprint.sha256 = sectionHash.value;
        }
        if (section.name == ".text") {
            fingerprint.textSectionSha256 = sectionFingerprint.sha256;
        }
        fingerprint.sections.push_back(std::move(sectionFingerprint));
    }

    if (fingerprint.textSectionSha256.empty()) {
        return Result<ExecutableFingerprint>::fail(
            "unable to fingerprint the PE .text section");
    }

    const auto confirmedHash = hasher.sha256File(path);
    const auto confirmedImage = reader.read(path);
    if (!confirmedHash || confirmedHash.value != fileHash.value || !confirmedImage ||
        !sameImageLayout(image.value, confirmedImage.value)) {
        return Result<ExecutableFingerprint>::fail(
            "executable changed while its fingerprint was being calculated");
    }

    return Result<ExecutableFingerprint>::ok(std::move(fingerprint));
}

#if !defined(_WIN32)
std::unique_ptr<FileHasher> createPlatformFileHasher() {
    return {};
}
#endif

} // namespace saors
