#include "saors_gta3/RadioStationMapRegistry.hpp"

#include <algorithm>
#include <utility>

namespace saors {

RadioStationMapRegistry::RadioStationMapRegistry(std::vector<RadioStationMapProfile> profiles)
    : profiles_(std::move(profiles)) {}

const RadioStationMapProfile*
RadioStationMapRegistry::find(const ExecutableProfileId profile) const noexcept {
    for (const auto& candidate : profiles_) {
        if (candidate.executableProfile == profile && isRadioStationMapProfileValid(candidate)) {
            return &candidate;
        }
    }
    return nullptr;
}

const std::vector<RadioStationMapProfile>&
RadioStationMapRegistry::profiles() const noexcept {
    return profiles_;
}

const RadioStationMapRegistry& defaultRadioStationMapRegistry() {
    static const RadioStationMapRegistry registry({
        {ExecutableProfileId::gta3_classic_local_candidate,
         {
             {0, RadioStationIdentity::headRadio,
              RadioStationEvidenceLevel::locallyReproduced, 2U, 1U},
             {1, RadioStationIdentity::doubleClefFm,
              RadioStationEvidenceLevel::locallyReproduced, 2U, 1U},
             {2, RadioStationIdentity::jahRadio,
              RadioStationEvidenceLevel::locallyReproduced, 2U, 1U},
             {3, RadioStationIdentity::riseFm,
              RadioStationEvidenceLevel::locallyReproduced, 2U, 1U},
             {4, RadioStationIdentity::lips106,
              RadioStationEvidenceLevel::locallyReproduced, 2U, 1U},
             {5, RadioStationIdentity::gameFm,
              RadioStationEvidenceLevel::locallyReproduced, 2U, 1U},
             {6, RadioStationIdentity::msxFm,
              RadioStationEvidenceLevel::locallyReproduced, 2U, 1U},
             {7, RadioStationIdentity::flashback956,
              RadioStationEvidenceLevel::locallyReproduced, 2U, 1U},
             {8, RadioStationIdentity::chatterbox109,
              RadioStationEvidenceLevel::locallyReproduced, 2U, 1U},
             {9, RadioStationIdentity::mp3Player,
              RadioStationEvidenceLevel::locallyReproduced, 2U, 1U},
             {11, RadioStationIdentity::radioOff,
              RadioStationEvidenceLevel::locallyReproduced, 2U, 1U},
         },
         "phase3d-two-local-manual-sessions-conflict-free"},
    });
    return registry;
}

bool isRadioStationMapProfileValid(const RadioStationMapProfile& profile) noexcept {
    if (profile.executableProfile == ExecutableProfileId::unsupported ||
        profile.evidenceRevision.empty() || profile.entries.empty()) {
        return false;
    }
    int previousRaw = -1;
    for (const auto& entry : profile.entries) {
        if (entry.rawValue < 0 || entry.rawValue > 255 || entry.rawValue <= previousRaw ||
            entry.identity == RadioStationIdentity::unknown ||
            entry.evidence == RadioStationEvidenceLevel::unverified) {
            return false;
        }
        previousRaw = entry.rawValue;
    }
    return true;
}

} // namespace saors
