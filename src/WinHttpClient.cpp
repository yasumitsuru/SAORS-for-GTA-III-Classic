#include "saors_gta3/WinHttpClient.hpp"

#include "saors_gta3/HttpUrl.hpp"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace saors {
namespace {

class WinHttpHandle {
  public:
    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET handle) noexcept : handle_(handle) {}

    ~WinHttpHandle() {
        if (handle_ != nullptr) {
            WinHttpCloseHandle(handle_);
        }
    }

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    WinHttpHandle(WinHttpHandle&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

    WinHttpHandle& operator=(WinHttpHandle&& other) noexcept {
        if (this != &other) {
            if (handle_ != nullptr) {
                WinHttpCloseHandle(handle_);
            }
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] HINTERNET get() const noexcept {
        return handle_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return handle_ != nullptr;
    }

  private:
    HINTERNET handle_{nullptr};
};

Result<std::wstring> toWide(const std::string& value) {
    if (value.empty()) {
        return Result<std::wstring>::ok({});
    }
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                             static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        return Result<std::wstring>::fail("HTTP URL is not valid UTF-8");
    }
    std::wstring converted(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), converted.data(),
                            required) != required) {
        return Result<std::wstring>::fail("HTTP URL UTF-8 conversion failed");
    }
    return Result<std::wstring>::ok(std::move(converted));
}

Result<std::string> toUtf8(const std::wstring& value) {
    if (value.empty()) {
        return Result<std::string>::ok({});
    }
    const int required =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return Result<std::string>::fail("HTTP response header is not valid text");
    }
    std::string converted(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), converted.data(), required, nullptr,
                            nullptr) != required) {
        return Result<std::string>::fail("HTTP response header conversion failed");
    }
    return Result<std::string>::ok(std::move(converted));
}

std::string winHttpError(const DWORD error) {
    if (error == ERROR_WINHTTP_TIMEOUT) {
        return "HTTP request timed out";
    }
    switch (error) {
    case ERROR_WINHTTP_SECURE_FAILURE:
    case ERROR_WINHTTP_SECURE_CERT_DATE_INVALID:
    case ERROR_WINHTTP_SECURE_CERT_CN_INVALID:
    case ERROR_WINHTTP_SECURE_INVALID_CA:
    case ERROR_WINHTTP_SECURE_CERT_REV_FAILED:
        return "TLS certificate validation failed";
    default:
        return "WinHTTP request failed (error " + std::to_string(error) + ")";
    }
}

Result<std::string> queryStringHeader(const HINTERNET request, const DWORD query) {
    DWORD bytes = 0;
    if (WinHttpQueryHeaders(request, query, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &bytes,
                            WINHTTP_NO_HEADER_INDEX) != FALSE) {
        return Result<std::string>::ok({});
    }
    const DWORD error = GetLastError();
    if (error == ERROR_WINHTTP_HEADER_NOT_FOUND) {
        return Result<std::string>::ok({});
    }
    if (error != ERROR_INSUFFICIENT_BUFFER || bytes < sizeof(wchar_t)) {
        return Result<std::string>::fail(winHttpError(error));
    }

    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t));
    if (WinHttpQueryHeaders(request, query, WINHTTP_HEADER_NAME_BY_INDEX, buffer.data(), &bytes,
                            WINHTTP_NO_HEADER_INDEX) == FALSE) {
        return Result<std::string>::fail(winHttpError(GetLastError()));
    }
    const std::wstring value(buffer.data());
    return toUtf8(value);
}

Result<int> queryStatusCode(const HINTERNET request) {
    DWORD status = 0;
    DWORD bytes = sizeof(status);
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &bytes,
                            WINHTTP_NO_HEADER_INDEX) == FALSE) {
        return Result<int>::fail(winHttpError(GetLastError()));
    }
    if (status > static_cast<DWORD>(std::numeric_limits<int>::max())) {
        return Result<int>::fail("HTTP status code is outside the supported range");
    }
    return Result<int>::ok(static_cast<int>(status));
}

