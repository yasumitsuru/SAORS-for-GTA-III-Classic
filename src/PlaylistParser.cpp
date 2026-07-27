#include "saors_gta3/PlaylistParser.hpp"

#include "saors_gta3/HttpUrl.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <sstream>
#include <utility>

namespace saors {
namespace {

std::string trim(std::string value) {
    const auto notSpace = [](const unsigned char character) {
        return std::isspace(character) == 0;
    };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool hasPlsHint(const std::string_view sourceName, const std::string& content) {
    const auto source = lowercase(std::string(sourceName));
    if (source.size() >= 4 && source.substr(source.size() - 4) == ".pls") {
        return true;
    }

    std::istringstream input(content);
    std::string line;
    while (std::getline(input, line)) {
        const auto normalized = lowercase(trim(line));
        if (normalized == "[playlist]" ||
            (normalized.rfind("file", 0) == 0 && normalized.find('=') != std::string::npos)) {
            return true;
        }
    }
    return false;
}

std::optional<int> plsFileIndex(const std::string& key) {
    const auto normalized = lowercase(trim(key));
    if (normalized.rfind("file", 0) != 0 || normalized.size() <= 4) {
        return std::nullopt;
    }

    const auto number = normalized.substr(4);
    if (!std::all_of(number.begin(), number.end(),
                     [](const unsigned char character) { return std::isdigit(character) != 0; })) {
        return std::nullopt;
    }

    try {
        const int index = std::stoi(number);
        if (index <= 0) {
            return std::nullopt;
        }
        return index;
    } catch (...) {
        return std::nullopt;
    }
}

PlaylistParseResult parseM3u(const std::string& content) {
    PlaylistParseResult result;
    result.kind = PlaylistKind::m3u;

    std::istringstream input(content);
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        auto cleaned = trim(line);
        if (!cleaned.empty() && cleaned.back() == '\r') {
            cleaned.pop_back();
            cleaned = trim(std::move(cleaned));
        }
        if (cleaned.empty() || cleaned.front() == '#') {
            continue;
        }
        if (PlaylistParser::isSupportedReference(cleaned)) {
            result.urls.push_back(std::move(cleaned));
        } else {
            result.warnings.push_back("line " + std::to_string(lineNumber) +
                                      ": ignored unsafe or unsupported playlist entry");
        }
    }

    if (result.urls.empty()) {
        result.errors.emplace_back("playlist does not contain a usable HTTP(S) reference");
    }
    result.success = result.errors.empty();
    return result;
}

PlaylistParseResult parsePls(const std::string& content) {
    PlaylistParseResult result;
    result.kind = PlaylistKind::pls;

    std::istringstream input(content);
    std::map<int, std::string> indexedUrls;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        auto cleaned = trim(line);
        if (!cleaned.empty() && cleaned.back() == '\r') {
            cleaned.pop_back();
            cleaned = trim(std::move(cleaned));
        }
        if (cleaned.empty() || cleaned.front() == ';' || cleaned.front() == '#' ||
            (cleaned.front() == '[' && cleaned.back() == ']')) {
            continue;
        }

        const auto separator = cleaned.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const auto index = plsFileIndex(cleaned.substr(0, separator));
        if (!index.has_value()) {
            continue;
        }

        auto url = trim(cleaned.substr(separator + 1));
        if (PlaylistParser::isSupportedReference(url)) {
            indexedUrls[*index] = std::move(url);
        } else {
            result.warnings.push_back("line " + std::to_string(lineNumber) +
                                      ": ignored unsafe or unsupported File entry");
        }
    }

    for (auto& [index, url] : indexedUrls) {
        static_cast<void>(index);
        result.urls.push_back(std::move(url));
    }
    if (result.urls.empty()) {
        result.errors.emplace_back("PLS playlist does not contain a usable FileN reference");
    }
    result.success = result.errors.empty();
    return result;
}

} // namespace

PlaylistParseResult PlaylistParser::parse(const std::string_view content,
                                          const std::string_view sourceName) {
    const auto cleaned = trim(std::string(content));
    if (isSupportedUrl(cleaned)) {
        PlaylistParseResult result;
        result.kind = PlaylistKind::directUrl;
        result.success = true;
        result.urls.push_back(cleaned);
        return result;
    }
    if (cleaned.empty()) {
        PlaylistParseResult result;
        result.errors.emplace_back("playlist is empty");
        return result;
    }
    if (hasPlsHint(sourceName, cleaned)) {
        return parsePls(cleaned);
    }
    return parseM3u(cleaned);
}

bool PlaylistParser::isSupportedUrl(const std::string_view value) {
    const auto cleaned = trim(std::string(value));
    return static_cast<bool>(parseHttpUrl(cleaned));
}

bool PlaylistParser::isSupportedReference(const std::string_view value) {
    const auto cleaned = trim(std::string(value));
    if (cleaned.empty() || hasHttpUrlControlCharacters(cleaned) ||
        std::any_of(cleaned.begin(), cleaned.end(), [](const unsigned char character) {
            return std::isspace(character) != 0 || character == '\\' || character == '<' ||
                   character == '>' || character == '"';
        })) {
        return false;
    }
    if (isSupportedUrl(cleaned)) {
        return true;
    }

    const auto firstDelimiter = cleaned.find_first_of("/?#");
    const auto colon = cleaned.find(':');
    return colon == std::string::npos ||
           (firstDelimiter != std::string::npos && colon > firstDelimiter);
}

} // namespace saors
