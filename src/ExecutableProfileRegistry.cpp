#include "saors_gta3/ExecutableProfileRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace saors {
namespace {

bool isLowercaseSha256(const std::string& value) {
    return value.size() == 64U &&
           std::all_of(value.begin(), value.end(), [](const unsigned char character) {
               return std::isdigit(character) != 0 ||
                      (character >= static_cast<unsigned char>('a') &&
                       character <= static_cast<unsigned char>('f'));
           });
}

bool isUsableProfile(const ExecutableProfile& profile) {
    return profile.id != ExecutableProfileId::unsupported && !profile.name.empty() &&
           isLowercaseSha256(profile.fileSha256) &&
           isLowercaseSha256(profile.textSectionSha256) && profile.machine == 0x014CU &&
           profile.optionalHeaderMagic == 0x010BU && profile.sizeOfImage != 0U &&
           profile.fileSize != 0U && !profile.evidenceOrigin.empty() &&
           !profile.verificationDate.empty();
}

bool metadataMatches(const ExecutableProfile& profile,
                     const ExecutableFingerprint& fingerprint) noexcept {
    return profile.machine == fingerprint.machine &&
           profile.optionalHeaderMagic == fingerprint.optionalHeaderMagic &&
           profile.timeDateStamp == fingerprint.timeDateStamp &&
           profile.entryPointRva == fingerprint.entryPointRva &&
           profile.sizeOfImage == fingerprint.sizeOfImage &&
           profile.checksum == fingerprint.checksum &&
           profile.fileSize == fingerprint.fileSize;
}

int matchScore(const ExecutableProfileMatch& match) noexcept {
    return (match.metadataMatch ? 4 : 0) + (match.fileFingerprintMatch ? 2 : 0) +
           (match.textFingerprintMatch ? 1 : 0);
}

} // namespace

const char*
executableVerificationStatusName(const ExecutableVerificationStatus status) noexcept {
    switch (status) {
    case ExecutableVerificationStatus::candidate:
        return "candidate";
    case ExecutableVerificationStatus::locallyReproduced:
        return "locally_reproduced";
    case ExecutableVerificationStatus::independentlyReproduced:
        return "independently_reproduced";
    case ExecutableVerificationStatus::verified:
        return "verified";
    }
    return "candidate";
}

const char*
executableFingerprintMatchKindName(const ExecutableFingerprintMatchKind kind) noexcept {
    switch (kind) {
    case ExecutableFingerprintMatchKind::none:
        return "no exact profile match";
    case ExecutableFingerprintMatchKind::candidateStructural:
        return "candidate structural match";
    case ExecutableFingerprintMatchKind::exact:
        return "exact fingerprint match";
    }
    return "no exact profile match";
}

ExecutableProfileRegistry::ExecutableProfileRegistry(
    std::vector<ExecutableProfile> profiles)
    : profiles_(std::move(profiles)) {}

ExecutableProfileMatch
ExecutableProfileRegistry::match(const ExecutableFingerprint& fingerprint) const {
    ExecutableProfileMatch best;
    int bestScore = 0;

    for (const auto& profile : profiles_) {
        if (!isUsableProfile(profile)) {
            continue;
        }

        ExecutableProfileMatch current;
        current.id = profile.id;
        current.profileName = profile.name;
        current.verificationStatus = profile.verificationStatus;
        current.fileFingerprintMatch = profile.fileSha256 == fingerprint.fileSha256;
        current.textFingerprintMatch =
            profile.textSectionSha256 == fingerprint.textSectionSha256;
        current.metadataMatch = metadataMatches(profile, fingerprint);

        if (current.fileFingerprintMatch && current.textFingerprintMatch &&
            current.metadataMatch) {
            current.kind = ExecutableFingerprintMatchKind::exact;
            return current;
        }
        if (current.metadataMatch) {
            current.kind = ExecutableFingerprintMatchKind::candidateStructural;
        }

        const auto score = matchScore(current);
        if (score > bestScore) {
            best = std::move(current);
            bestScore = score;
        }
    }

    if (bestScore == 0) {
        return {};
    }
    if (best.kind != ExecutableFingerprintMatchKind::candidateStructural) {
        best.id = ExecutableProfileId::unsupported;
        best.profileName = "none";
        best.verificationStatus.reset();
    }
    return best;
}

const std::vector<ExecutableProfile>& ExecutableProfileRegistry::profiles() const noexcept {
    return profiles_;
}

const ExecutableProfileRegistry& defaultExecutableProfileRegistry() {
    static const ExecutableProfileRegistry registry([] {
        ExecutableProfile candidate;
        candidate.id = ExecutableProfileId::gta3_classic_local_candidate;
        candidate.name = "GTA III Classic local candidate";
        candidate.fileSha256 = "ebb8cd22b88bd84b9a223aee02e67e3dc0b4acbc17d7155951e7cc02f524a343";
        candidate.textSectionSha256 =
            "695fe240ba96fc010b3363e64319d7327ca7f171ffa6eba50454ee37d6bbe79b";
        candidate.machine = 0x014CU;
        candidate.optionalHeaderMagic = 0x010BU;
        candidate.timeDateStamp = 1020186132U;
        candidate.entryPointRva = 1842800U;
        candidate.sizeOfImage = 5775360U;
        candidate.checksum = 0U;
        candidate.fileSize = 2383872U;
        candidate.verificationStatus = ExecutableVerificationStatus::locallyReproduced;
        candidate.evidenceOrigin =
            "Steam-managed GTA III Classic installation; two identical local probe runs";
        candidate.verificationDate = "2026-07-27";
        return ExecutableProfileRegistry({std::move(candidate)});
    }());
    return registry;
}

} // namespace saors
