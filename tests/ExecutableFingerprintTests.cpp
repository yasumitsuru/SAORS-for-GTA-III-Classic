#include "TestPeFixture.hpp"

#include "saors_gta3/ExecutableFingerprint.hpp"
#include "saors_gta3/ExecutableReport.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace {

class DeterministicHasher final : public saors::FileHasher {
  public:
    saors::Result<std::string>
    sha256File(const std::filesystem::path& path) const override {
        observedPaths.push_back(path);
        ++fileCalls;
        if (failFile) {
            return saors::Result<std::string>::fail("synthetic full-file failure");
        }
        return saors::Result<std::string>::ok(
            std::string(64U, changeOnSecondFileHash && fileCalls > 1U ? 'd' : 'a'));
    }

    saors::Result<std::string> sha256Range(const std::filesystem::path& path,
                                           const std::uint64_t offset,
                                           const std::uint64_t length) const override {
        observedPaths.push_back(path);
        ranges.emplace_back(offset, length);
        if (failRange) {
            return saors::Result<std::string>::fail("synthetic range failure");
        }
        return saors::Result<std::string>::ok(
            std::string(64U, offset == 0x200U ? 'b' : 'c'));
    }

    mutable std::vector<std::filesystem::path> observedPaths;
    mutable std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    bool failFile{false};
    bool failRange{false};
    bool changeOnSecondFileHash{false};
    mutable std::size_t fileCalls{0};
};

} // namespace

TEST_CASE("Executable fingerprint composes structural metadata and deterministic hashes") {
    const saors::tests::TemporaryBinary file(saors::tests::validPe32Fixture());
    const saors::PeImageReader reader;
    DeterministicHasher hasher;

    const auto result = saors::fingerprintExecutable(file.path(), reader, hasher);

    REQUIRE(result);
    CHECK(result.value.fileSha256 == std::string(64U, 'a'));
    CHECK(result.value.textSectionSha256 == std::string(64U, 'b'));
    REQUIRE(result.value.sections.size() == 2U);
    CHECK(result.value.sections[0].sha256 == std::string(64U, 'b'));
    CHECK(result.value.sections[1].sha256 == std::string(64U, 'c'));
    CHECK(result.value.machine == 0x014CU);
    CHECK(result.value.fileSize == 0x600U);
    REQUIRE(hasher.ranges.size() == 2U);
    CHECK(hasher.ranges[0] ==
          std::make_pair(static_cast<std::uint64_t>(0x200U),
                         static_cast<std::uint64_t>(0x200U)));
    CHECK(hasher.ranges[1] ==
          std::make_pair(static_cast<std::uint64_t>(0x400U),
                         static_cast<std::uint64_t>(0x200U)));
}

TEST_CASE("Executable fingerprint propagates file and section hashing failures") {
    const saors::tests::TemporaryBinary file(saors::tests::validPe32Fixture());
    const saors::PeImageReader reader;

    DeterministicHasher fileFailure;
    fileFailure.failFile = true;
    const auto full = saors::fingerprintExecutable(file.path(), reader, fileFailure);
    CHECK_FALSE(full);
    CHECK(full.error == "synthetic full-file failure");

    DeterministicHasher rangeFailure;
    rangeFailure.failRange = true;
    const auto range = saors::fingerprintExecutable(file.path(), reader, rangeFailure);
    CHECK_FALSE(range);
    CHECK(range.error == "synthetic range failure");
}

TEST_CASE("Executable fingerprint rejects a file that changes during collection") {
    const saors::tests::TemporaryBinary file(saors::tests::validPe32Fixture());
    const saors::PeImageReader reader;
    DeterministicHasher hasher;
    hasher.changeOnSecondFileHash = true;

    const auto result = saors::fingerprintExecutable(file.path(), reader, hasher);
    CHECK_FALSE(result);
    CHECK(result.error == "executable changed while its fingerprint was being calculated");
}

TEST_CASE("Redacted executable reports contain hashes but no personal path") {
    saors::ExecutableFingerprint fingerprint;
    fingerprint.fileSha256 = std::string(64U, 'a');
    fingerprint.textSectionSha256 = std::string(64U, 'b');
    fingerprint.machine = 0x014CU;
    fingerprint.optionalHeaderMagic = 0x010BU;
    fingerprint.sections.push_back(
        {".text", 0x1000U, 0x100U, 0x200U, 0x200U, std::string(64U, 'b')});

    const auto personal =
        std::filesystem::path("C:\\Private\\ExampleAccount\\Games\\GTA3\\gta3.exe");
    const saors::ExecutableReportContext context;
    const auto text =
        saors::formatExecutableTextReport(personal, fingerprint, context, true);
    const auto json =
        saors::formatExecutableJsonReport(personal, fingerprint, context, true);

    CHECK(text.find("[redacted]") != std::string::npos);
    CHECK(json.find("[redacted]") != std::string::npos);
    CHECK(text.find("ExampleAccount") == std::string::npos);
    CHECK(json.find("ExampleAccount") == std::string::npos);
    CHECK(json.find(std::string(64U, 'a')) != std::string::npos);
    CHECK(json.find("\"sections\"") != std::string::npos);
}

TEST_CASE("Executable JSON reports escape controls and preserve high-bit bytes") {
    saors::ExecutableFingerprint fingerprint;
    saors::ExecutableReportContext context;
    context.profileName.clear();

    std::string match = "ASCII \"quote\" \\ slash";
    match.push_back('\b');
    match.push_back('\f');
    match.push_back('\n');
    match.push_back('\r');
    match.push_back('\t');
    match.push_back(static_cast<char>(0x01U));
    match += "\xC3\xA9";
    match.push_back(static_cast<char>(0x80U));
    context.matchDescription = match;

    const auto json = saors::formatExecutableJsonReport("gta3.exe", fingerprint, context, false);

    std::string escaped = "ASCII \\\"quote\\\" \\\\ slash\\b\\f\\n\\r\\t\\u0001";
    escaped += "\xC3\xA9";
    escaped.push_back(static_cast<char>(0x80U));

    CHECK(json.find("\"profile\": \"\"") != std::string::npos);
    CHECK(json.find("\"match\": \"" + escaped + "\"") != std::string::npos);
}
