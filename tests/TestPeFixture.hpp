#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace saors::tests {

inline void writeU16(std::vector<std::uint8_t>& bytes, const std::size_t offset,
                     const std::uint16_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value & 0xFFU);
    bytes.at(offset + 1U) = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

inline void writeU32(std::vector<std::uint8_t>& bytes, const std::size_t offset,
                     const std::uint32_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value & 0xFFU);
    bytes.at(offset + 1U) = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes.at(offset + 2U) = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    bytes.at(offset + 3U) = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

inline void writeSectionName(std::vector<std::uint8_t>& bytes, const std::size_t offset,
                             const std::string& name) {
    for (std::size_t index = 0; index < 8U; ++index) {
        bytes.at(offset + index) =
            index < name.size() ? static_cast<std::uint8_t>(name[index]) : 0U;
    }
}

inline std::vector<std::uint8_t> validPe32Fixture() {
    constexpr std::size_t peOffset = 0x80U;
    constexpr std::size_t coffOffset = peOffset + 4U;
    constexpr std::size_t optionalOffset = coffOffset + 20U;
    constexpr std::size_t sectionTableOffset = optionalOffset + 0xE0U;

    std::vector<std::uint8_t> bytes(0x600U, 0U);
    bytes[0] = static_cast<std::uint8_t>('M');
    bytes[1] = static_cast<std::uint8_t>('Z');
    writeU32(bytes, 0x3CU, static_cast<std::uint32_t>(peOffset));

    bytes[peOffset] = static_cast<std::uint8_t>('P');
    bytes[peOffset + 1U] = static_cast<std::uint8_t>('E');
    writeU16(bytes, coffOffset, 0x014CU);
    writeU16(bytes, coffOffset + 2U, 2U);
    writeU32(bytes, coffOffset + 4U, 0x12345678U);
    writeU16(bytes, coffOffset + 16U, 0x00E0U);
    writeU16(bytes, coffOffset + 18U, 0x010FU);

    writeU16(bytes, optionalOffset, 0x010BU);
    writeU32(bytes, optionalOffset + 16U, 0x1000U);
    writeU32(bytes, optionalOffset + 32U, 0x1000U);
    writeU32(bytes, optionalOffset + 36U, 0x0200U);
    writeU32(bytes, optionalOffset + 56U, 0x3000U);
    writeU32(bytes, optionalOffset + 60U, 0x0200U);
    writeU32(bytes, optionalOffset + 64U, 0xAABBCCDDU);

    writeSectionName(bytes, sectionTableOffset, ".text");
    writeU32(bytes, sectionTableOffset + 8U, 0x0180U);
    writeU32(bytes, sectionTableOffset + 12U, 0x1000U);
    writeU32(bytes, sectionTableOffset + 16U, 0x0200U);
    writeU32(bytes, sectionTableOffset + 20U, 0x0200U);

    const auto second = sectionTableOffset + 40U;
    writeSectionName(bytes, second, ".rdata");
    writeU32(bytes, second + 8U, 0x0100U);
    writeU32(bytes, second + 12U, 0x2000U);
    writeU32(bytes, second + 16U, 0x0200U);
    writeU32(bytes, second + 20U, 0x0400U);

    for (std::size_t index = 0x200U; index < 0x400U; ++index) {
        bytes[index] = static_cast<std::uint8_t>(index & 0xFFU);
    }
    for (std::size_t index = 0x400U; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>((index * 3U) & 0xFFU);
    }
    return bytes;
}

class TemporaryBinary {
  public:
    explicit TemporaryBinary(const std::vector<std::uint8_t>& bytes,
                             const bool unicodeName = false) {
        static std::atomic_uint64_t counter{0};
        const auto id = counter.fetch_add(1U, std::memory_order_relaxed);
        std::filesystem::path name;
#if defined(_WIN32)
        name = unicodeName
                   ? std::filesystem::path(std::wstring(L"saors-pe-\u00E7-") +
                                           std::to_wstring(id) + L".exe")
                   : std::filesystem::path(L"saors-pe-" + std::to_wstring(id) + L".exe");
#else
        name = unicodeName
                   ? std::filesystem::path(std::string("saors-pe-\xC3\xA7-") +
                                           std::to_string(id) + ".exe")
                   : std::filesystem::path("saors-pe-" + std::to_string(id) + ".exe");
#endif
        path_ = std::filesystem::temp_directory_path() / name;
        write(bytes);
    }

    ~TemporaryBinary() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    TemporaryBinary(const TemporaryBinary&) = delete;
    TemporaryBinary& operator=(const TemporaryBinary&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

    void write(const std::vector<std::uint8_t>& bytes) const {
        std::ofstream file(path_, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }

  private:
    std::filesystem::path path_;
};

} // namespace saors::tests