bool isRedirectStatus(const int status) {
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

Result<HttpResponse> performGet(const HttpUrl& url, const HttpRequestOptions& options,
                                const std::size_t redirectCount) {
    if (options.cancelled()) {
        return Result<HttpResponse>::fail("HTTP request cancelled");
    }

    const auto host = toWide(url.host);
    const auto resource = toWide(url.pathAndQuery());
    if (!host) {
        return Result<HttpResponse>::fail(host.error);
    }
    if (!resource) {
        return Result<HttpResponse>::fail(resource.error);
    }

    WinHttpHandle session(WinHttpOpen(L"SAORS-for-GTA-III/0.3", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        return Result<HttpResponse>::fail(winHttpError(GetLastError()));
    }

    const auto connectTimeout = static_cast<int>(std::min<std::uint32_t>(
        options.connectTimeoutMilliseconds, static_cast<std::uint32_t>(INT_MAX)));
    const auto receiveTimeout = static_cast<int>(std::min<std::uint32_t>(
        options.receiveTimeoutMilliseconds, static_cast<std::uint32_t>(INT_MAX)));
    if (WinHttpSetTimeouts(session.get(), connectTimeout, connectTimeout, connectTimeout,
                           receiveTimeout) == FALSE) {
        return Result<HttpResponse>::fail(winHttpError(GetLastError()));
    }

    WinHttpHandle connection(WinHttpConnect(session.get(), host.value.c_str(), url.port, 0));
    if (!connection) {
        return Result<HttpResponse>::fail(winHttpError(GetLastError()));
    }

    const DWORD flags = url.secure() ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle request(WinHttpOpenRequest(connection.get(), L"GET", resource.value.c_str(),
                                             nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request) {
        return Result<HttpResponse>::fail(winHttpError(GetLastError()));
    }

    DWORD disabledFeatures = WINHTTP_DISABLE_REDIRECTS;
    if (WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabledFeatures,
                         sizeof(disabledFeatures)) == FALSE) {
        return Result<HttpResponse>::fail(winHttpError(GetLastError()));
    }
    if (options.cancelled()) {
        return Result<HttpResponse>::fail("HTTP request cancelled");
    }
    if (WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA,
                           0, 0, 0) == FALSE ||
        WinHttpReceiveResponse(request.get(), nullptr) == FALSE) {
        return Result<HttpResponse>::fail(winHttpError(GetLastError()));
    }

    const auto status = queryStatusCode(request.get());
    const auto contentType = queryStringHeader(request.get(), WINHTTP_QUERY_CONTENT_TYPE);
    if (!status) {
        return Result<HttpResponse>::fail(status.error);
    }
    if (!contentType) {
        return Result<HttpResponse>::fail(contentType.error);
    }

    if (isRedirectStatus(status.value)) {
        if (redirectCount >= options.maximumRedirects) {
            return Result<HttpResponse>::fail("HTTP redirect limit exceeded");
        }
        const auto location = queryStringHeader(request.get(), WINHTTP_QUERY_LOCATION);
        if (!location) {
            return Result<HttpResponse>::fail(location.error);
        }
        if (location.value.empty()) {
            return Result<HttpResponse>::fail("HTTP redirect response has no Location header");
        }
        const auto nextUrl = resolveHttpUrl(url.normalized, location.value);
        if (!nextUrl) {
            return Result<HttpResponse>::fail("HTTP redirect Location is invalid");
        }
        const auto parsedNext = parseHttpUrl(nextUrl.value);
        if (!parsedNext) {
            return Result<HttpResponse>::fail(parsedNext.error);
        }
        if (url.secure() && !parsedNext.value.secure()) {
            return Result<HttpResponse>::fail("HTTPS to HTTP redirect is blocked");
        }
        return performGet(parsedNext.value, options, redirectCount + 1);
    }

    HttpResponse response;
    response.statusCode = status.value;
    response.finalUrl = url.normalized;
    response.contentType = contentType.value;
    response.redirectCount = redirectCount;
    if (!options.readBody) {
        return Result<HttpResponse>::ok(std::move(response));
    }
    if (options.maximumResponseBytes == 0) {
        return Result<HttpResponse>::fail("maximum HTTP response size must be greater than zero");
    }

    DWORD declaredLength = 0;
    DWORD declaredLengthBytes = sizeof(declaredLength);
    if (WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &declaredLength, &declaredLengthBytes,
                            WINHTTP_NO_HEADER_INDEX) != FALSE &&
        static_cast<std::size_t>(declaredLength) > options.maximumResponseBytes) {
        return Result<HttpResponse>::fail("HTTP response exceeds the configured size limit");
    }

    std::array<char, 8192> buffer{};
    for (;;) {
        if (options.cancelled()) {
            return Result<HttpResponse>::fail("HTTP request cancelled");
        }
        DWORD read = 0;
        if (WinHttpReadData(request.get(), buffer.data(), static_cast<DWORD>(buffer.size()),
                            &read) == FALSE) {
            return Result<HttpResponse>::fail(winHttpError(GetLastError()));
        }
        if (read == 0) {
            break;
        }
        const auto bytesRead = static_cast<std::size_t>(read);
        if (bytesRead > options.maximumResponseBytes - response.body.size()) {
            return Result<HttpResponse>::fail("HTTP response exceeds the configured size limit");
        }
        response.body.append(buffer.data(), bytesRead);
    }
    return Result<HttpResponse>::ok(std::move(response));
}

} // namespace

Result<HttpResponse> WinHttpClient::get(const std::string& url,
                                        const HttpRequestOptions& options) noexcept {
    try {
        if (options.cancelled()) {
            return Result<HttpResponse>::fail("HTTP request cancelled");
        }
        const auto parsed = parseHttpUrl(url);
        if (!parsed) {
            return Result<HttpResponse>::fail(parsed.error);
        }
        return performGet(parsed.value, options, 0);
    } catch (...) {
        return Result<HttpResponse>::fail("WinHTTP request failed unexpectedly");
    }
}

} // namespace saors
