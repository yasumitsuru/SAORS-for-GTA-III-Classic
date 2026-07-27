#include "saors_gta3/PlaylistResolver.hpp"

#include "saors_gta3/HttpUrl.hpp"
#include "saors_gta3/PlaylistParser.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace saors {
namespace {

struct PlaylistSignal {
    bool playlist{false};
    bool plainText{false};
    std::string parserHint;
    std::string type{"direct"};
};

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string normalizedContentType(const std::string& value) {
    const auto separator = value.find(';');
    auto normalized = lowercase(value.substr(0, separator));
    const auto notSpace = [](const unsigned char character) {
        return std::isspace(character) == 0;
    };
    normalized.erase(normalized.begin(),
                     std::find_if(normalized.begin(), normalized.end(), notSpace));
    normalized.erase(std::find_if(normalized.rbegin(), normalized.rend(), notSpace).base(),
                     normalized.end());
    return normalized;
}

std::string lowercasePath(const std::string& url) {
    const auto parsed = parseHttpUrl(url);
    return parsed ? lowercase(parsed.value.path) : std::string{};
}

bool hasSuffix(const std::string& value, const std::string_view suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

PlaylistSignal detectPlaylist(const std::string& url, const std::string& contentType) {
    PlaylistSignal signal;
    const auto path = lowercasePath(url);
    const auto type = normalizedContentType(contentType);

    if (hasSuffix(path, ".pls") || type == "audio/x-scpls" || type == "application/pls+xml") {
        signal.playlist = true;
        signal.parserHint = ".pls";
        signal.type = "pls";
        return signal;
    }
    if (hasSuffix(path, ".m3u") || hasSuffix(path, ".m3u8") || type == "audio/x-mpegurl" ||
        type == "audio/mpegurl" || type == "application/x-mpegurl" ||
        type == "application/vnd.apple.mpegurl") {
        signal.playlist = true;
        signal.parserHint = ".m3u";
        signal.type = "m3u";
        return signal;
    }
    if (type == "text/plain") {
        signal.playlist = true;
        signal.plainText = true;
        signal.parserHint = ".m3u";
        signal.type = "m3u";
    }
    return signal;
}

bool validUtf8(const std::string_view text) {
    std::size_t index = 0;
    while (index < text.size()) {
        const auto first = static_cast<unsigned char>(text[index]);
        if (first <= 0x7FU) {
            ++index;
            continue;
        }

        std::size_t continuationCount = 0;
        unsigned int minimum = 0;
        unsigned int codePoint = 0;
        if ((first & 0xE0U) == 0xC0U) {
            continuationCount = 1;
            minimum = 0x80U;
            codePoint = first & 0x1FU;
        } else if ((first & 0xF0U) == 0xE0U) {
            continuationCount = 2;
            minimum = 0x800U;
            codePoint = first & 0x0FU;
        } else if ((first & 0xF8U) == 0xF0U) {
            continuationCount = 3;
            minimum = 0x10000U;
            codePoint = first & 0x07U;
        } else {
            return false;
        }
        if (index + continuationCount >= text.size()) {
            return false;
        }
        for (std::size_t offset = 1; offset <= continuationCount; ++offset) {
            const auto continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xC0U) != 0x80U) {
                return false;
            }
            codePoint = (codePoint << 6U) | (continuation & 0x3FU);
        }
        if (codePoint < minimum || codePoint > 0x10FFFFU ||
            (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
            return false;
        }
        index += continuationCount + 1;
    }
    return true;
}

std::string stripUtf8Bom(std::string body) {
    if (body.size() >= 3 && static_cast<unsigned char>(body[0]) == 0xEFU &&
        static_cast<unsigned char>(body[1]) == 0xBBU &&
        static_cast<unsigned char>(body[2]) == 0xBFU) {
        body.erase(0, 3);
    }
    return body;
}

bool containsHlsTag(const std::string& body) {
    const auto normalized = lowercase(body);
    return normalized.find("#ext-x-targetduration") != std::string::npos ||
           normalized.find("#ext-x-media-sequence") != std::string::npos ||
           normalized.find("#ext-x-stream-inf") != std::string::npos ||
           normalized.find("#ext-x-endlist") != std::string::npos;
}

bool successfulStatus(const int status) {
    return status >= 200 && status < 300;
}

HttpRequestOptions requestOptions(const ResolveOptions& options, const bool readBody) {
    HttpRequestOptions request;
    request.connectTimeoutMilliseconds = options.connectTimeoutMilliseconds;
    request.receiveTimeoutMilliseconds = options.receiveTimeoutMilliseconds;
    request.maximumResponseBytes = options.maximumPlaylistBytes;
    request.maximumRedirects = options.maximumRedirects;
    request.readBody = readBody;
    request.cancelRequested = options.cancelRequested;
    return request;
}

class ResolutionAttempt {
  public:
    ResolutionAttempt(HttpClient& client, const std::string& configuredUrl,
                      const ResolveOptions& options)
        : client_(client), configuredUrl_(configuredUrl), options_(options) {}

