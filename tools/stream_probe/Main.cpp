#include "saors_gta3/AudioBackendFactory.hpp"
#include "saors_gta3/Logger.hpp"
#include "saors_gta3/PlaylistParser.hpp"
#include "saors_gta3/UrlSanitizer.hpp"

#if SAORS_HAS_LIBVLC
#include "saors_gta3/LibVlcAudioBackend.hpp"
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace {

using Clock = std::chrono::steady_clock;

#ifdef _WIN32
std::atomic_bool stopRequested{false};
std::atomic_bool stopHandlingCompleted{false};

bool isStopRequested() noexcept {
    return stopRequested.load(std::memory_order_relaxed);
}

void resetStopHandling() noexcept {
    stopRequested.store(false, std::memory_order_relaxed);
    stopHandlingCompleted.store(false, std::memory_order_relaxed);
}

BOOL WINAPI consoleControlHandler(const DWORD controlType) {
    switch (controlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        stopRequested.store(true, std::memory_order_relaxed);
        if (controlType == CTRL_CLOSE_EVENT) {
            constexpr int completionChecks = 40;
            constexpr DWORD completionCheckIntervalMilliseconds = 100;
            for (int check = 0;
                 check < completionChecks && !stopHandlingCompleted.load(std::memory_order_relaxed);
                 ++check) {
                Sleep(completionCheckIntervalMilliseconds);
            }
        }
        return TRUE;
    default:
        return FALSE;
    }
}
#else
volatile std::sig_atomic_t stopRequested = 0;

bool isStopRequested() noexcept {
    return stopRequested != 0;
}

void resetStopHandling() noexcept {
    stopRequested = 0;
}

void signalHandler(const int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        stopRequested = 1;
    }
}
#endif

class StopHandlerRegistration {
  public:
    StopHandlerRegistration() noexcept {
#ifdef _WIN32
        installed_ = SetConsoleCtrlHandler(consoleControlHandler, TRUE) != FALSE;
#else
        previousInterrupt_ = std::signal(SIGINT, signalHandler);
        previousTermination_ = std::signal(SIGTERM, signalHandler);
        installed_ = previousInterrupt_ != SIG_ERR && previousTermination_ != SIG_ERR;
#endif
    }

    ~StopHandlerRegistration() {
#ifdef _WIN32
        if (installed_) {
            static_cast<void>(SetConsoleCtrlHandler(consoleControlHandler, FALSE));
        }
#else
        if (previousInterrupt_ != SIG_ERR) {
            static_cast<void>(std::signal(SIGINT, previousInterrupt_));
        }
        if (previousTermination_ != SIG_ERR) {
            static_cast<void>(std::signal(SIGTERM, previousTermination_));
        }
#endif
    }

    StopHandlerRegistration(const StopHandlerRegistration&) = delete;
    StopHandlerRegistration& operator=(const StopHandlerRegistration&) = delete;

    [[nodiscard]] bool installed() const noexcept {
        return installed_;
    }

  private:
    bool installed_{false};
#ifndef _WIN32
    using SignalHandler = void (*)(int);
    SignalHandler previousInterrupt_{SIG_ERR};
    SignalHandler previousTermination_{SIG_ERR};
#endif
};

struct Options {
    std::string url;
    std::uint32_t durationSeconds{10};
    float volume{1.0F};
    std::uint32_t bufferMilliseconds{3000};
    std::optional<std::uint32_t> pauseAfterSeconds;
    std::optional<std::uint32_t> reconnectAfterSeconds;
    std::string logFile;
    bool help{false};
};

class Output {
  public:
    explicit Output(const std::string& path) {
        if (!path.empty()) {
            file_.open(path, std::ios::out | std::ios::trunc);
            ready_ = file_.is_open() && file_.good();
        }
    }

    [[nodiscard]] bool ready() const noexcept {
        return ready_;
    }

    void line(const std::string& value) {
        std::cout << value << '\n';
        if (file_.is_open()) {
            file_ << value << '\n';
            file_.flush();
        }
    }

  private:
    std::ofstream file_;
    bool ready_{true};
};

const char* stateName(const saors::AudioState state) noexcept {
    switch (state) {
    case saors::AudioState::stopped:
        return "stopped";
    case saors::AudioState::opening:
        return "opening";
    case saors::AudioState::playing:
        return "playing";
    case saors::AudioState::paused:
        return "paused";
    case saors::AudioState::error:
        return "error";
    }
    return "error";
}

std::string architectureName() {
#if defined(_WIN32) && defined(_WIN64)
    return "Windows x64";
#elif defined(_WIN32)
    return "Windows x86";
#elif defined(__linux__) && defined(__x86_64__)
    return "Linux x86_64";
#elif defined(__linux__) && defined(__i386__)
    return "Linux x86";
#else
    return "unknown";
#endif
}

