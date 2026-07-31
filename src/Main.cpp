#include "saors_gta3/AudioBackendFactory.hpp"
#include "saors_gta3/Configuration.hpp"
#include "saors_gta3/ExecutableFingerprint.hpp"
#include "saors_gta3/FileHasher.hpp"
#include "saors_gta3/GameIntegration.hpp"
#include "saors_gta3/Logger.hpp"
#include "saors_gta3/OriginalRadioController.hpp"
#include "saors_gta3/PeImageReader.hpp"
#include "saors_gta3/PlaylistResolver.hpp"
#include "saors_gta3/RadioActionSink.hpp"
#include "saors_gta3/RadioController.hpp"
#include "saors_gta3/RadioStationObservationRecorder.hpp"
#include "saors_gta3/RadioStationResolver.hpp"
#include "saors_gta3/StreamManager.hpp"

#if SAORS_HAS_WINHTTP
#include "saors_gta3/WinHttpClient.hpp"
#endif

#include <windows.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {

std::filesystem::path modulePath(const HMODULE module) {
    std::wstring path(260, L'\0');
    for (;;) {
        const DWORD copied =
            GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        if (copied == 0) {
            return {};
        }
        if (static_cast<std::size_t>(copied) < path.size() - 1) {
            path.resize(copied);
            return std::filesystem::path(path);
        }
        path.resize(path.size() * 2);
    }
}

const char* booleanName(const bool value) noexcept {
    return value ? "true" : "false";
}

class SnapshotListenerMultiplexer final : public saors::GameStateSnapshotListener {
  public:
    void setRecorder(saors::RadioStationObservationRecorder* recorder, bool logObservations) noexcept {
        recorder_ = recorder;
        logObservations_ = logObservations;
    }
    void setController(saors::GameStateSnapshotListener* controller) noexcept {
        controller_ = controller;
    }
    void onGameStateSnapshot(const saors::GameStateSnapshot& snapshot) noexcept override {
        if (recorder_ != nullptr) {
            const auto observation = recorder_->observe(snapshot);
            if (observation && logObservations_) {
                saors::Logger::info(
                    "Radio research: observation=" + std::to_string(observation->ordinal) +
                    " raw=" + std::to_string(observation->rawValue) +
                    " stable-frames=" + std::to_string(observation->stableFrames));
            }
        }
        if (controller_ != nullptr) {
            controller_->onGameStateSnapshot(snapshot);
        }
    }
  private:
    saors::RadioStationObservationRecorder* recorder_{nullptr};
    saors::GameStateSnapshotListener* controller_{nullptr};
    bool logObservations_{false};
};

struct GameplayRuntime {
    std::unique_ptr<saors::StreamManager> streams;
#if SAORS_HAS_ORIGINAL_RADIO_SUPPRESSION
    std::unique_ptr<saors::OriginalRadioController> originalRadio;
#endif
    std::unique_ptr<saors::RadioActionSink> sink;
    std::unique_ptr<saors::RadioController> controller;
};

#if SAORS_HAS_GAMEPLAY_STREAM_EXECUTOR
std::vector<std::unique_ptr<GameplayRuntime>>& gameplayRuntimes() {
    // ASI unload is unsupported. Keep backend teardown out of the loader lock;
    // the operating system releases this process-lifetime registry on exit.
    static auto* runtimes = new std::vector<std::unique_ptr<GameplayRuntime>>;
    return *runtimes;
}
#endif