    Result<ResolvedStream> run() {
        if (options_.maximumPlaylistBytes == 0 || options_.maximumEntries == 0 ||
            options_.maximumDepth == 0) {
            return Result<ResolvedStream>::fail("playlist limits must be greater than zero");
        }
        if (options_.connectTimeoutMilliseconds == 0 || options_.receiveTimeoutMilliseconds == 0) {
            return Result<ResolvedStream>::fail("playlist timeouts must be greater than zero");
        }
        return resolveResource(configuredUrl_, 0);
    }

  private:
    Result<HttpResponse> request(const std::string& url, const bool readBody) {
        const auto requestedUrl = parseHttpUrl(url);
        if (!requestedUrl) {
            return Result<HttpResponse>::fail("HTTP request URL is invalid");
        }
        const auto response = client_.get(url, requestOptions(options_, readBody));
        if (!response) {
            return Result<HttpResponse>::fail(response.error);
        }
        if (!successfulStatus(response.value.statusCode)) {
            return Result<HttpResponse>::fail("playlist request returned HTTP status " +
                                              std::to_string(response.value.statusCode));
        }
        if (response.value.finalUrl.empty()) {
            return Result<HttpResponse>::fail("HTTP client returned an empty final URL");
        }
        const auto finalUrl = parseHttpUrl(response.value.finalUrl);
        if (!finalUrl) {
            return Result<HttpResponse>::fail("HTTP client returned an invalid final URL");
        }
        if (requestedUrl.value.secure() && !finalUrl.value.secure()) {
            return Result<HttpResponse>::fail("HTTPS to HTTP redirect is blocked");
        }
        auto normalized = response.value;
        normalized.finalUrl = finalUrl.value.normalized;
        return Result<HttpResponse>::ok(std::move(normalized));
    }

