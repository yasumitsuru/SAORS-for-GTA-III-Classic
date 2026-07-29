#include "saors_gta3/RadioStationEvidence.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string command;
    std::vector<std::string> inputs;
    std::string output;
    bool help{false};
};

void printHelp() {
    std::cout << "SAORS radio station map tool\n"
              << "Usage:\n"
              << "  saors_radio_map_tool --validate <file>\n"
              << "  saors_radio_map_tool --compare <file-a> <file-b>\n"
              << "  saors_radio_map_tool --summarize <file> [--output <file>]\n"
              << "  saors_radio_map_tool --redact <file> [--output <file>]\n"
              << "  saors_radio_map_tool --help\n";
}

bool readFile(const std::string& path, std::string& content) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    content.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return true;
}

bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output << content;
    return output.good();
}

bool parseArguments(int argc, char** argv, Options& options) {
    if (argc < 2) return false;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--help") {
            options.help = true;
            continue;
        }
        if (argument == "--output" && index + 1 < argc) {
            options.output = argv[++index];
            continue;
        }
        if ((argument == "--validate" || argument == "--summarize" || argument == "--redact") &&
            index + 1 < argc) {
            options.command = argument.substr(2);
            options.inputs = {argv[++index]};
            continue;
        }
        if (argument == "--compare" && index + 2 < argc) {
            options.command = "compare";
            options.inputs = {argv[++index], argv[++index]};
            continue;
        }
        return false;
    }
    return options.help || (!options.command.empty() && !options.inputs.empty());
}

int run(const Options& options) {
    if (options.help) {
        printHelp();
        return 0;
    }
    std::vector<saors::RadioStationEvidenceParseResult> parsed;
    for (const auto& input : options.inputs) {
        std::string content;
        if (!readFile(input, content)) {
            std::cerr << "Error: unable to read input file\n";
            return 3;
        }
        parsed.push_back(saors::parseRadioStationEvidenceJson(content));
        if (!parsed.back().success) {
            for (const auto& error : parsed.back().validation.errors) std::cerr << "Error: " << error << '\n';
            return 4;
        }
    }
    if (options.command == "validate") {
        std::cout << "valid=true\n";
        return 0;
    }
    if (options.command == "compare") {
        const auto comparison = saors::compareRadioStationEvidence(parsed[0].document, parsed[1].document);
        std::cout << "equivalent=" << (comparison.equivalent ? "true" : "false") << '\n';
        for (const auto& conflict : comparison.conflicts) std::cout << "conflict=" << conflict << '\n';
        for (const auto& warning : comparison.warnings) std::cout << "warning=" << warning << '\n';
        return comparison.conflicts.empty() ? 0 : 5;
    }
    if (options.command == "redact") {
        const auto content = saors::serializeRadioStationEvidenceJson(parsed[0].document);
        if (options.output.empty()) std::cout << content;
        else if (!writeFile(options.output, content)) return 6;
        return 0;
    }
    if (options.command == "summarize") {
        const auto map = saors::buildRadioStationMap(parsed[0].document.executableProfile,
                                                      {parsed[0].document});
        std::ostringstream summary;
        summary << "profile=" << saors::executableProfileIdName(map.executableProfile) << '\n'
                << "entries=" << map.entries.size() << '\n';
        for (const auto& entry : map.entries) {
            summary << "raw=" << entry.rawValue << " identity="
                    << saors::radioStationIdentityName(entry.identity) << " evidence="
                    << saors::radioStationEvidenceLevelName(entry.evidence) << '\n';
        }
        if (options.output.empty()) std::cout << summary.str();
        else if (!writeFile(options.output, summary.str())) return 6;
        return 0;
    }
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseArguments(argc, argv, options)) {
        printHelp();
        return 2;
    }
    return run(options);
}
