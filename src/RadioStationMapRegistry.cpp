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
    static const RadioStationMapRegistry registry;
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
