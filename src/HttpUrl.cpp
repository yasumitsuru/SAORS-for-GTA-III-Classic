#include "saors_gta3/HttpUrl.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace saors {
namespace {

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool isHexDigit(const char character) {
    return std::isxdigit(static_cast<unsigned char>(character)) != 0;
}

unsigned int hexValue(const char character) {
    if (character >= '0' && character <= '9') {
        return static_cast<unsigned int>(character - '0');
    }
    const auto normalized = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return static_cast<unsigned int>(normalized - 'a' + 10);
}

bool hasValidEscapes(const std::string_view value) {
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '%') {
            continue;
        }
        if (index + 2 >= value.size() || !isHexDigit(value[index + 1]) ||
            !isHexDigit(value[index + 2])) {
            return false;
        }
        const auto decoded = (hexValue(value[index + 1]) << 4U) | hexValue(value[index + 2]);
        if (decoded < 0x20U || decoded == 0x7FU) {
            return false;
        }
        index += 2;
    }
    return true;
}

std::string removeDotSegments(const std::string_view inputPath) {
    std::vector<std::string> segments;
    std::size_t start = 0;
    while (start <= inputPath.size()) {
        const auto end = inputPath.find('/', start);
        const auto segment = std::string(inputPath.substr(
            start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (segment == "..") {
            if (!segments.empty()) {
                segments.pop_back();
            }
        } else if (!segment.empty() && segment != ".") {
            segments.push_back(segment);
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }

    std::ostringstream output;
    output << '/';
    for (std::size_t index = 0; index < segments.size(); ++index) {
        if (index != 0) {
            output << '/';
        }
        output << segments[index];
    }
    if (inputPath.size() > 1 && inputPath.back() == '/' && !segments.empty()) {
        output << '/';
    }
    return output.str();
}

Result<std::uint16_t> parsePort(const std::string_view text) {
    if (text.empty() || !std::all_of(text.begin(), text.end(), [](const unsigned char character) {
            return std::isdigit(character) != 0;
        })) {
        return Result<std::uint16_t>::fail("HTTP URL contains an invalid port");
    }

    unsigned long value = 0;
    try {
        value = std::stoul(std::string(text));
    } catch (...) {
        return Result<std::uint16_t>::fail("HTTP URL contains an invalid port");
    }
    if (value == 0 || value > std::numeric_limits<std::uint16_t>::max()) {
        return Result<std::uint16_t>::fail("HTTP URL port is outside 1-65535");
    }
    return Result<std::uint16_t>::ok(static_cast<std::uint16_t>(value));
}

std::string buildNormalized(const HttpUrl& url) {
    std::ostringstream output;
    output << url.scheme << "://";
    if (url.ipv6Literal) {
        output << '[' << lowercase(url.host) << ']';
    } else {
        output << lowercase(url.host);
    }
    const bool defaultPort =
        (url.scheme == "http" && url.port == 80) || (url.scheme == "https" && url.port == 443);
    if (url.explicitPort && !defaultPort) {
        output << ':' << url.port;
    }
    output << removeDotSegments(url.path.empty() ? "/" : url.path);
    if (!url.query.empty()) {
        output << '?' << url.query;
    }
    return output.str();
}

} // namespace

std::string HttpUrl::pathAndQuery() const {
    return query.empty() ? path : path + "?" + query;
}

bool hasHttpUrlControlCharacters(const std::string_view value) noexcept {
    return std::any_of(value.begin(), value.end(), [](const unsigned char character) {
        return character < 0x20U || character == 0x7FU;
    });
}

Result<HttpUrl> parseHttpUrl(const std::string_view value) {
    try {
        if (value.empty() || hasHttpUrlControlCharacters(value) ||
            std::any_of(value.begin(), value.end(), [](const unsigned char character) {
                return std::isspace(character) != 0 || character == '\\';
            })) {
            return Result<HttpUrl>::fail("HTTP URL is empty or contains unsafe characters");
        }

        const auto schemeEnd = value.find("://");
        if (schemeEnd == std::string_view::npos) {
            return Result<HttpUrl>::fail("URL scheme must be HTTP or HTTPS");
        }
        HttpUrl result;
        result.scheme = lowercase(std::string(value.substr(0, schemeEnd)));
        if (result.scheme != "http" && result.scheme != "https") {
            return Result<HttpUrl>::fail("URL scheme must be HTTP or HTTPS");
        }

        const auto authorityStart = schemeEnd + 3;
        const auto authorityEnd = value.find_first_of("/?#", authorityStart);
        const auto authority = value.substr(authorityStart, authorityEnd == std::string_view::npos
                                                                ? std::string_view::npos
                                                                : authorityEnd - authorityStart);
        if (authority.empty() || authority.find('@') != std::string_view::npos ||
            authority.find_first_of("<>\"") != std::string_view::npos) {
            return Result<HttpUrl>::fail("HTTP URL contains an invalid authority");
        }

        std::string_view portText;
        if (authority.front() == '[') {
            const auto bracket = authority.find(']');
            if (bracket == std::string_view::npos || bracket == 1) {
                return Result<HttpUrl>::fail("HTTP URL contains an invalid IPv6 literal");
            }
            result.ipv6Literal = true;
            result.host = std::string(authority.substr(1, bracket - 1));
            const auto remainder = authority.substr(bracket + 1);
            if (!remainder.empty()) {
                if (remainder.front() != ':' || remainder.size() == 1) {
                    return Result<HttpUrl>::fail("HTTP URL contains an invalid authority");
                }
                portText = remainder.substr(1);
                result.explicitPort = true;
            }
        } else {
            const auto colon = authority.rfind(':');
            if (colon != std::string_view::npos) {
                if (authority.find(':') != colon) {
                    return Result<HttpUrl>::fail("IPv6 literals in HTTP URLs must use brackets");
                }
                result.host = std::string(authority.substr(0, colon));
                portText = authority.substr(colon + 1);
                result.explicitPort = true;
            } else {
                result.host = std::string(authority);
            }
        }
        if (result.host.empty()) {
            return Result<HttpUrl>::fail("HTTP URL host cannot be empty");
        }

        if (result.explicitPort) {
            const auto port = parsePort(portText);
            if (!port) {
                return Result<HttpUrl>::fail(port.error);
            }
            result.port = port.value;
        } else {
            result.port = result.secure() ? 443 : 80;
        }

        std::string_view resource;
        if (authorityEnd != std::string_view::npos) {
            resource = value.substr(authorityEnd);
        }
        const auto fragment = resource.find('#');
        if (fragment != std::string_view::npos) {
            resource = resource.substr(0, fragment);
        }
        const auto query = resource.find('?');
        if (query == std::string_view::npos) {
            result.path = resource.empty() ? "/" : std::string(resource);
        } else {
            result.path = query == 0 ? "/" : std::string(resource.substr(0, query));
            result.query = std::string(resource.substr(query + 1));
        }
        if (result.path.empty() || result.path.front() != '/' || !hasValidEscapes(result.path) ||
            !hasValidEscapes(result.query)) {
            return Result<HttpUrl>::fail("HTTP URL contains an invalid path or query");
        }

        result.normalized = buildNormalized(result);
        return Result<HttpUrl>::ok(std::move(result));
    } catch (...) {
        return Result<HttpUrl>::fail("HTTP URL validation failed");
    }
}

Result<std::string> resolveHttpUrl(const std::string_view baseUrl,
                                   const std::string_view reference) {
    try {
        const auto base = parseHttpUrl(baseUrl);
        if (!base) {
            return Result<std::string>::fail("base HTTP URL is invalid");
        }
        if (reference.empty() || hasHttpUrlControlCharacters(reference) ||
            std::any_of(reference.begin(), reference.end(), [](const unsigned char character) {
                return std::isspace(character) != 0 || character == '\\';
            })) {
            return Result<std::string>::fail("relative URL is empty or contains unsafe characters");
        }

        const auto absolute = parseHttpUrl(reference);
        if (absolute) {
            return Result<std::string>::ok(absolute.value.normalized);
        }
        const auto firstDelimiter = reference.find_first_of("/?#");
        const auto colon = reference.find(':');
        if (colon != std::string_view::npos &&
            (firstDelimiter == std::string_view::npos || colon < firstDelimiter)) {
            return Result<std::string>::fail(
                "relative URL cannot contain a scheme or local drive prefix");
        }

        auto cleaned = reference.substr(0, reference.find('#'));
        std::string candidate;
        if (cleaned.rfind("//", 0) == 0) {
            candidate = base.value.scheme + ":" + std::string(cleaned);
        } else {
            std::string path;
            std::string query;
            const auto queryStart = cleaned.find('?');
            if (queryStart != std::string_view::npos) {
                query = std::string(cleaned.substr(queryStart + 1));
                cleaned = cleaned.substr(0, queryStart);
            }

            if (cleaned.empty()) {
                path = base.value.path;
                if (queryStart == std::string_view::npos) {
                    query = base.value.query;
                }
            } else if (cleaned.front() == '/') {
                path = std::string(cleaned);
            } else {
                const auto slash = base.value.path.rfind('/');
                const auto directory = slash == std::string::npos
                                           ? std::string("/")
                                           : base.value.path.substr(0, slash + 1);
                path = directory + std::string(cleaned);
            }

            HttpUrl resolved = base.value;
            resolved.path = removeDotSegments(path);
            resolved.query = std::move(query);
            candidate = buildNormalized(resolved);
        }

        const auto validated = parseHttpUrl(candidate);
        if (!validated) {
            return Result<std::string>::fail(validated.error);
        }
        return Result<std::string>::ok(validated.value.normalized);
    } catch (...) {
        return Result<std::string>::fail("relative HTTP URL resolution failed");
    }
}

} // namespace saors