DWORD WINAPI initializePlugin(const LPVOID parameter) {
    try {
        const auto module = static_cast<HMODULE>(parameter);
        const auto pluginPath = modulePath(module);
        const auto directory =
            pluginPath.empty() ? std::filesystem::current_path() : pluginPath.parent_path();
        const auto configPath = directory / "SAORSForGTA3.ini";
        const auto configuration = saors::Configuration::load(configPath);
        const auto runtimeConfiguration =
            configuration.success ? configuration.value : saors::Configuration::defaults();

        saors::Logger::initialize(directory / "SAORSForGTA3.log",
                                  runtimeConfiguration.general.logLevel);
        saors::Logger::info("SAORS for GTA III Classic initialized");
        saors::Logger::info("Plugin version: " SAORS_PLUGIN_VERSION);

        // The observer may be called for the remainder of the game process. The
        // process-lifetime allocation intentionally avoids teardown under the
        // loader lock if an external loader attempts to unload the ASI.
        auto* game = new saors::GameIntegration;
#if SAORS_HAS_EXECUTABLE_FINGERPRINTING
        const auto executablePath = modulePath(nullptr);
        auto hasher = saors::createPlatformFileHasher();
        if (!executablePath.empty() && hasher) {
            const saors::PeImageReader reader;
            const auto fingerprint = saors::fingerprintExecutable(executablePath, reader, *hasher);
            if (fingerprint) {
                static_cast<void>(game->detectExecutableVersion(fingerprint.value));
                saors::Logger::info("Executable architecture: PE32 x86");
            } else {
                saors::Logger::warning("Executable architecture: unsupported");
            }
        } else {
            saors::Logger::warning("Executable architecture: unsupported");
        }
#else
        saors::Logger::warning("Executable architecture: unsupported");
#endif
        saors::Logger::info("Executable profile: " + game->detectedExecutableDescription());
        saors::Logger::info("Fingerprint status: " + game->fingerprintStatusDescription());
        saors::Logger::info("Verification level: " + game->verificationStatusDescription());
        saors::Logger::info(std::string("File fingerprint match: ") +
                            booleanName(game->fileFingerprintMatch()));
        saors::Logger::info(std::string("Text fingerprint match: ") +
                            booleanName(game->textFingerprintMatch()));

        if (configuration.success) {
            saors::Logger::info("Configuration loaded");
        } else {
            saors::Logger::warning("Configuration loaded: built-in defaults");
            for (const auto& error : configuration.errors) {
                saors::Logger::warning(error);
            }
        }
        for (const auto& warning : configuration.warnings) {
            saors::Logger::warning(warning);
        }

        game->configureObserver(runtimeConfiguration.experimental.enableGameObserver,
                                runtimeConfiguration.experimental.observerDryRun,
                                runtimeConfiguration.experimental.logStateTransitions);

        auto* listener = new SnapshotListenerMultiplexer;
#if SAORS_HAS_EXPERIMENTAL_GAME_OBSERVER
        saors::RadioStationObservationRecorder* recorder = nullptr;
        if (runtimeConfiguration.research.enableRadioStationMapRecorder &&
            runtimeConfiguration.experimental.enableGameObserver &&
            !runtimeConfiguration.experimental.observerDryRun &&
            game->detectedExecutableProfile() != saors::ExecutableProfileId::unsupported) {
            recorder = new saors::RadioStationObservationRecorder(
                game->detectedExecutableProfile(),
                runtimeConfiguration.research.radioStationMinimumStableFrames);
            recorder->reset("runtime-session");
            listener->setRecorder(recorder, runtimeConfiguration.research.logRadioStationObservations);
            saors::Logger::info("Radio station recorder: enabled; audio and network remain disabled");
        } else if (runtimeConfiguration.research.enableRadioStationMapRecorder) {
            saors::Logger::warning(
                "Radio station recorder: requires a non-dry-run observer and an exact executable profile");
        }
#else
        if (runtimeConfiguration.research.enableRadioStationMapRecorder ||
            runtimeConfiguration.research.logRadioStationObservations) {
            saors::Logger::warning("Radio station recorder: unavailable in this build");
        }
#endif

        bool originalRadioSuppressionEnabled = false;
        bool originalRadioSuppressionStatusLogged = false;
#if SAORS_HAS_RADIO_CONTROLLER_DRY_RUN
        if (runtimeConfiguration.experimental.enableRadioController) {
            auto runtime = std::make_unique<GameplayRuntime>();
            bool gameplayExecutorEnabled = false;
#if SAORS_HAS_GAMEPLAY_STREAM_EXECUTOR
            if (runtimeConfiguration.experimental.enableGameplayAudioExecutor) {
                auto backend = saors::createConfiguredAudioBackend();
                const bool backendAvailable = std::string(backend->name()) != "null";
                if (!backendAvailable) {
                    saors::Logger::warning(
                        "Gameplay audio executor: disabled because the playback backend is unavailable");
                } else {
#if SAORS_HAS_WINHTTP
                    auto httpClient = std::make_shared<saors::WinHttpClient>();
                    auto resolver = std::make_shared<saors::PlaylistResolver>(std::move(httpClient));
                    runtime->streams = std::make_unique<saors::StreamManager>(
                        std::move(backend), std::move(resolver));
#else
                    runtime->streams = std::make_unique<saors::StreamManager>(std::move(backend));
#endif
                    gameplayExecutorEnabled = true;
#if SAORS_HAS_ORIGINAL_RADIO_SUPPRESSION
                    runtime->originalRadio = saors::createOriginalRadioController(
                        game->detectedExecutableProfile());
                    const saors::OriginalRadioSuppressionPolicy suppressionPolicy{
                        true,
                        gameplayExecutorEnabled,
                        runtimeConfiguration.experimental.muteOriginalRadioDuringGameplayAudio,
                        game->detectedExecutableProfile(),
                    };
                    originalRadioSuppressionEnabled =
                        saors::shouldEnableOriginalRadioSuppression(suppressionPolicy,
                                                                   *runtime->originalRadio);
                    if (runtimeConfiguration.experimental
                            .muteOriginalRadioDuringGameplayAudio) {
                        saors::Logger::info(
                            originalRadioSuppressionEnabled
                                ? "Original radio suppression: enabled"
                                : "Original radio suppression: unavailable");
                        originalRadioSuppressionStatusLogged = true;
                    }
#endif
                    runtime->sink = std::make_unique<saors::GameplayRadioActionSink>(
                        *runtime->streams, runtimeConfiguration.general,
#if SAORS_HAS_ORIGINAL_RADIO_SUPPRESSION
                        runtime->originalRadio.get(),
#else
                        nullptr,
#endif
                        originalRadioSuppressionEnabled);
                    saors::Logger::info(std::string("Gameplay audio executor: enabled; backend=") +
                                        runtime->streams->backendName());
                }
            }
#endif
            if (!runtime->sink) {
                runtime->sink = std::make_unique<saors::DryRunRadioActionSink>(
                    runtimeConfiguration.experimental.logRadioDecisions);
#if !SAORS_HAS_GAMEPLAY_STREAM_EXECUTOR
                if (runtimeConfiguration.experimental.enableGameplayAudioExecutor) {
                    saors::Logger::warning("Gameplay audio executor: unavailable in this build");
                }
#endif
            }
            runtime->controller = std::make_unique<saors::RadioController>(
                runtimeConfiguration, game->detectedExecutableProfile(),
                saors::defaultRadioStationResolver(), *runtime->sink);
            listener->setController(runtime->controller.get());
            game->setSnapshotListener(listener);
#if SAORS_HAS_GAMEPLAY_STREAM_EXECUTOR
            gameplayRuntimes().push_back(std::move(runtime));
#else
            static_cast<void>(runtime.release());
#endif
            saors::Logger::info(gameplayExecutorEnabled ? "Radio controller: gameplay audio"
                                                        : "Radio controller: dry-run");
        } else {
            saors::Logger::info("Radio controller: disabled");
            game->setSnapshotListener(listener);
        }
#else
        game->setSnapshotListener(listener);
        if (runtimeConfiguration.experimental.enableRadioController) {
            saors::Logger::warning("Radio controller: unavailable in this build");
        } else {
            saors::Logger::info("Radio controller: disabled");
        }
#endif
        if (runtimeConfiguration.experimental.muteOriginalRadioDuringGameplayAudio &&
            !originalRadioSuppressionStatusLogged) {
            saors::Logger::info("Original radio suppression: unavailable");
        }
        static_cast<void>(game->installHooks());
        saors::Logger::info("Game observer: " + game->observerStatusDescription());
        const auto observerResult = game->observerInstallResult();
        saors::Logger::info(std::string("Expected-byte validation: ") +
                            (observerResult.expectedBytesMatched ? "matched" : "not matched"));
        saors::Logger::info(std::string("Gameplay reads: ") +
                            (observerResult.status == saors::ObserverInstallStatus::installed
                                 ? "read-only observer active"
                                 : "disabled"));
        if (!runtimeConfiguration.experimental.enableRadioController) {
            saors::Logger::info("Gameplay audio executor: disabled");
        }
        saors::Logger::info("Audio playback: not started");
        saors::Logger::info("Network activity: not started");
        if (!originalRadioSuppressionEnabled) {
            saors::Logger::info("Original radio: untouched");
        }
    } catch (...) {
        saors::Logger::error("Unhandled exception during plugin initialization");
    }
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(const HMODULE module, const DWORD reason, const LPVOID reserved) {
    static_cast<void>(reserved);
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        const HANDLE thread = CreateThread(nullptr, 0, initializePlugin, module, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
