#include "saors_gta3/LibVlcAudioBackend.hpp"

#include "saors_gta3/PlaylistParser.hpp"
#include "saors_gta3/UrlSanitizer.hpp"

#include <windows.h>

#include <vlc/vlc.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace saors {
namespace {

struct LibVlcApi {
    HMODULE module{nullptr};

    decltype(&libvlc_new) newInstance{nullptr};
    decltype(&libvlc_release) releaseInstance{nullptr};
    decltype(&libvlc_get_version) getVersion{nullptr};
    decltype(&libvlc_errmsg) errorMessage{nullptr};
    decltype(&libvlc_log_unset) unsetLogging{nullptr};
    decltype(&libvlc_media_new_location) newMediaLocation{nullptr};
    decltype(&libvlc_media_add_option) addMediaOption{nullptr};
    decltype(&libvlc_media_release) releaseMedia{nullptr};
    decltype(&libvlc_media_player_new) newPlayer{nullptr};
    decltype(&libvlc_media_player_release) releasePlayer{nullptr};
    decltype(&libvlc_media_player_set_media) setPlayerMedia{nullptr};
    decltype(&libvlc_media_player_play) playPlayer{nullptr};
    decltype(&libvlc_media_player_set_pause) setPlayerPause{nullptr};
    decltype(&libvlc_media_player_stop) stopPlayer{nullptr};
    decltype(&libvlc_media_player_get_state) getPlayerState{nullptr};
    decltype(&libvlc_audio_set_volume) setAudioVolume{nullptr};
    decltype(&libvlc_media_player_set_role) setPlayerRole{nullptr};

    LibVlcApi() = default;
    ~LibVlcApi() {
        if (module != nullptr) {
            FreeLibrary(module);
        }
    }

    LibVlcApi(const LibVlcApi&) = delete;
    LibVlcApi& operator=(const LibVlcApi&) = delete;

    template <typename FunctionPointer>
    bool load(FunctionPointer& target, const char* symbol, std::string& error) {
        const auto procedure = GetProcAddress(module, symbol);
        if (procedure == nullptr) {
            error = std::string("libVLC runtime is missing required symbol ") + symbol;
            return false;
        }
        static_assert(sizeof(target) == sizeof(procedure));
        std::memcpy(&target, &procedure, sizeof(target));
        return true;
    }
};

std::filesystem::path executableDirectory() {
    std::wstring value(260, L'\0');
    for (;;) {
        const DWORD copied =
            GetModuleFileNameW(nullptr, value.data(), static_cast<DWORD>(value.size()));
        if (copied == 0) {
            return {};
        }
        if (static_cast<std::size_t>(copied) < value.size() - 1) {
            value.resize(copied);
            return std::filesystem::path(value).parent_path();
        }
        if (value.size() > static_cast<std::size_t>(std::numeric_limits<DWORD>::max() / 2)) {
            return {};
        }
        value.resize(value.size() * 2);
    }
}

bool runtimeLayoutExists(const std::filesystem::path& root) {
    std::error_code error;
    return !root.empty() && std::filesystem::is_regular_file(root / "libvlc.dll", error) &&
           std::filesystem::is_regular_file(root / "libvlccore.dll", error) &&
           std::filesystem::is_directory(root / "plugins", error);
}

std::vector<std::filesystem::path> runtimeCandidates(const std::filesystem::path& requestedRoot,
                                                     const bool allowAdjacentRuntimeSearch) {
    std::vector<std::filesystem::path> candidates;
    if (!requestedRoot.empty()) {
        candidates.push_back(requestedRoot);
    }
    if (allowAdjacentRuntimeSearch) {
        const auto executable = executableDirectory();
        if (!executable.empty()) {
            candidates.push_back(executable / "vlc");
            candidates.push_back(executable);
        }
    }
    return candidates;
}

std::string lastLibVlcError(const LibVlcApi& api, const std::string& fallback) {
    const auto* message = api.errorMessage == nullptr ? nullptr : api.errorMessage();
    if (message == nullptr || *message == '\0') {
        return fallback;
    }
    return sanitizeTextForLogging(message);
}

AudioState mapState(const libvlc_state_t state) noexcept {
    switch (state) {
    case libvlc_Opening:
    case libvlc_Buffering:
        return AudioState::opening;
    case libvlc_Playing:
        return AudioState::playing;
    case libvlc_Paused:
        return AudioState::paused;
    case libvlc_Error:
        return AudioState::error;
    case libvlc_NothingSpecial:
    case libvlc_Stopped:
    case libvlc_Ended:
        return AudioState::stopped;
    }
    return AudioState::error;
}

} // namespace

