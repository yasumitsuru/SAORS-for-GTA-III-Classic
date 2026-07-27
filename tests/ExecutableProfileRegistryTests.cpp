#include "saors_gta3/ExecutableProfileRegistry.hpp"
#include "saors_gta3/GameIntegration.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

namespace {

saors::ExecutableFingerprint fingerprint() {
    saors::ExecutableFingerprint result;
    result.fileSha256 = std::string(64U, 'a');
    result.textSectionSha256 = std::string(64U, 'b');
    result.machine = 0x014CU;
    result.optionalHeaderMagic = 0x010BU;
    result.timeDateStamp = 0x12345678U;
    result.entryPointRva = 0x1000U;
    result.sizeOfImage = 0x3000U;
    result.checksum = 0xAABBCCDDU;
    result.fileSize = 0x600U;
    return result;
}

saors::ExecutableProfile profile() {
    const auto source = fingerprint();
    saors::ExecutableProfile result;
    result.id = saors::ExecutableProfileId::gta3_classic_local_candidate;
    result.name = "GTA III Classic local candidate";
    result.fileSha256 = source.fileSha256;
    result.textSectionSha256 = source.textSectionSha256;
    result.machine = source.machine;
    result.optionalHeaderMagic = source.optionalHeaderMagic;
    result.timeDateStamp = source.timeDateStamp;
    result.entryPointRva = source.entryPointRva;
    result.sizeOfImage = source.sizeOfImage;
    result.checksum = source.checksum;
    result.fileSize = source.fileSize;
    result.verificationStatus = saors::ExecutableVerificationStatus::candidate;
    result.evidenceOrigin = "synthetic unit-test fixture";
    result.verificationDate = "2026-07-27";
    return result;
}

saors::ExecutableFingerprint localCandidateFingerprint() {
    saors::ExecutableFingerprint result;
    result.fileSha256 = "ebb8cd22b88bd84b9a223aee02e67e3dc0b4acbc17d7155951e7cc02f524a343";
    result.textSectionSha256 = "695fe240ba96fc010b3363e64319d7327ca7f171ffa6eba50454ee37d6bbe79b";
    result.machine = 0x014CU;
    result.optionalHeaderMagic = 0x010BU;
    result.timeDateStamp = 1020186132U;
    result.entryPointRva = 1842800U;
    result.sizeOfImage = 5775360U;
    result.checksum = 0U;
    result.fileSize = 2383872U;
    return result;
}

} // namespace

TEST_CASE("Profile registry accepts only an exact full text and metadata match") {
    const saors::ExecutableProfileRegistry registry({profile()});
    const auto match = registry.match(fingerprint());

    CHECK(match.exact());
    CHECK(match.id == saors::ExecutableProfileId::gta3_classic_local_candidate);
    CHECK(match.fileFingerprintMatch);
    CHECK(match.textFingerprintMatch);
    CHECK(match.metadataMatch);
    REQUIRE(match.verificationStatus);
    CHECK(*match.verificationStatus == saors::ExecutableVerificationStatus::candidate);
}

TEST_CASE("Default registry recognizes only the locally reproduced exact fingerprint") {
    REQUIRE(saors::defaultExecutableProfileRegistry().profiles().size() == 1U);
    const auto match = saors::defaultExecutableProfileRegistry().match(localCandidateFingerprint());

    CHECK(match.exact());
    CHECK(match.id == saors::ExecutableProfileId::gta3_classic_local_candidate);
    CHECK(match.profileName == "GTA III Classic local candidate");
    CHECK(match.fileFingerprintMatch);
    CHECK(match.textFingerprintMatch);
    CHECK(match.metadataMatch);
    REQUIRE(match.verificationStatus);
    CHECK(*match.verificationStatus == saors::ExecutableVerificationStatus::locallyReproduced);
}

TEST_CASE("Profile registry reports modified full and text fingerprints as partial only") {
    const saors::ExecutableProfileRegistry registry({profile()});

    auto fullChanged = fingerprint();
    fullChanged.fileSha256 = std::string(64U, 'c');
    const auto full = registry.match(fullChanged);
    CHECK_FALSE(full.exact());
    CHECK(full.kind == saors::ExecutableFingerprintMatchKind::candidateStructural);
    CHECK_FALSE(full.fileFingerprintMatch);
    CHECK(full.textFingerprintMatch);
    CHECK(full.metadataMatch);

    auto textChanged = fingerprint();
    textChanged.textSectionSha256 = std::string(64U, 'd');
    const auto text = registry.match(textChanged);
    CHECK_FALSE(text.exact());
    CHECK(text.kind == saors::ExecutableFingerprintMatchKind::candidateStructural);
    CHECK(text.fileFingerprintMatch);
    CHECK_FALSE(text.textFingerprintMatch);
}

