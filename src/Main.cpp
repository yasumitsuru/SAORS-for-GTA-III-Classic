#include "saors_gta3/Configuration.hpp"
#include "saors_gta3/ExecutableFingerprint.hpp"
#include "saors_gta3/FileHasher.hpp"
#include "saors_gta3/GameIntegration.hpp"
#include "saors_gta3/Logger.hpp"
#include "saors_gta3/PeImageReader.hpp"
#include "saors_gta3/RadioActionSink.hpp"
#include "saors_gta3/RadioController.hpp"

#include <windows.h>

#include <filesystem>
#include <string>

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

#if SAORS_HAS_RADIO_CONTROLLER_DRY_RUN
        if (runtimeConfiguration.experimental.enableRadioController) {
            auto* sink = new saors::DryRunRadioActionSink(
                runtimeConfiguration.experimental.logRadioDecisions);
            auto* controller = new saors::RadioController(runtimeConfiguration, *sink);
            game->setSnapshotListener(controller);
            saors::Logger::info("Radio controller: dry-run");
        } else {
            saors::Logger::info("Radio controller: disabled");
        }
#else
        if (runtimeConfiguration.experimental.enableRadioController) {
            saors::Logger::warning("Radio controller: unavailable in this build");
        } else {
            saors::Logger::info("Radio controller: disabled");
        }
#endif
        static_cast<void>(game->installHooks());
        saors::Logger::info("Game observer: " + game->observerStatusDescription());
        const auto observerResult = game->observerInstallResult();
        saors::Logger::info(std::string("Expected-byte validation: ") +
                            (observerResult.expectedBytesMatched ? "matched" : "not matched"));
        saors::Logger::info(std::string("Gameplay reads: ") +
                            (observerResult.status == saors::ObserverInstallStatus::installed
                                 ? "read-only observer active"
                                 : "disabled"));
        saors::Logger::info("Gameplay audio executor: unavailable");
        saors::Logger::info("Audio playback: not started");
        saors::Logger::info("Network activity: not started");
        saors::Logger::info("Original radio: untouched");
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
