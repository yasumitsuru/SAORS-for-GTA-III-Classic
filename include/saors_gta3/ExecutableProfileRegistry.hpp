#pragma once

#include "saors_gta3/ExecutableFingerprint.hpp"
#include "saors_gta3/ExecutableProfile.hpp"

#include <optional>
#include <string>
#include <vector>

namespace saors {

enum class ExecutableFingerprintMatchKind {
    none,
    candidateStructural,
    exact,
};

struct ExecutableProfileMatch {
    ExecutableFingerprintMatchKind kind{ExecutableFingerprintMatchKind::none};
    ExecutableProfileId id{ExecutableProfileId::unsupported};
    std::string profileName{"none"};
    std::optional<ExecutableVerificationStatus> verificationStatus;
    bool fileFingerprintMatch{false};
    bool textFingerprintMatch{false};
    bool metadataMatch{false};

    [[nodiscard]] bool exact() const noexcept {
        return kind == ExecutableFingerprintMatchKind::exact;
    }
};

class ExecutableProfileRegistry {
  public:
    ExecutableProfileRegistry() = default;
    explicit ExecutableProfileRegistry(std::vector<ExecutableProfile> profiles);

    [[nodiscard]] ExecutableProfileMatch
    match(const ExecutableFingerprint& fingerprint) const;

    [[nodiscard]] const std::vector<ExecutableProfile>& profiles() const noexcept;

  private:
    std::vector<ExecutableProfile> profiles_;
};

[[nodiscard]] const ExecutableProfileRegistry& defaultExecutableProfileRegistry();
[[nodiscard]] const char*
executableFingerprintMatchKindName(ExecutableFingerprintMatchKind kind) noexcept;

} // namespace saors