TEST_CASE("Profile registry rejects each required metadata difference") {
    const saors::ExecutableProfileRegistry registry({profile()});

    auto sizeChanged = fingerprint();
    sizeChanged.fileSize += 1U;
    CHECK_FALSE(registry.match(sizeChanged).exact());

    auto timestampChanged = fingerprint();
    timestampChanged.timeDateStamp += 1U;
    CHECK_FALSE(registry.match(timestampChanged).exact());

    auto entryPointChanged = fingerprint();
    entryPointChanged.entryPointRva += 1U;
    CHECK_FALSE(registry.match(entryPointChanged).exact());

    auto imageSizeChanged = fingerprint();
    imageSizeChanged.sizeOfImage += 1U;
    CHECK_FALSE(registry.match(imageSizeChanged).exact());

    auto machineChanged = fingerprint();
    machineChanged.machine = 0x8664U;
    CHECK_FALSE(registry.match(machineChanged).exact());

    auto optionalMagicChanged = fingerprint();
    optionalMagicChanged.optionalHeaderMagic = 0x020BU;
    CHECK_FALSE(registry.match(optionalMagicChanged).exact());

    auto checksumChanged = fingerprint();
    checksumChanged.checksum += 1U;
    CHECK_FALSE(registry.match(checksumChanged).exact());
}

TEST_CASE("Profile registry rejects unknown fingerprints") {
    const saors::ExecutableProfileRegistry registry({profile()});

    auto unknown = fingerprint();
    unknown.fileSha256 = std::string(64U, 'e');
    unknown.textSectionSha256 = std::string(64U, 'f');
    unknown.fileSize += 1U;
    const auto none = registry.match(unknown);
    CHECK(none.kind == saors::ExecutableFingerprintMatchKind::none);
    CHECK(none.profileName == "none");
}

TEST_CASE("Empty and malformed profile registries never recognize an executable") {
    CHECK_FALSE(saors::ExecutableProfileRegistry{}.match(fingerprint()).exact());

    auto malformed = profile();
    malformed.fileSha256 = "not-a-sha256";
    const saors::ExecutableProfileRegistry invalid({std::move(malformed)});
    CHECK_FALSE(invalid.match(fingerprint()).exact());
}

TEST_CASE("Game integration recognizes an exact candidate but keeps all hooks and reads inert") {
    const saors::ExecutableProfileRegistry registry({profile()});
    saors::GameIntegration game(registry);

    CHECK(game.detectExecutableVersion(fingerprint()) ==
          saors::ExecutableVersion::gta3_classic_local_unmapped);
    CHECK(game.detectedExecutableDescription() == "GTA III Classic local candidate");
    CHECK(game.fingerprintStatusDescription() == "exact fingerprint match");
    CHECK(game.verificationStatusDescription() == "candidate");
    CHECK(game.fileFingerprintMatch());
    CHECK(game.textFingerprintMatch());
    CHECK_FALSE(game.installHooks());
    CHECK(game.currentStation() == -1);
    CHECK_FALSE(game.isPlayerInVehicle());
    CHECK_FALSE(game.isPauseMenuActive());
    CHECK(game.radioVolume() == 1.0F);
}

TEST_CASE("Verification and match state names remain explicit") {
    CHECK(std::string(saors::executableVerificationStatusName(
              saors::ExecutableVerificationStatus::candidate)) == "candidate");
    CHECK(std::string(saors::executableVerificationStatusName(
              saors::ExecutableVerificationStatus::locallyReproduced)) ==
          "locally_reproduced");
    CHECK(std::string(saors::executableVerificationStatusName(
              saors::ExecutableVerificationStatus::independentlyReproduced)) ==
          "independently_reproduced");
    CHECK(std::string(saors::executableVerificationStatusName(
              saors::ExecutableVerificationStatus::verified)) == "verified");
    CHECK(std::string(saors::executableFingerprintMatchKindName(
              saors::ExecutableFingerprintMatchKind::candidateStructural)) ==
          "candidate structural match");
}
