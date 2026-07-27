#include "saors_gta3/WinHttpClient.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>

TEST_CASE("WinHTTP client honors cancellation before opening a connection") {
    saors::WinHttpClient client;
    std::atomic_bool cancelled{true};
    saors::HttpRequestOptions options;
    options.cancelRequested = &cancelled;

    const auto result = client.get("https://example.invalid/live", options);

    CHECK_FALSE(result);
    CHECK(result.error == "HTTP request cancelled");
}

TEST_CASE("WinHTTP client validates the scheme before opening a connection") {
    saors::WinHttpClient client;

    const auto result = client.get("file:///C:/radio/live.mp3", {});

    CHECK_FALSE(result);
    CHECK(result.error.find("HTTP or HTTPS") != std::string::npos);
}

TEST_CASE("WinHTTP client rejects unbounded timeout and body options before connecting") {
    saors::WinHttpClient client;

    saors::HttpRequestOptions noTimeout;
    noTimeout.connectTimeoutMilliseconds = 0;
    const auto timeout = client.get("https://example.invalid/live", noTimeout);
    CHECK_FALSE(timeout);
    CHECK(timeout.error == "HTTP timeouts must be greater than zero");

    saors::HttpRequestOptions noBodyLimit;
    noBodyLimit.maximumResponseBytes = 0;
    const auto bodyLimit = client.get("https://example.invalid/list.m3u", noBodyLimit);
    CHECK_FALSE(bodyLimit);
    CHECK(bodyLimit.error == "maximum HTTP response size must be greater than zero");
}