    Result<ResolvedStream> resolveResource(const std::string& resourceUrl,
                                           const std::size_t depth) {
        const auto parsedResource = parseHttpUrl(resourceUrl);
        if (!parsedResource) {
            return Result<ResolvedStream>::fail(parsedResource.error);
        }

        auto signal = detectPlaylist(parsedResource.value.normalized, {});
        if (signal.playlist && depth >= options_.maximumDepth) {
            return Result<ResolvedStream>::fail("playlist nesting limit exceeded");
        }
        Result<HttpResponse> response = signal.playlist
                                            ? request(parsedResource.value.normalized, true)
                                            : request(parsedResource.value.normalized, false);
        if (!response) {
            return Result<ResolvedStream>::fail(response.error);
        }

        const auto responseSignal =
            detectPlaylist(response.value.finalUrl, response.value.contentType);
        if (!signal.playlist) {
            signal = responseSignal;
            if (signal.playlist) {
                response = request(parsedResource.value.normalized, true);
                if (!response) {
                    return Result<ResolvedStream>::fail(response.error);
                }
                signal = detectPlaylist(response.value.finalUrl, response.value.contentType);
            }
        } else if (responseSignal.playlist) {
            signal = responseSignal;
        }

        if (!signal.playlist) {
            return directResult(response.value);
        }
        if (depth >= options_.maximumDepth) {
            return Result<ResolvedStream>::fail("playlist nesting limit exceeded");
        }
        if (response.value.body.size() > options_.maximumPlaylistBytes) {
            return Result<ResolvedStream>::fail(
                "playlist response exceeds the configured size limit");
        }
        if (!validUtf8(response.value.body)) {
            return Result<ResolvedStream>::fail("playlist response is not valid UTF-8");
        }

        const auto finalUrl = parseHttpUrl(response.value.finalUrl);
        if (!finalUrl) {
            return Result<ResolvedStream>::fail("playlist final URL is invalid");
        }
        if (!visitedPlaylists_.insert(finalUrl.value.normalized).second) {
            return Result<ResolvedStream>::fail("playlist cycle detected");
        }

        const auto body = stripUtf8Bom(std::move(response.value.body));
        if (containsHlsTag(body)) {
            return Result<ResolvedStream>::fail(
                "HLS playlist detected but HLS resolution is not implemented");
        }
        const auto parsed = PlaylistParser::parse(body, signal.parserHint);
        if (!parsed.success) {
            return Result<ResolvedStream>::fail(signal.plainText
                                                    ? "text/plain response is not a valid playlist"
                                                    : "playlist contains no usable stream");
        }
        if (parsed.urls.size() > options_.maximumEntries) {
            return Result<ResolvedStream>::fail("playlist entry limit exceeded");
        }

        for (std::size_t index = 0; index < parsed.urls.size(); ++index) {
            const auto resolved = resolveHttpUrl(finalUrl.value.normalized, parsed.urls[index]);
            if (!resolved) {
                continue;
            }
            const auto entry = parseHttpUrl(resolved.value);
            if (!entry) {
                continue;
            }
            if (finalUrl.value.secure() && !entry.value.secure() && !options_.allowHttpStreams) {
                continue;
            }

            auto selected = resolveResource(entry.value.normalized, depth + 1);
            if (selected) {
                if (selected.value.finalPlaylistUrl.empty()) {
                    selected.value.finalPlaylistUrl = finalUrl.value.normalized;
                    selected.value.playlistType = signal.type;
                    selected.value.contentType = normalizedContentType(response.value.contentType);
                    selected.value.selectedEntryIndex = index;
                    selected.value.playlistEntries = parsed.urls.size();
                }
                selected.value.configuredUrl = configuredUrl_;
                return selected;
            }
            if (selected.error == "playlist cycle detected" ||
                selected.error == "playlist nesting limit exceeded" ||
                selected.error == "HTTP request cancelled" ||
                selected.error == "HLS playlist detected but HLS resolution is not implemented") {
                return selected;
            }
        }
        return Result<ResolvedStream>::fail("playlist contains no usable stream");
    }

    Result<ResolvedStream> directResult(const HttpResponse& response) const {
        const auto media = parseHttpUrl(response.finalUrl);
        if (!media) {
            return Result<ResolvedStream>::fail("resolved media URL is invalid");
        }
        ResolvedStream result;
        result.configuredUrl = configuredUrl_;
        result.mediaUrl = media.value.normalized;
        result.playlistType = "direct";
        result.contentType = normalizedContentType(response.contentType);
        return Result<ResolvedStream>::ok(std::move(result));
    }

    HttpClient& client_;
    const std::string& configuredUrl_;
    const ResolveOptions& options_;
    std::unordered_set<std::string> visitedPlaylists_;
};

} // namespace

PlaylistResolver::PlaylistResolver(std::shared_ptr<HttpClient> httpClient)
    : httpClient_(std::move(httpClient)) {}

Result<ResolvedStream> PlaylistResolver::resolve(const std::string& configuredUrl,
                                                 const ResolveOptions& options) noexcept {
    try {
        if (!httpClient_) {
            return Result<ResolvedStream>::fail("HTTP client is unavailable");
        }
        ResolutionAttempt attempt(*httpClient_, configuredUrl, options);
        return attempt.run();
    } catch (...) {
        return Result<ResolvedStream>::fail("playlist resolution failed unexpectedly");
    }
}

} // namespace saors