void printHelp() {
    std::cout << "SAORS stream probe " SAORS_STREAM_PROBE_VERSION "\n"
              << "Usage: saors_stream_probe --url <HTTP(S) URL> [options]\n\n"
              << "Options:\n"
              << "  --url <url>              Stream URL to test\n"
              << "  --duration <seconds>     Total playback duration (default: 10)\n"
              << "  --volume <0.0-1.0>       Playback volume (default: 1.0)\n"
              << "  --buffer <milliseconds>  Network cache duration (default: 3000)\n"
              << "  --pause-after <seconds>  Pause once, then resume after one second\n"
              << "  --reconnect-after <sec>  Stop and reopen the stream once\n"
              << "  --log-file <path>        Duplicate sanitized output to a file\n"
              << "  --help                    Show this help\n\n"
              << "The libVLC backend is optional. Set SAORS_LIBVLC_ROOT to an extracted\n"
              << "Windows x86 VLC runtime when it is not adjacent in a vlc/ directory.\n";
}

bool parseUnsigned(const std::string_view text, std::uint32_t& value) {
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        parsed > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool parseVolume(const std::string& text, float& value) {
    std::size_t consumed = 0;
    try {
        value = std::stof(text, &consumed);
    } catch (...) {
        return false;
    }
    return consumed == text.size() && std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

bool parseArguments(const int argc, char** argv, Options& options, std::string& error) {
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--help" || argument == "-h") {
            options.help = true;
            continue;
        }
        if (index + 1 >= argc) {
            error = argument + " requires a value";
            return false;
        }
        const std::string value(argv[++index]);
        if (argument == "--url") {
            options.url = value;
        } else if (argument == "--duration") {
            if (!parseUnsigned(value, options.durationSeconds) || options.durationSeconds == 0) {
                error = "--duration must be a positive integer";
                return false;
            }
        } else if (argument == "--volume") {
            if (!parseVolume(value, options.volume)) {
                error = "--volume must be between 0.0 and 1.0";
                return false;
            }
        } else if (argument == "--buffer") {
            if (!parseUnsigned(value, options.bufferMilliseconds) ||
                options.bufferMilliseconds == 0) {
                error = "--buffer must be a positive integer";
                return false;
            }
        } else if (argument == "--pause-after") {
            std::uint32_t seconds = 0;
            if (!parseUnsigned(value, seconds)) {
                error = "--pause-after must be a non-negative integer";
                return false;
            }
            options.pauseAfterSeconds = seconds;
        } else if (argument == "--reconnect-after") {
            std::uint32_t seconds = 0;
            if (!parseUnsigned(value, seconds)) {
                error = "--reconnect-after must be a non-negative integer";
                return false;
            }
            options.reconnectAfterSeconds = seconds;
        } else if (argument == "--log-file") {
            options.logFile = value;
        } else {
            error = "unknown option: " + argument;
            return false;
        }
    }

    if (!options.help && options.url.empty()) {
        error = "--url is required";
        return false;
    }
    return true;
}

bool reportBackendError(const saors::AudioBackend& backend, Output& output) {
    const auto details = saors::sanitizeTextForLogging(backend.lastError());
    if (!details.empty()) {
        output.line("Error: " + details);
        return false;
    }

    switch (backend.state()) {
    case saors::AudioState::error:
        output.line("Error: audio backend entered an error state");
        break;
    case saors::AudioState::stopped:
        output.line("Error: audio backend stopped before playback completed");
        break;
    default:
        output.line("Error: audio backend failed");
        break;
    }
    return false;
}