struct LibVlcAudioBackend::Impl {
    std::mutex mutex;
    LibVlcApi api;
    libvlc_instance_t* instance{nullptr};
    libvlc_media_player_t* player{nullptr};
    std::filesystem::path root;
    std::string lastError;
    AudioState observedState{AudioState::stopped};
    std::uint32_t bufferMilliseconds{3000};
    float volume{1.0F};
    bool hasMedia{false};
    bool pauseRequested{false};

    ~Impl() {
        try {
            if (player != nullptr) {
                if (hasMedia) {
                    api.stopPlayer(player);
                    api.setPlayerMedia(player, nullptr);
                    hasMedia = false;
                }
                api.releasePlayer(player);
                player = nullptr;
            }
            if (instance != nullptr) {
                api.releaseInstance(instance);
                instance = nullptr;
            }
        } catch (...) {
            // External-library shutdown must not escape into the host process.
        }
    }
};

LibVlcAudioBackend::LibVlcAudioBackend(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

LibVlcAudioBackend::~LibVlcAudioBackend() = default;

std::unique_ptr<LibVlcAudioBackend>
LibVlcAudioBackend::tryCreate(const std::filesystem::path& runtimeRoot,
                              const bool allowAdjacentRuntimeSearch, std::string& error) noexcept {
    error.clear();
    try {
        auto impl = std::make_unique<Impl>();
        const auto candidates = runtimeCandidates(runtimeRoot, allowAdjacentRuntimeSearch);
        const auto selected =
            std::find_if(candidates.begin(), candidates.end(), runtimeLayoutExists);
        if (selected == candidates.end()) {
            error = "libVLC runtime layout is incomplete; libvlc.dll, libvlccore.dll, and plugins/ "
                    "must share one root";
            return nullptr;
        }
        impl->root = *selected;

        const auto libraryPath = impl->root / "libvlc.dll";
        impl->api.module =
            LoadLibraryExW(libraryPath.c_str(), nullptr,
                           LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (impl->api.module == nullptr) {
            const DWORD windowsError = GetLastError();
            if (windowsError == ERROR_BAD_EXE_FORMAT) {
                error = "libvlc.dll architecture is incompatible with this Windows x86 process";
            } else {
                error = "libvlc.dll could not be loaded (Windows error " +
                        std::to_string(windowsError) + ')';
            }
            return nullptr;
        }

#define SAORS_LOAD_LIBVLC(member, symbol)                                                          \
    if (!impl->api.load(impl->api.member, symbol, error)) {                                        \
        return nullptr;                                                                            \
    }

        SAORS_LOAD_LIBVLC(newInstance, "libvlc_new")
        SAORS_LOAD_LIBVLC(releaseInstance, "libvlc_release")
        SAORS_LOAD_LIBVLC(getVersion, "libvlc_get_version")
        SAORS_LOAD_LIBVLC(errorMessage, "libvlc_errmsg")
        SAORS_LOAD_LIBVLC(unsetLogging, "libvlc_log_unset")
        SAORS_LOAD_LIBVLC(newMediaLocation, "libvlc_media_new_location")
        SAORS_LOAD_LIBVLC(addMediaOption, "libvlc_media_add_option")
        SAORS_LOAD_LIBVLC(releaseMedia, "libvlc_media_release")
        SAORS_LOAD_LIBVLC(newPlayer, "libvlc_media_player_new")
        SAORS_LOAD_LIBVLC(releasePlayer, "libvlc_media_player_release")
        SAORS_LOAD_LIBVLC(setPlayerMedia, "libvlc_media_player_set_media")
        SAORS_LOAD_LIBVLC(playPlayer, "libvlc_media_player_play")
        SAORS_LOAD_LIBVLC(setPlayerPause, "libvlc_media_player_set_pause")
        SAORS_LOAD_LIBVLC(stopPlayer, "libvlc_media_player_stop")
        SAORS_LOAD_LIBVLC(getPlayerState, "libvlc_media_player_get_state")
        SAORS_LOAD_LIBVLC(setAudioVolume, "libvlc_audio_set_volume")
        SAORS_LOAD_LIBVLC(setPlayerRole, "libvlc_media_player_set_role")

#undef SAORS_LOAD_LIBVLC

        constexpr const char* arguments[]{
            "--no-video",
            "--no-video-title-show",
            "--intf=dummy",
        };
        impl->instance = impl->api.newInstance(static_cast<int>(std::size(arguments)), arguments);
        if (impl->instance == nullptr) {
            error = lastLibVlcError(impl->api, "libvlc_new failed");
            return nullptr;
        }
        impl->api.unsetLogging(impl->instance);

        impl->player = impl->api.newPlayer(impl->instance);
        if (impl->player == nullptr) {
            error = lastLibVlcError(impl->api, "libvlc_media_player_new failed");
            return nullptr;
        }
        static_cast<void>(impl->api.setPlayerRole(impl->player, libvlc_role_Music));
        return std::unique_ptr<LibVlcAudioBackend>(new LibVlcAudioBackend(std::move(impl)));
    } catch (const std::exception& exception) {
        error = sanitizeTextForLogging(exception.what());
    } catch (...) {
        error = "unknown exception while initializing libVLC";
    }
    return nullptr;
}

bool LibVlcAudioBackend::open(const std::string& url) {
    try {
        if (!PlaylistParser::isSupportedUrl(url)) {
            const std::lock_guard<std::mutex> lock(impl_->mutex);
            impl_->lastError = "stream URL must be an absolute HTTP(S) URL";
            impl_->observedState = AudioState::error;
            return false;
        }

        const std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->hasMedia) {
            impl_->api.stopPlayer(impl_->player);
            impl_->api.setPlayerMedia(impl_->player, nullptr);
            impl_->hasMedia = false;
        }
        impl_->pauseRequested = false;

        auto* media = impl_->api.newMediaLocation(impl_->instance, url.c_str());
        if (media == nullptr) {
            impl_->lastError = lastLibVlcError(impl_->api, "libVLC could not create media");
            impl_->observedState = AudioState::error;
            return false;
        }

        const auto caching = ":network-caching=" + std::to_string(impl_->bufferMilliseconds);
        impl_->api.addMediaOption(media, caching.c_str());
        impl_->api.setPlayerMedia(impl_->player, media);
        impl_->api.releaseMedia(media);
        impl_->hasMedia = true;
        impl_->lastError.clear();
        impl_->observedState = AudioState::opening;
        return true;
    } catch (const std::exception& exception) {
        const std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->lastError = sanitizeTextForLogging(exception.what());
        impl_->observedState = AudioState::error;
    } catch (...) {
        const std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->lastError = "unknown exception while opening stream";
        impl_->observedState = AudioState::error;
    }
    return false;
}

bool LibVlcAudioBackend::play() {
    try {
        const std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->hasMedia) {
            impl_->lastError = "play requires a stream to be opened first";
            impl_->observedState = AudioState::error;
            return false;
        }

        if (impl_->pauseRequested) {
            impl_->api.setPlayerPause(impl_->player, 0);
            impl_->pauseRequested = false;
        } else if (impl_->api.playPlayer(impl_->player) != 0) {
            impl_->lastError = lastLibVlcError(impl_->api, "libVLC could not start playback");
            impl_->observedState = AudioState::error;
            return false;
        }
        impl_->lastError.clear();
        impl_->observedState = AudioState::opening;
        return true;
    } catch (...) {
        try {
            const std::lock_guard<std::mutex> lock(impl_->mutex);
            impl_->lastError = "exception while starting playback";
            impl_->observedState = AudioState::error;
        } catch (...) {
        }
        return false;
    }
}

