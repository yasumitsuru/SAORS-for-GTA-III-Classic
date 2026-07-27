#pragma once

#include "saors_gta3/ExecutableFingerprint.hpp"

#include <filesystem>
#include <string>

namespace saors {

struct ExecutableReportContext {
    std::string profileName{"none"};
    std::string matchDescription{"none"};
    std::string verificationStatus{"not registered"};
    bool supported{false};
};

[[nodiscard]] std::string
formatExecutableTextReport(const std::filesystem::path& path,
                           const ExecutableFingerprint& fingerprint,
                           const ExecutableReportContext& context, bool redactPath);

[[nodiscard]] std::string
formatExecutableJsonReport(const std::filesystem::path& path,
                           const ExecutableFingerprint& fingerprint,
                           const ExecutableReportContext& context, bool redactPath);

} // namespace saors