bool waitForPlayback(saors::AudioBackend& backend, Output& output,
                     const std::chrono::seconds timeout) {
    const auto started = Clock::now();
    const auto deadline = Clock::now() + timeout;
    std::optional<saors::AudioState> previous;
    while (Clock::now() < deadline) {
        if (isStopRequested()) {
            output.line("Stop requested");
            return false;
        }
        const auto current = backend.state();
        if (!previous || current != *previous) {
            output.line(std::string("State: ") + stateName(current));
            previous = current;
        }
        if (current == saors::AudioState::playing) {
            const auto startupMilliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started);
            output.line("Startup time: " + std::to_string(startupMilliseconds.count()) + " ms");
            return true;
        }
        if (current == saors::AudioState::error || current == saors::AudioState::stopped) {
            return reportBackendError(backend, output);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    output.line("Error: timed out waiting for playback");
    return false;
}

bool sleepInterruptibly(const std::chrono::milliseconds duration) {
    const auto deadline = Clock::now() + duration;
    while (Clock::now() < deadline) {
        if (isStopRequested()) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return true;
}

bool openAndPlay(saors::AudioBackend& backend, const Options& options, Output& output) {
    output.line("Opening: " + saors::sanitizeUrlForLogging(options.url));
    if (!backend.open(options.url) || !backend.play()) {
        return reportBackendError(backend, output);
    }
    return waitForPlayback(backend, output, std::chrono::seconds(15));
}

int finishInterrupted(saors::AudioBackend& backend, Output& output) {
    backend.stop();
    output.line(std::string("State: ") + stateName(backend.state()));
    output.line("Playback test interrupted cleanly");
    return 130;
}

int runProbe(const Options& options) {
    Output output(options.logFile);
    if (!output.ready()) {
        std::cerr << "Error: could not open log file\n";
        return 2;
    }

    output.line("SAORS stream probe " SAORS_STREAM_PROBE_VERSION);
    output.line("Architecture: " + architectureName());

    if (!saors::PlaylistParser::isSupportedUrl(options.url)) {
        output.line("Error: stream URL must be an absolute HTTP(S) URL");
        return 2;
    }

    saors::Logger::initialize({}, saors::LogLevel::off);
    auto backend = saors::createConfiguredAudioBackend();
    output.line(std::string("Backend: ") + backend->name());
#if SAORS_HAS_LIBVLC
    if (const auto* libVlc = dynamic_cast<const saors::LibVlcAudioBackend*>(backend.get())) {
        const auto version = libVlc->runtimeVersion();
        if (!version.empty()) {
            output.line("Runtime: libVLC " + version);
        }
    }
#endif

    if (!backend->setVolume(options.volume) ||
        !backend->setBufferMilliseconds(options.bufferMilliseconds)) {
        reportBackendError(*backend, output);
        return 3;
    }
    if (!openAndPlay(*backend, options, output)) {
        if (isStopRequested()) {
            return finishInterrupted(*backend, output);
        }
        return 3;
    }

    const auto started = Clock::now();
    bool pauseDone = false;
    bool reconnectDone = false;
    while (Clock::now() - started < std::chrono::seconds(options.durationSeconds) &&
           !isStopRequested()) {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - started);

        if (!pauseDone && options.pauseAfterSeconds &&
            elapsed >= std::chrono::seconds(*options.pauseAfterSeconds)) {
            if (!backend->pause()) {
                reportBackendError(*backend, output);
                return 3;
            }
            output.line(std::string("State: ") + stateName(backend->state()));
            if (!sleepInterruptibly(std::chrono::seconds(1))) {
                return finishInterrupted(*backend, output);
            }
            if (!backend->play() || !waitForPlayback(*backend, output, std::chrono::seconds(15))) {
                if (isStopRequested()) {
                    return finishInterrupted(*backend, output);
                }
                return 3;
            }
            pauseDone = true;
        }

        if (!reconnectDone && options.reconnectAfterSeconds &&
            elapsed >= std::chrono::seconds(*options.reconnectAfterSeconds)) {
            backend->stop();
            output.line(std::string("State: ") + stateName(backend->state()));
            if (!openAndPlay(*backend, options, output)) {
                if (isStopRequested()) {
                    return finishInterrupted(*backend, output);
                }
                return 3;
            }
            reconnectDone = true;
        }

        const auto current = backend->state();
        if (current == saors::AudioState::error || current == saors::AudioState::stopped) {
            output.line(std::string("State: ") + stateName(current));
            reportBackendError(*backend, output);
            return 3;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (isStopRequested()) {
        return finishInterrupted(*backend, output);
    }

    backend->stop();
    output.line(std::string("State: ") + stateName(backend->state()));
    output.line("Playback test completed successfully");
    return 0;
}

} // namespace

int main(const int argc, char** argv) {
    resetStopHandling();

    Options options;
    std::string error;
    if (!parseArguments(argc, argv, options, error)) {
        std::cerr << "Error: " << saors::sanitizeTextForLogging(error) << "\n\n";
        printHelp();
        return 2;
    }
    if (options.help) {
        printHelp();
        return 0;
    }

    const StopHandlerRegistration stopHandler;
    if (!stopHandler.installed()) {
        std::cerr << "Error: could not install the console stop handler\n";
        return 4;
    }

    int exitCode = 4;
    try {
        exitCode = runProbe(options);
    } catch (const std::exception& exception) {
        std::cerr << "Error: " << saors::sanitizeTextForLogging(exception.what()) << '\n';
    } catch (...) {
        std::cerr << "Error: unknown stream probe failure\n";
    }
#ifdef _WIN32
    stopHandlingCompleted.store(true, std::memory_order_relaxed);
#endif
    return exitCode;
}
