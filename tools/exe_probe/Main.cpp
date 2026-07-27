#include "saors_gta3/ExecutableFingerprint.hpp"
#include "saors_gta3/ExecutableReport.hpp"
#include "saors_gta3/ExecutableProfileRegistry.hpp"
#include "saors_gta3/FileHasher.hpp"
#include "saors_gta3/PeImageReader.hpp"

#include <filesystem>
#include <fstream>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

struct Options {
    std::filesystem::path executable;
    std::optional<std::filesystem::path> output;
    bool json{false};
    bool redactPath{false};
    bool compareKnown{true};
    bool help{false};
};

void printHelp() {
    std::cout
        << "SAORS executable probe " SAORS_EXE_PROBE_VERSION "\n"
        << "Usage: saors_exe_probe.exe --exe <path> [options]\n\n"
        << "  --exe <path>       Explicit PE executable path; no disk search is performed\n"
        << "  --json             Print the report as JSON instead of text\n"
        << "  --output [path]    Write JSON; default: research/local/"
           "executable.exe-profile.local.json\n"
        << "  --redact-path      Replace the executable path with [redacted]\n"
        << "  --compare-known    Compare against the built-in exact profile registry\n"
        << "  --help             Show this help\n";
}

bool isOption(const wchar_t* value) {
    return value != nullptr && value[0] == L'-' && value[1] == L'-';
}

bool parseArguments(const int argc, wchar_t** argv, Options& options, std::string& error) {
    for (int index = 1; index < argc; ++index) {
        const std::wstring_view argument(argv[index]);
        if (argument == L"--help") {
            options.help = true;
        } else if (argument == L"--json") {
            options.json = true;
        } else if (argument == L"--redact-path") {
            options.redactPath = true;
        } else if (argument == L"--compare-known") {
            options.compareKnown = true;
        } else if (argument == L"--exe") {
            if (index + 1 >= argc || isOption(argv[index + 1])) {
                error = "--exe requires an explicit path";
                return false;
            }
            options.executable = std::filesystem::path(argv[++index]);
        } else if (argument == L"--output") {
            if (index + 1 < argc && !isOption(argv[index + 1])) {
                options.output = std::filesystem::path(argv[++index]);
            } else {
                options.output = std::filesystem::path("research") / "local" /
                                 "executable.exe-profile.local.json";
            }
        } else {
            error = "unknown argument";
            return false;
        }
    }

    if (!options.help && options.executable.empty()) {
        error = "--exe is required; automatic executable discovery is not supported";
        return false;
    }
    return true;
}

bool writeJson(const std::filesystem::path& path, const std::string& report,
               std::string& error) {
    std::error_code filesystemError;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, filesystemError);
        if (filesystemError) {
            error = "unable to create the report directory";
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        error = "unable to open the JSON report for writing";
        return false;
    }
    file.write(report.data(), static_cast<std::streamsize>(report.size()));
    if (!file.good()) {
        error = "unable to write the JSON report";
        return false;
    }
    return true;
}

int run(const Options& options) {
    auto hasher = saors::createPlatformFileHasher();
    if (!hasher) {
        std::cerr << "Error: SHA-256 hashing is unavailable on this platform\n";
        return 3;
    }

    const saors::PeImageReader reader;
    const auto fingerprint =
        saors::fingerprintExecutable(options.executable, reader, *hasher);
    if (!fingerprint) {
        std::cerr << "Error: " << fingerprint.error << '\n';
        return 3;
    }

    saors::ExecutableReportContext context;
    if (options.compareKnown) {
        const auto match =
            saors::defaultExecutableProfileRegistry().match(fingerprint.value);
        context.profileName = match.profileName;
        context.matchDescription =
            saors::executableFingerprintMatchKindName(match.kind);
        if (match.verificationStatus) {
            context.verificationStatus =
                saors::executableVerificationStatusName(*match.verificationStatus);
        }
        context.supported = match.exact();
    }
    const auto text = saors::formatExecutableTextReport(
        options.executable, fingerprint.value, context, options.redactPath);
    const auto json = saors::formatExecutableJsonReport(
        options.executable, fingerprint.value, context, options.redactPath);

    std::cout << (options.json ? json : text);
    if (options.output) {
        std::string error;
        if (!writeJson(*options.output, json, error)) {
            std::cerr << "Error: " << error << '\n';
            return 4;
        }
        std::cerr << "Local JSON report written: yes\n";
    }
    return 0;
}

} // namespace

int wmain(const int argc, wchar_t** argv) {
    Options options;
    std::string error;
    if (!parseArguments(argc, argv, options, error)) {
        std::cerr << "Error: " << error << '\n';
        printHelp();
        return 2;
    }
    if (options.help) {
        printHelp();
        return 0;
    }
    try {
        return run(options);
    } catch (const std::exception& exception) {
        std::cerr << "Error: " << exception.what() << '\n';
    } catch (...) {
        std::cerr << "Error: unexpected executable-probe failure\n";
    }
    return 3;
}
