#include "saors_gta3/GameAddressProfile.hpp"

#include <initializer_list>

namespace saors {
namespace {

constexpr char pluginSdkRevision[] = "5da18b6f1956bb20bdfa39dcb07c44863ce26c81";

std::vector<std::byte> bytes(const std::initializer_list<unsigned int> values) {
    std::vector<std::byte> result;
    result.reserve(values.size());
    for (const auto value : values) {
        result.push_back(std::byte{static_cast<unsigned char>(value)});
    }
    return result;
}

ExpectedCodeBytes exactBytes(const GameSymbolId symbol, const std::uintptr_t address,
                             const std::initializer_list<unsigned int> values) {
    ExpectedCodeBytes result;
    result.symbol = symbol;
    result.address = address;
    result.expected = bytes(values);
    result.mask.assign(result.expected.size(), std::byte{0xFF});
    return result;
}

GameAddressProfile localCandidateProfile() {
    GameAddressProfile profile;
    profile.executableProfile = ExecutableProfileId::gta3_classic_local_candidate;
    profile.sourceRevision = pluginSdkRevision;
    profile.sourceVariant = "plugin-sdk GAME_10EN address set; local structural match only";
    profile.imageBase = 0x00400000U;
    profile.imageSize = 0x00582000U;

    profile.gameProcessCallback = 0x0048E49BU;
    profile.findPlayerVehicle = 0x004A10C0U;
    profile.frontEndMenuManager = 0x008F59D8U;
    profile.getRadioInCar = 0x0057CE40U;
    profile.musicVolume = 0x005F2E4CU;
    profile.dmaAudio = 0x0095CDBEU;

    profile.pauseMenuActiveOffset = 0x111U;
    profile.gameNotLoadedOffset = 0x116U;
    profile.minimumRadioStation = 0;
    profile.maximumRadioStation = 11;
    profile.minimumMusicVolume = 0;
    profile.maximumMusicVolume = 127;

    profile.expectedCode = {
        exactBytes(GameSymbolId::gameProcessCallback, 0x0048E49BU,
                   {0xE8U, 0xB0U, 0xE3U, 0xFFU, 0xFFU}),
        exactBytes(GameSymbolId::findPlayerVehicle, 0x004A10C0U,
                   {0x0FU, 0xB6U, 0x05U, 0x61U, 0xCDU, 0x95U, 0x00U, 0x6BU, 0xC0U, 0x4FU, 0x8BU,
                    0x0CU, 0x85U, 0xF0U, 0x12U, 0x94U, 0x00U}),
        exactBytes(GameSymbolId::frontEndMenuManager, 0x0048E450U,
                   {0x80U, 0x3DU, 0xE9U, 0x5AU, 0x8FU, 0x00U, 0x00U, 0x74U, 0x0AU, 0xB9U, 0xD8U,
                    0x59U, 0x8FU, 0x00U}),
        exactBytes(GameSymbolId::frontEndMenuManager, 0x00582DE0U,
                   {0xC6U, 0x05U, 0xEEU, 0x5AU, 0x8FU, 0x00U, 0x01U, 0xC6U, 0x05U, 0xF4U, 0xCCU,
                    0x95U, 0x00U, 0x01U}),
        exactBytes(
            GameSymbolId::getRadioInCar, 0x0057CE40U,
            {0x83U, 0xECU, 0x08U, 0x89U, 0x4CU, 0x24U, 0x04U, 0xB9U, 0x64U, 0x39U, 0x8FU, 0x00U}),
        exactBytes(GameSymbolId::musicVolume, 0x00486B3FU,
                   {0x83U, 0x05U, 0x4CU, 0x2EU, 0x5FU, 0x00U, 0x08U, 0x83U, 0x3DU, 0x4CU, 0x2EU,
                    0x5FU, 0x00U, 0x00U}),
    };
    return profile;
}

bool meetsMinimumVerification(const ExecutableVerificationStatus status) noexcept {
    return status == ExecutableVerificationStatus::locallyReproduced ||
           status == ExecutableVerificationStatus::independentlyReproduced ||
           status == ExecutableVerificationStatus::verified;
}

} // namespace

const char* gameSymbolName(const GameSymbolId symbol) noexcept {
    switch (symbol) {
    case GameSymbolId::gameProcessCallback:
        return "gameProcessCallback";
    case GameSymbolId::findPlayerVehicle:
        return "findPlayerVehicle";
    case GameSymbolId::frontEndMenuManager:
        return "frontEndMenuManager";
    case GameSymbolId::getRadioInCar:
        return "getRadioInCar";
    case GameSymbolId::musicVolume:
        return "musicVolume";
    }
    return "unknown";
}

const std::vector<GameAddressProfile>& defaultGameAddressProfiles() {
    static const std::vector<GameAddressProfile> profiles{localCandidateProfile()};
    return profiles;
}

const GameAddressProfile*
findGameAddressProfile(const ExecutableProfileId executableProfile,
                       const ExecutableVerificationStatus verificationStatus) noexcept {
    if (!meetsMinimumVerification(verificationStatus)) {
        return nullptr;
    }
    for (const auto& profile : defaultGameAddressProfiles()) {
        if (profile.executableProfile == executableProfile) {
            return &profile;
        }
    }
    return nullptr;
}

} // namespace saors
