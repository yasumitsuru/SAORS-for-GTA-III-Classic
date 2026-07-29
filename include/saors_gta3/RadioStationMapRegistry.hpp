#pragma once

#include "saors_gta3/RadioStationEvidence.hpp"

#include <vector>

namespace saors {

class RadioStationMapRegistry {
  public:
    RadioStationMapRegistry() = default;
    explicit RadioStationMapRegistry(std::vector<RadioStationMapProfile> profiles);

    [[nodiscard]] const RadioStationMapProfile*
    find(ExecutableProfileId profile) const noexcept;

    [[nodiscard]] const std::vector<RadioStationMapProfile>& profiles() const noexcept;

  private:
    std::vector<RadioStationMapProfile> profiles_;
};

[[nodiscard]] const RadioStationMapRegistry& defaultRadioStationMapRegistry();
[[nodiscard]] bool isRadioStationMapProfileValid(const RadioStationMapProfile& profile) noexcept;

} // namespace saors
