#pragma once

#include "saors_gta3/Result.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace saors {

struct PeSectionInfo {
    std::string name;
    std::uint32_t virtualAddress{0};
    std::uint32_t virtualSize{0};
    std::uint32_t rawOffset{0};
    std::uint32_t rawSize{0};
};

struct PeImageMetadata {
    std::uint16_t machine{0};
    std::uint16_t optionalHeaderMagic{0};
    std::uint32_t timeDateStamp{0};
    std::uint32_t entryPointRva{0};
    std::uint32_t sizeOfImage{0};
    std::uint32_t checksum{0};
    std::uint64_t fileSize{0};
    std::vector<PeSectionInfo> sections;
};

class PeImageReader {
  public:
    [[nodiscard]] Result<PeImageMetadata> read(const std::filesystem::path& path) const;
};

} // namespace saors
