#include "saors_gta3/UrlSanitizer.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace saors {
namespace {

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool isSensitiveQueryKey(const std::string_view key) {
    const auto normalized = lowercase(std::string(key));
    return normalized == "token" || normalized == "key" || normalized == "auth";
}

std::size_t findScheme(const std::string& text, const std::size_t start) {
    const auto lowered = lowercase(text.substr(start));
    const auto http = lowered.find("http://");
    const auto https = lowered.find("https://");
    if (http == std::string::npos) {
        return https == std::string::npos ? std::string::npos : start + https;
    }
    if (https == std::string::npos) {
        return start + http;
    }
    return start + std::min(http, https);
}

} // namespace

std::string sanitizeUrlForLogging(const std::string_view url) {
    std::string sanitized(url);
    const auto schemeEnd = sanitized.find("://");
    if (schemeEnd == std::string::npos) {
        return sanitized;
    }

    const auto authorityStart = schemeEnd + 3;
    const auto authorityEnd = sanitized.find_first_of("/?#", authorityStart);
    const auto authorityLength = authorityEnd == std::string::npos
                                     ? sanitized.size() - authorityStart
                                     : authorityEnd - authorityStart;
    const auto at = sanitized.rfind('@', authorityStart + authorityLength);
    if (at != std::string::npos && at >= authorityStart && at < authorityStart + authorityLength) {
        sanitized.replace(authorityStart, at - authorityStart, "[redacted]");
    }

    const auto queryStart = sanitized.find('?', schemeEnd + 3);
    if (queryStart == std::string::npos) {
        return sanitized;
    }

    const auto fragmentStart = sanitized.find('#', queryStart + 1);
    const auto queryEnd = fragmentStart == std::string::npos ? sanitized.size() : fragmentStart;
    const std::string_view query(sanitized.data() + queryStart + 1, queryEnd - queryStart - 1);

    std::string result = sanitized.substr(0, queryStart + 1);
    std::size_t cursor = 0;
    while (cursor < query.size()) {
        const auto separator = query.find('&', cursor);
        const auto parameterEnd = separator == std::string_view::npos ? query.size() : separator;
        const auto parameter = query.substr(cursor, parameterEnd - cursor);
        const auto equals = parameter.find('=');
        if (equals != std::string_view::npos && isSensitiveQueryKey(parameter.substr(0, equals))) {
            result.append(parameter.substr(0, equals + 1));
            result.append("[redacted]");
        } else {
            result.append(parameter);
        }
        if (separator == std::string_view::npos) {
            break;
        }
        result.push_back('&');
        cursor = separator + 1;
    }
    if (fragmentStart != std::string::npos) {
        result.append(sanitized.substr(fragmentStart));
    }
    return result;
}

std::string sanitizeTextForLogging(const std::string_view text) {
    std::string sanitized(text);
    std::size_t searchFrom = 0;
    for (;;) {
        const auto urlStart = findScheme(sanitized, searchFrom);
        if (urlStart == std::string::npos) {
            break;
        }
        const auto urlEnd = sanitized.find_first_of(" \t\r\n\"'<>", urlStart);
        const auto length =
            urlEnd == std::string::npos ? sanitized.size() - urlStart : urlEnd - urlStart;
        const auto replacement =
            sanitizeUrlForLogging(std::string_view(sanitized).substr(urlStart, length));
        sanitized.replace(urlStart, length, replacement);
        searchFrom = urlStart + replacement.size();
    }
    return sanitized;
}

} // namespace saors