bool LibVlcAudioBackend::pause() {
    try {
        const std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->hasMedia || (impl_->observedState != AudioState::playing &&
                                 impl_->observedState != AudioState::opening)) {
            impl_->lastError = "pause requires an active stream";
            impl_->observedState = AudioState::error;
            return false;
        }
        impl_->api.setPlayerPause(impl_->player, 1);
        impl_->pauseRequested = true;
        impl_->lastError.clear();
        impl_->observedState = AudioState::paused;
        return true;
    } catch (...) {
        try {
            const std::lock_guard<std::mutex> lock(impl_->mutex);
            impl_->lastError = "exception while pausing playback";
            impl_->observedState = AudioState::error;
        } catch (...) {
        }
        return false;
    }
}

void LibVlcAudioBackend::stop() noexcept {
    try {
        const std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->hasMedia) {
            impl_->api.stopPlayer(impl_->player);
            impl_->api.setPlayerMedia(impl_->player, nullptr);
            impl_->hasMedia = false;
        }
        impl_->pauseRequested = false;
        impl_->lastError.clear();
        impl_->observedState = AudioState::stopped;
    } catch (...) {
        // Repeated stop and shutdown are deliberately idempotent.
    }
}

bool LibVlcAudioBackend::setVolume(const float volume) {
    try {
        const std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!std::isfinite(volume) || volume < 0.0F || volume > 1.0F) {
            impl_->lastError = "volume must be between 0.0 and 1.0";
            impl_->observedState = AudioState::error;
            return false;
        }
        const int percentage = static_cast<int>(std::lround(volume * 100.0F));
        if (impl_->api.setAudioVolume(impl_->player, percentage) != 0) {
            impl_->lastError = lastLibVlcError(impl_->api, "libVLC rejected the volume");
            impl_->observedState = AudioState::error;
            return false;
        }
        impl_->volume = volume;
        impl_->lastError.clear();
        return true;
    } catch (...) {
        return false;
    }
}

