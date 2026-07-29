#include "saors_gta3/RadioStationResolver.hpp"

#include <algorithm>

namespace saors {

RadioStationResolver::RadioStationResolver(const RadioStationMapRegistry& registry) noexcept
    : registry_(registry) {}

RadioStationResolution
RadioStationResolver::resolve(const ExecutableProfileId executableProfile, const int rawValue) const noexcept {
    RadioStationResolution result;
    result.executableProfile = executableProfile;
    result.rawValue = rawValue;

    if (rawValue < 0 || rawValue > 255) {
        return result;
    }

    const auto* profile = registry_.find(executableProfile);
    if (profile == nullptr) {
        result.status = RadioStationResolutionStatus::unsupportedProfile;
        return result;
    }

    const auto entry = std::find_if(profile->entries.begin(), profile->entries.end(),
                                    [rawValue](const RadioStationMapEntry& candidate) {
                                        return candidate.rawValue == rawValue;
                                    });
    if (entry == profile->entries.end()) {
        result.status = RadioStationResolutionStatus::unknownRaw;
        return result;
    }

    result.status = RadioStationResolutionStatus::resolved;
    result.identity = entry->identity;
    result.evidence = entry->evidence;
    return result;
}

const RadioStationResolver& defaultRadioStationResolver() {
    static const RadioStationResolver resolver(defaultRadioStationMapRegistry());
    return resolver;
}

} // namespace saors
