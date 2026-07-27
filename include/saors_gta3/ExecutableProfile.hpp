#pragma once

#include <cstdint>
#include <string>

namespace saors {

enum class ExecutableProfileId {
    unsupported,
    gta3_10_us_candidate,
};

enum class ExecutableVerificationStatus {
    candidate,
    locallyReproduced,
    independentlyReproduced,
    verified,
};

struct ExecutableProfile {
    ExecutableProfileId id{ExecutableProfileId::unsupported};
    std::string name;
    std::string fileSha256;
    std::string textSectionSha256;

    std::uint16_t machine{0};
    std::uint16_t optionalHeaderMagic{0};
    std::uint32_t timeDateStamp{0};
    std::uint32_t entryPointRva{0};
    std::uint32_t sizeOfImage{0};
    std::uint32_t checksum{0};
    std::uint64_t fileSize{0};

    ExecutableVerificationStatus verificationStatus{
        ExecutableVerificationStatus::candidate};
    std::string evidenceOrigin;
    std::string verificationDate;
};

[[nodiscard]] const char*
executableVerificationStatusName(ExecutableVerificationStatus status) noexcept;

} // namespace saors
