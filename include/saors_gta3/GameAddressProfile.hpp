#pragma once

#include "saors_gta3/ExecutableProfile.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace saors {

enum class GameSymbolId {
    gameProcessCallback,
    findPlayerVehicle,
    frontEndMenuManager,
    getRadioInCar,
    musicVolume,
};

struct ExpectedCodeBytes {
    GameSymbolId symbol{GameSymbolId::gameProcessCallback};
    std::uintptr_t address{0};
    std::vector<std::byte> expected;
    std::vector<std::byte> mask;
};

struct GameAddressProfile {
    ExecutableProfileId executableProfile{ExecutableProfileId::unsupported};
    std::string sourceRevision;
    std::string sourceVariant;
    std::uintptr_t imageBase{0};
    std::size_t imageSize{0};

    std::optional<std::uintptr_t> gameProcessCallback;
    std::optional<std::uintptr_t> findPlayerVehicle;
    std::optional<std::uintptr_t> frontEndMenuManager;
    std::optional<std::uintptr_t> getRadioInCar;
    std::optional<std::uintptr_t> musicVolume;
    std::optional<std::uintptr_t> dmaAudio;

    std::size_t pauseMenuActiveOffset{0};
    std::size_t gameNotLoadedOffset{0};
    int minimumRadioStation{0};
    int maximumRadioStation{0};
    int minimumMusicVolume{0};
    int maximumMusicVolume{0};

    std::vector<ExpectedCodeBytes> expectedCode;
};

[[nodiscard]] const char* gameSymbolName(GameSymbolId symbol) noexcept;
[[nodiscard]] const std::vector<GameAddressProfile>& defaultGameAddressProfiles();
[[nodiscard]] const GameAddressProfile*
findGameAddressProfile(ExecutableProfileId executableProfile,
                       ExecutableVerificationStatus verificationStatus) noexcept;

} // namespace saors
