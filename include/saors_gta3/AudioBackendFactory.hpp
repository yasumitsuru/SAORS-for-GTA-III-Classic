#pragma once

#include "saors_gta3/AudioBackend.hpp"

#include <filesystem>
#include <memory>

namespace saors {

enum class RequestedAudioBackend {
    automatic,
    nullBackend,
    libVlc,
};

struct AudioBackendFactoryOptions {
    RequestedAudioBackend requested{RequestedAudioBackend::automatic};
    std::filesystem::path libVlcRoot;
    bool allowAdjacentRuntimeSearch{true};
};

[[nodiscard]] std::unique_ptr<AudioBackend> createConfiguredAudioBackend();
[[nodiscard]] std::unique_ptr<AudioBackend>
createConfiguredAudioBackend(const AudioBackendFactoryOptions& options);

} // namespace saors
