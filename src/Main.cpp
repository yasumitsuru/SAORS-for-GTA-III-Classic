#include "saors_gta3/AudioBackend.hpp"
#include "saors_gta3/Configuration.hpp"
#include "saors_gta3/GameIntegration.hpp"
#include "saors_gta3/Logger.hpp"

#include <windows.h>

#include <filesystem>
#include <string>

namespace {

std::filesystem::path moduleDirectory(const HMODULE module) {
    std::wstring path(260, L'\0');
    for (;;) {
        const DWORD copied =
            GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        if (copied == 0) {
            return std::filesystem::current_path();
        }
        if (copied < path.size() - 1) {
            path.resize(copied);
            return std::filesystem::path(path).parent_path();
        }
        path.resize(path.size() * 2);
    }
}

DWORD WINAPI initializePlugin(const LPVOID parameter) {
    try {
        const auto module = static_cast<HMODULE>(parameter);
        const auto directory = moduleDirectory(module);
        const auto configPath = directory / "SAORSForGTA3.ini";
        const auto configuration = saors::Configuration::load(configPath);

        saors::Logger::initialize(directory / "SAORSForGTA3.log",
                                  configuration.value.general.logLevel);
        saors::Logger::info("SAORS for GTA III Classic initialized");
        saors::Logger::info("Plugin version: " SAORS_PLUGIN_VERSION);

        saors::GameIntegration game;
        static_cast<void>(game.detectExecutableVersion());
        saors::Logger::info("Detected executable: " + game.detectedExecutableDescription());

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

        const auto backend = saors::createNullAudioBackend();
        saors::Logger::info(std::string("Audio backend: ") + backend->name());
        static_cast<void>(game.installHooks());
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
