#pragma once

#include "saors_gta3/RadioStationMapRegistry.hpp"

#include <optional>

namespace saors {

enum class RadioStationResolutionStatus {
    resolved,
    unknownRaw,
    unsupportedProfile,
    invalidRaw,
};

struct RadioStationResolution {
    RadioStationResolutionStatus status{RadioStationResolutionStatus::invalidRaw};
    ExecutableProfileId executableProfile{ExecutableProfileId::unsupported};
    int rawValue{-1};
    std::optional<RadioStationIdentity> identity;
    std::optional<RadioStationEvidenceLevel> evidence;
};

class RadioStationResolver final {
  public:
    explicit RadioStationResolver(const RadioStationMapRegistry& registry) noexcept;

    [[nodiscard]] RadioStationResolution
    resolve(ExecutableProfileId executableProfile, int rawValue) const noexcept;

  private:
    const RadioStationMapRegistry& registry_;
};

[[nodiscard]] const RadioStationResolver& defaultRadioStationResolver();

} // namespace saors