bool LibVlcAudioBackend::setBufferMilliseconds(const std::uint32_t milliseconds) {
    try {
        const std::lock_guard<std::mutex> lock(impl_->mutex);
        if (milliseconds == 0 ||
            milliseconds > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
            impl_->lastError = "buffer duration must be between 1 and INT_MAX milliseconds";
            impl_->observedState = AudioState::error;
            return false;
        }
        impl_->bufferMilliseconds = milliseconds;
        impl_->lastError.clear();
        return true;
    } catch (...) {
        return false;
    }
}

AudioState LibVlcAudioBackend::state() const noexcept {
    try {
        const std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->hasMedia) {
            return impl_->observedState;
        }
        const auto nativeState = impl_->api.getPlayerState(impl_->player);
        if (impl_->pauseRequested && nativeState != libvlc_Error && nativeState != libvlc_Stopped &&
            nativeState != libvlc_Ended) {
            return AudioState::paused;
        }
        if (nativeState == libvlc_NothingSpecial && impl_->observedState == AudioState::opening) {
            return AudioState::opening;
        }
        if ((nativeState == libvlc_Stopped || nativeState == libvlc_Ended) &&
            impl_->observedState == AudioState::opening) {
            impl_->lastError = lastLibVlcError(impl_->api, "libVLC stopped before playback began");
            impl_->observedState = AudioState::error;
            return AudioState::error;
        }
        const auto mapped = mapState(nativeState);
        if (mapped == AudioState::error) {
            impl_->lastError = lastLibVlcError(impl_->api, "libVLC reported a playback error");
        }
        impl_->observedState = mapped;
        return mapped;
    } catch (...) {
        return AudioState::error;
    }
}

std::string LibVlcAudioBackend::lastError() const {
    try {
        const std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->lastError;
    } catch (...) {
        return "error details unavailable";
    }
}

const char* LibVlcAudioBackend::name() const noexcept {
    return "libVLC";
}

std::string LibVlcAudioBackend::runtimeVersion() const {
    try {
        const std::lock_guard<std::mutex> lock(impl_->mutex);
        const auto* version = impl_->api.getVersion();
        return version == nullptr ? std::string{} : std::string(version);
    } catch (...) {
        return {};
    }
}

std::filesystem::path LibVlcAudioBackend::runtimeRoot() const {
    try {
        const std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->root;
    } catch (...) {
        return {};
    }
}

} // namespace saors
