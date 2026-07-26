#pragma once

#include "saors_gta3/Configuration.hpp"

#include <filesystem>
#include <string_view>

namespace saors {

class Logger {
  public:
    static void initialize(std::filesystem::path logFile, LogLevel minimumLevel);
    static void log(LogLevel level, std::string_view message) noexcept;

    static void trace(std::string_view message) noexcept;
    static void debug(std::string_view message) noexcept;
    static void info(std::string_view message) noexcept;
    static void warning(std::string_view message) noexcept;
    static void error(std::string_view message) noexcept;
};

} // namespace saors
