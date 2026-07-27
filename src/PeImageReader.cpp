#include "saors_gta3/PeImageReader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace saors {
namespace {

constexpr std::uint16_t imageFileMachineI386 = 0x014CU;
constexpr std::uint16_t imageOptionalHeader32Magic = 0x010BU;
constexpr std::uint64_t maximumPe32FileSize =
    static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max());
constexpr std::uint32_t maximumPeHeaderOffset = 1024U * 1024U;
constexpr std::uint16_t maximumSections = 96U;
constexpr std::size_t dosHeaderSize = 64U;
constexpr std::size_t coffHeaderSize = 20U;
constexpr std::size_t sectionHeaderSize = 40U;
constexpr std::size_t minimumOptionalHeaderSize = 68U;

template <typename T>
bool checkedAdd(const T left, const T right, T& result) noexcept {
    static_assert(std::numeric_limits<T>::is_integer, "checkedAdd requires an integer type");
    if (right > std::numeric_limits<T>::max() - left) {
        return false;
    }
    result = static_cast<T>(left + right);
    return true;
}

std::uint16_t readU16(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U);
}

std::uint32_t readU32(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

bool readExact(std::ifstream& file, const std::uint64_t offset, std::uint8_t* destination,
               const std::size_t size) {
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()) ||
        size > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        return false;
    }
    file.clear();
    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!file.good()) {
        return false;
    }
    file.read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(size));
    return file.good() || (file.eof() && file.gcount() == static_cast<std::streamsize>(size));
}

bool isPrintableSectionNameByte(const std::uint8_t value) noexcept {
    return value >= 0x20U && value <= 0x7EU;
}

Result<std::string> parseSectionName(const std::uint8_t* bytes) {
    std::size_t length = 0;
    while (length < 8U && bytes[length] != 0U) {
        if (!isPrintableSectionNameByte(bytes[length])) {
            return Result<std::string>::fail("PE section name contains unsafe characters");
        }
        ++length;
    }
    if (length == 0U) {
        return Result<std::string>::fail("PE section name is empty");
    }
    for (std::size_t index = length; index < 8U; ++index) {
        if (bytes[index] != 0U) {
            return Result<std::string>::fail("PE section name has inconsistent padding");
        }
    }
    return Result<std::string>::ok(
        std::string(reinterpret_cast<const char*>(bytes), length));
}

struct Region {
    std::uint64_t begin{0};
    std::uint64_t end{0};
};

bool regionsOverlap(const Region& left, const Region& right) noexcept {
    return left.begin < right.end && right.begin < left.end;
}

Result<PeImageMetadata> fail(const char* message) {
    return Result<PeImageMetadata>::fail(message);
}

} // namespace

