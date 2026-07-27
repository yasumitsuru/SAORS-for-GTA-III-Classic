#include "saors_gta3/ExecutableFingerprint.hpp"

#include <algorithm>
#include <utility>

namespace saors {

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

    const auto confirmed = reader.read(path);
    if (!confirmed || confirmed.value.fileSize != image.value.fileSize ||
        confirmed.value.timeDateStamp != image.value.timeDateStamp ||
        confirmed.value.checksum != image.value.checksum ||
        confirmed.value.sections.size() != image.value.sections.size()) {
        return Result<ExecutableFingerprint>::fail(
            "executable changed while its fingerprint was being calculated");
    }

    return Result<ExecutableFingerprint>::ok(std::move(fingerprint));
}

} // namespace saors
