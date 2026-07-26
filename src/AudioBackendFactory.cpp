#include "saors_gta3/AudioBackendFactory.hpp"

#include "saors_gta3/Logger.hpp"
#include "saors_gta3/UrlSanitizer.hpp"

#if SAORS_HAS_LIBVLC
#include "saors_gta3/LibVlcAudioBackend.hpp"
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <string>

namespace saors {
namespace {

std::string environmentValue(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) {
        return {};
    }
    const std::string result(value);
    std::free(value);
    return result;
#else
    const auto* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
#endif
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

RequestedAudioBackend requestedFromEnvironment() {
    const auto requested = lowercase(environmentValue("SAORS_AUDIO_BACKEND"));
    if (requested == "null" || requested == "none") {
        return RequestedAudioBackend::nullBackend;
    }
    if (requested == "libvlc" || requested == "vlc") {
        return RequestedAudioBackend::libVlc;
    }
    return RequestedAudioBackend::automatic;
}

const char* requestedName(const RequestedAudioBackend requested) noexcept {
    switch (requested) {
    case RequestedAudioBackend::automatic:
#if SAORS_HAS_LIBVLC
        return "libVLC";
#else
        return "null";
#endif
    case RequestedAudioBackend::nullBackend:
        return "null";
    case RequestedAudioBackend::libVlc:
        return "libVLC";
    }
    return "null";
}

} // namespace

std::unique_ptr<AudioBackend> createConfiguredAudioBackend() {
    AudioBackendFactoryOptions options;
    options.requested = requestedFromEnvironment();
    options.libVlcRoot = environmentValue("SAORS_LIBVLC_ROOT");
    return createConfiguredAudioBackend(options);
}

std::unique_ptr<AudioBackend>
createConfiguredAudioBackend(const AudioBackendFactoryOptions& options) {
    try {
        Logger::info(std::string("Audio backend requested: ") + requestedName(options.requested));

        if (options.requested == RequestedAudioBackend::nullBackend) {
            Logger::info("Audio backend selected: null");
            return createNullAudioBackend();
        }

#if SAORS_HAS_LIBVLC
        std::string error;
        auto backend = LibVlcAudioBackend::tryCreate(options.libVlcRoot,
                                                     options.allowAdjacentRuntimeSearch, error);
        if (backend) {
            Logger::info("Audio backend selected: libVLC");
            return backend;
        }
        Logger::warning("Audio backend unavailable: " + sanitizeTextForLogging(error));
#else
        if (options.requested == RequestedAudioBackend::libVlc) {
            Logger::warning(
                "Audio backend unavailable: libVLC support was not compiled into this build");
        }
#endif

        Logger::warning("Falling back to NullAudioBackend");
        return createNullAudioBackend();
    } catch (const std::exception& exception) {
        Logger::warning("Audio backend unavailable: " + sanitizeTextForLogging(exception.what()));
    } catch (...) {
        Logger::warning("Audio backend unavailable: unknown initialization error");
    }

    Logger::warning("Falling back to NullAudioBackend");
    return createNullAudioBackend();
}

} // namespace saors