Result<PeImageMetadata> PeImageReader::read(const std::filesystem::path& path) const {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return fail("unable to open executable for reading");
    }

    file.seekg(0, std::ios::end);
    const auto endPosition = file.tellg();
    if (endPosition < 0) {
        return fail("unable to determine executable size");
    }
    const auto fileSize = static_cast<std::uint64_t>(endPosition);
    if (fileSize < dosHeaderSize) {
        return fail("file is too small to contain a DOS header");
    }
    if (fileSize > maximumPe32FileSize) {
        return fail("file is too large for defensive PE32 inspection");
    }

    std::array<std::uint8_t, dosHeaderSize> dos{};
    if (!readExact(file, 0, dos.data(), dos.size())) {
        return fail("DOS header is truncated");
    }
    if (dos[0] != static_cast<std::uint8_t>('M') ||
        dos[1] != static_cast<std::uint8_t>('Z')) {
        return fail("invalid DOS signature");
    }

    const auto peOffset = readU32(dos.data() + 0x3CU);
    if (peOffset < dosHeaderSize || peOffset > maximumPeHeaderOffset) {
        return fail("PE header offset is outside the permitted range");
    }
    std::uint64_t coffEnd = 0;
    if (!checkedAdd(static_cast<std::uint64_t>(peOffset),
                    static_cast<std::uint64_t>(4U + coffHeaderSize), coffEnd) ||
        coffEnd > fileSize) {
        return fail("PE header is outside the file");
    }

    std::array<std::uint8_t, 4U + coffHeaderSize> peAndCoff{};
    if (!readExact(file, peOffset, peAndCoff.data(), peAndCoff.size())) {
        return fail("PE and COFF headers are truncated");
    }
    if (peAndCoff[0] != static_cast<std::uint8_t>('P') ||
        peAndCoff[1] != static_cast<std::uint8_t>('E') || peAndCoff[2] != 0U ||
        peAndCoff[3] != 0U) {
        return fail("invalid PE signature");
    }

    const auto* coff = peAndCoff.data() + 4U;
    const auto machine = readU16(coff);
    if (machine != imageFileMachineI386) {
        return fail("PE machine is not Intel 386");
    }
    const auto sectionCount = readU16(coff + 2U);
    if (sectionCount == 0U || sectionCount > maximumSections) {
        return fail("PE section count is outside the permitted range");
    }
    const auto timeDateStamp = readU32(coff + 4U);
    const auto optionalHeaderSize = readU16(coff + 16U);
    if (optionalHeaderSize < minimumOptionalHeaderSize || optionalHeaderSize > 4096U) {
        return fail("PE32 optional header size is invalid");
    }

    const auto optionalOffset = coffEnd;
    std::uint64_t sectionTableOffset = 0;
    if (!checkedAdd(optionalOffset, static_cast<std::uint64_t>(optionalHeaderSize),
                    sectionTableOffset) ||
        sectionTableOffset > fileSize) {
        return fail("PE32 optional header is truncated");
    }

    std::vector<std::uint8_t> optional(optionalHeaderSize);
    if (!readExact(file, optionalOffset, optional.data(), optional.size())) {
        return fail("PE32 optional header is truncated");
    }
    const auto optionalMagic = readU16(optional.data());
    if (optionalMagic != imageOptionalHeader32Magic) {
        return fail(optionalMagic == 0x020BU ? "PE32+ executables are unsupported"
                                            : "invalid PE32 optional header magic");
    }

    const auto entryPointRva = readU32(optional.data() + 16U);
    const auto sectionAlignment = readU32(optional.data() + 32U);
    const auto fileAlignment = readU32(optional.data() + 36U);
    const auto sizeOfImage = readU32(optional.data() + 56U);
    const auto sizeOfHeaders = readU32(optional.data() + 60U);
    const auto checksum = readU32(optional.data() + 64U);
    if (sectionAlignment == 0U || fileAlignment == 0U ||
        fileAlignment > sectionAlignment || sizeOfImage == 0U ||
        entryPointRva >= sizeOfImage) {
        return fail("PE32 alignment or image metadata is inconsistent");
    }

    const auto sectionTableBytes =
        static_cast<std::uint64_t>(sectionCount) * sectionHeaderSize;
    std::uint64_t sectionTableEnd = 0;
    if (!checkedAdd(sectionTableOffset, sectionTableBytes, sectionTableEnd) ||
        sectionTableEnd > fileSize) {
        return fail("PE section table is truncated");
    }
    if (sizeOfHeaders < sectionTableEnd || sizeOfHeaders > fileSize) {
        return fail("PE SizeOfHeaders is inconsistent with the file");
    }

    std::vector<std::uint8_t> sectionTable(static_cast<std::size_t>(sectionTableBytes));
    if (!readExact(file, sectionTableOffset, sectionTable.data(), sectionTable.size())) {
        return fail("PE section table is truncated");
    }

    PeImageMetadata image;
    image.machine = machine;
    image.optionalHeaderMagic = optionalMagic;
    image.timeDateStamp = timeDateStamp;
    image.entryPointRva = entryPointRva;
    image.sizeOfImage = sizeOfImage;
    image.checksum = checksum;
    image.fileSize = fileSize;
    image.sections.reserve(sectionCount);

    std::vector<Region> rawRegions;
    std::vector<Region> virtualRegions;
    bool foundText = false;

    for (std::uint16_t index = 0; index < sectionCount; ++index) {
        const auto* section = sectionTable.data() + (static_cast<std::size_t>(index) *
                                                     sectionHeaderSize);
        const auto name = parseSectionName(section);
        if (!name) {
            return fail(name.error.c_str());
        }

        PeSectionInfo info;
        info.name = name.value;
        info.virtualSize = readU32(section + 8U);
        info.virtualAddress = readU32(section + 12U);
        info.rawSize = readU32(section + 16U);
        info.rawOffset = readU32(section + 20U);

        if (info.virtualSize > 0U) {
            std::uint32_t virtualEnd = 0;
            if (!checkedAdd(info.virtualAddress, info.virtualSize, virtualEnd) ||
                info.virtualAddress >= sizeOfImage || virtualEnd > sizeOfImage) {
                return fail("PE section virtual range is outside the image");
            }
            const Region virtualRegion{info.virtualAddress, virtualEnd};
            if (std::any_of(virtualRegions.begin(), virtualRegions.end(),
                            [&](const Region& existing) {
                                return regionsOverlap(existing, virtualRegion);
                            })) {
                return fail("PE section virtual ranges overlap");
            }
            virtualRegions.push_back(virtualRegion);
        }

        if (info.rawSize > 0U) {
            std::uint32_t rawEnd = 0;
            if (!checkedAdd(info.rawOffset, info.rawSize, rawEnd) ||
                info.rawOffset < sizeOfHeaders || rawEnd > fileSize) {
                return fail("PE section raw range is outside the file");
            }
            const Region rawRegion{info.rawOffset, rawEnd};
            if (std::any_of(rawRegions.begin(), rawRegions.end(), [&](const Region& existing) {
                    return regionsOverlap(existing, rawRegion);
                })) {
                return fail("PE section raw ranges overlap");
            }
            rawRegions.push_back(rawRegion);
        } else if (info.rawOffset > fileSize) {
            return fail("empty PE section has an invalid raw offset");
        }

        if (info.name == ".text") {
            if (foundText) {
                return fail("PE contains more than one .text section");
            }
            if (info.rawSize == 0U) {
                return fail("PE .text section has no file data");
            }
            foundText = true;
        }
        image.sections.push_back(std::move(info));
    }

    if (!foundText) {
        return fail("PE .text section is missing");
    }
    return Result<PeImageMetadata>::ok(std::move(image));
}

} // namespace saors
