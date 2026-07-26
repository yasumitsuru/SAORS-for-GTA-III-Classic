#include "saors_gta3/Logger.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <utility>

namespace saors {
namespace {

struct LoggerState {
    std::mutex mutex;
    std::filesystem::path path{"SAORSForGTA3.log"};
    LogLevel minimumLevel{LogLevel::info};
};

LoggerState& loggerState() {
    // This process-lifetime allocation intentionally has no DLL-detach destructor.
    // It avoids filesystem and mutex teardown while the Windows loader lock is held.
    static auto* state = new LoggerState;
    return *state;
}

int severity(const LogLevel level) noexcept {
    return static_cast<int>(level);
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif
    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

} // namespace

void Logger::initialize(std::filesystem::path logFile, const LogLevel minimumLevel) {
    auto& state = loggerState();
    const std::lock_guard<std::mutex> lock(state.mutex);
    state.path = std::move(logFile);
    state.minimumLevel = minimumLevel;
}

void Logger::log(const LogLevel level, const std::string_view message) noexcept {
    try {
        auto& state = loggerState();
        const std::lock_guard<std::mutex> lock(state.mutex);
        if (level == LogLevel::off || state.minimumLevel == LogLevel::off ||
            severity(level) < severity(state.minimumLevel)) {
            return;
        }

        std::ofstream output(state.path, std::ios::app);
        if (output) {
            output << '[' << timestamp() << "] [" << toString(level) << "] " << message << '\n';
        }
    } catch (...) {
        // Logging must never bring down the game process.
    }
}

void Logger::trace(const std::string_view message) noexcept {
    log(LogLevel::trace, message);
}

void Logger::debug(const std::string_view message) noexcept {
    log(LogLevel::debug, message);
}

void Logger::info(const std::string_view message) noexcept {
    log(LogLevel::info, message);
}

void Logger::warning(const std::string_view message) noexcept {
    log(LogLevel::warning, message);
}

void Logger::error(const std::string_view message) noexcept {
    log(LogLevel::error, message);
}

} // namespace saors
