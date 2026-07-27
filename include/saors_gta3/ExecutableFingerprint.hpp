#pragma once

#include "saors_gta3/FileHasher.hpp"
#include "saors_gta3/PeImageReader.hpp"
#include "saors_gta3/Result.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace saors {

struct PeSectionFingerprint {
    std::string name;
    std::uint32_t virtualAddress{0};
    std::uint32_t virtualSize{0};
    std::uint32_t rawOffset{0};
    std::uint32_t rawSize{0};
    std::string sha256;
};

struct ExecutableFingerprint {
    std::string fileSha256;
    std::string textSectionSha256;

    std::uint16_t machine{0};
    std::uint16_t optionalHeaderMagic{0};
    std::uint32_t timeDateStamp{0};
    std::uint32_t entryPointRva{0};
    std::uint32_t sizeOfImage{0};
    std::uint32_t checksum{0};
    std::uint64_t fileSize{0};

    std::vector<PeSectionFingerprint> sections;
};

[[nodiscard]] Result<ExecutableFingerprint>
fingerprintExecutable(const std::filesystem::path& path, const PeImageReader& reader,
                      const FileHasher& hasher);

} // namespace saors
