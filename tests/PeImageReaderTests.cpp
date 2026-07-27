#include "TestPeFixture.hpp"

#include "saors_gta3/PeImageReader.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

namespace {

constexpr std::size_t peOffset = 0x80U;
constexpr std::size_t coffOffset = peOffset + 4U;
constexpr std::size_t optionalOffset = coffOffset + 20U;
constexpr std::size_t sectionTableOffset = optionalOffset + 0xE0U;

saors::Result<saors::PeImageMetadata> readFixture(const std::vector<std::uint8_t>& bytes,
                                                 const bool unicodeName = false) {
    const saors::tests::TemporaryBinary file(bytes, unicodeName);
    return saors::PeImageReader{}.read(file.path());
}

} // namespace

TEST_CASE("PE reader accepts a bounded synthetic PE32 Intel 386 image") {
    const auto result = readFixture(saors::tests::validPe32Fixture());

    REQUIRE(result);
    CHECK(result.value.machine == 0x014CU);
    CHECK(result.value.optionalHeaderMagic == 0x010BU);
    CHECK(result.value.timeDateStamp == 0x12345678U);
    CHECK(result.value.entryPointRva == 0x1000U);
    CHECK(result.value.sizeOfImage == 0x3000U);
    CHECK(result.value.checksum == 0xAABBCCDDU);
    CHECK(result.value.fileSize == 0x600U);
    REQUIRE(result.value.sections.size() == 2U);
    CHECK(result.value.sections[0].name == ".text");
    CHECK(result.value.sections[0].rawOffset == 0x200U);
    CHECK(result.value.sections[0].rawSize == 0x200U);
}

TEST_CASE("PE reader supports an explicitly supplied Unicode path") {
    const auto result = readFixture(saors::tests::validPe32Fixture(), true);
    REQUIRE(result);
    CHECK(result.value.sections.front().name == ".text");
}

TEST_CASE("PE reader reports a missing explicitly supplied file") {
    const auto result = saors::PeImageReader{}.read(
        std::filesystem::temp_directory_path() / "saors-missing-pe-fixture.exe");
    CHECK_FALSE(result);
    CHECK(result.error == "unable to open executable for reading");
}

TEST_CASE("PE reader rejects empty small and invalid DOS images") {
    CHECK_FALSE(readFixture({}));
    CHECK_FALSE(readFixture(std::vector<std::uint8_t>(63U, 0U)));

    auto bytes = saors::tests::validPe32Fixture();
    bytes[0] = 0U;
    const auto result = readFixture(bytes);
    CHECK_FALSE(result);
    CHECK(result.error == "invalid DOS signature");
}

TEST_CASE("PE reader rejects out-of-range e_lfanew and invalid PE signatures") {
    auto outside = saors::tests::validPe32Fixture();
    saors::tests::writeU32(outside, 0x3CU, 0x200000U);
    CHECK_FALSE(readFixture(outside));

    auto signature = saors::tests::validPe32Fixture();
    signature[peOffset] = static_cast<std::uint8_t>('N');
    const auto result = readFixture(signature);
    CHECK_FALSE(result);
    CHECK(result.error == "invalid PE signature");
}

TEST_CASE("PE reader rejects PE32 plus and non-i386 machines") {
    auto pe32Plus = saors::tests::validPe32Fixture();
    saors::tests::writeU16(pe32Plus, optionalOffset, 0x020BU);
    const auto plusResult = readFixture(pe32Plus);
    CHECK_FALSE(plusResult);
    CHECK(plusResult.error == "PE32+ executables are unsupported");

    auto x64 = saors::tests::validPe32Fixture();
    saors::tests::writeU16(x64, coffOffset, 0x8664U);
    const auto machineResult = readFixture(x64);
    CHECK_FALSE(machineResult);
    CHECK(machineResult.error == "PE machine is not Intel 386");
}

TEST_CASE("PE reader rejects truncated optional headers and section tables") {
    auto optional = saors::tests::validPe32Fixture();
    optional.resize(optionalOffset + 20U);
    const auto optionalResult = readFixture(optional);
    CHECK_FALSE(optionalResult);
    CHECK(optionalResult.error == "PE32 optional header is truncated");

    auto table = saors::tests::validPe32Fixture();
    table.resize(sectionTableOffset + 20U);
    const auto tableResult = readFixture(table);
    CHECK_FALSE(tableResult);
    CHECK(tableResult.error == "PE section table is truncated");
}

TEST_CASE("PE reader requires exactly one nonempty text section") {
    auto missing = saors::tests::validPe32Fixture();
    saors::tests::writeSectionName(missing, sectionTableOffset, ".code");
    const auto missingResult = readFixture(missing);
    CHECK_FALSE(missingResult);
    CHECK(missingResult.error == "PE .text section is missing");

    auto empty = saors::tests::validPe32Fixture();
    saors::tests::writeU32(empty, sectionTableOffset + 16U, 0U);
    const auto emptyResult = readFixture(empty);
    CHECK_FALSE(emptyResult);
    CHECK(emptyResult.error == "PE .text section has no file data");
}

TEST_CASE("PE reader rejects raw ranges outside the file and integer overflow") {
    auto outside = saors::tests::validPe32Fixture();
    saors::tests::writeU32(outside, sectionTableOffset + 20U, 0x580U);
    CHECK_FALSE(readFixture(outside));

    auto overflow = saors::tests::validPe32Fixture();
    saors::tests::writeU32(overflow, sectionTableOffset + 20U, 0xFFFFFFF0U);
    const auto overflowResult = readFixture(overflow);
    CHECK_FALSE(overflowResult);
    CHECK(overflowResult.error == "PE section raw range is outside the file");
}

TEST_CASE("PE reader rejects overlapping raw and virtual section regions") {
    auto raw = saors::tests::validPe32Fixture();
    saors::tests::writeU32(raw, sectionTableOffset + 40U + 20U, 0x300U);
    const auto rawResult = readFixture(raw);
    CHECK_FALSE(rawResult);
    CHECK(rawResult.error == "PE section raw ranges overlap");

    auto virtualBytes = saors::tests::validPe32Fixture();
    saors::tests::writeU32(virtualBytes, sectionTableOffset + 40U + 12U, 0x1100U);
    const auto virtualResult = readFixture(virtualBytes);
    CHECK_FALSE(virtualResult);
    CHECK(virtualResult.error == "PE section virtual ranges overlap");
}

TEST_CASE("PE reader rejects unsafe section names and inconsistent metadata") {
    auto unsafe = saors::tests::validPe32Fixture();
    unsafe[sectionTableOffset] = 0x01U;
    CHECK_FALSE(readFixture(unsafe));

    auto metadata = saors::tests::validPe32Fixture();
    saors::tests::writeU32(metadata, optionalOffset + 16U, 0x4000U);
    const auto metadataResult = readFixture(metadata);
    CHECK_FALSE(metadataResult);
    CHECK(metadataResult.error == "PE32 alignment or image metadata is inconsistent");
}
