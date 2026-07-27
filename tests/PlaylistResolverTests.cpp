#include "saors_gta3/PlaylistResolver.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

class FakeHttpClient final : public saors::HttpClient {
  public:
    void respond(const std::string& url, saors::HttpResponse response) {
        responses[url].push_back(saors::Result<saors::HttpResponse>::ok(std::move(response)));
    }

    void fail(const std::string& url, const std::string& error) {
        responses[url].push_back(saors::Result<saors::HttpResponse>::fail(error));
    }

    saors::Result<saors::HttpResponse>
    get(const std::string& url, const saors::HttpRequestOptions& options) noexcept override {
        requestedUrls.push_back(url);
        bodyRequests.push_back(options.readBody);
        const auto found = responses.find(url);
        if (found == responses.end() || found->second.empty()) {
            return saors::Result<saors::HttpResponse>::fail("unexpected fake HTTP request");
        }
        auto result = std::move(found->second.front());
        found->second.pop_front();
        return result;
    }

    std::unordered_map<std::string, std::deque<saors::Result<saors::HttpResponse>>> responses;
    std::vector<std::string> requestedUrls;
    std::vector<bool> bodyRequests;
};

saors::HttpResponse response(const std::string& finalUrl, const std::string& contentType,
                             const std::string& body = {}, const int status = 200) {
    saors::HttpResponse value;
    value.statusCode = status;
    value.finalUrl = finalUrl;
    value.contentType = contentType;
    value.body = body;
    return value;
}

void directResponse(FakeHttpClient& client, const std::string& url,
                    const std::string& contentType = "audio/aac") {
    client.respond(url, response(url, contentType));
}

} // namespace

TEST_CASE("Direct HTTP and HTTPS media URLs are resolved without reading a body") {
    auto client = std::make_shared<FakeHttpClient>();
    directResponse(*client, "http://radio.example.com/live");
    directResponse(*client, "https://radio.example.com/live");
    saors::PlaylistResolver resolver(client);

    const auto http = resolver.resolve("http://radio.example.com/live");
    const auto https = resolver.resolve("https://radio.example.com/live");

    REQUIRE(http);
    CHECK(http.value.mediaUrl == "http://radio.example.com/live");
    REQUIRE(https);
    CHECK(https.value.mediaUrl == "https://radio.example.com/live");
    CHECK(client->bodyRequests == std::vector<bool>{false, false});
}

TEST_CASE("Remote M3U resolves its first usable absolute entry") {
    auto client = std::make_shared<FakeHttpClient>();
    client->respond("https://radio.example.com/list.m3u",
                    response("https://cdn.example.com/current/list.m3u", "audio/x-mpegurl",
                             "#EXTM3U\r\nftp://bad.example.com/live\r\n"
                             "https://media.example.com/live.aac\r\n"));
    directResponse(*client, "https://media.example.com/live.aac");
    saors::PlaylistResolver resolver(client);

    const auto result = resolver.resolve("https://radio.example.com/list.m3u");

    REQUIRE(result);
    CHECK(result.value.configuredUrl == "https://radio.example.com/list.m3u");
    CHECK(result.value.finalPlaylistUrl == "https://cdn.example.com/current/list.m3u");
    CHECK(result.value.mediaUrl == "https://media.example.com/live.aac");
    CHECK(result.value.playlistType == "m3u");
    CHECK(result.value.selectedEntryIndex == 0);
    CHECK(result.value.playlistEntries == 1);
}

TEST_CASE("Remote PLS is detected by Content-Type and uses numeric entry order") {
    auto client = std::make_shared<FakeHttpClient>();
    client->respond("https://radio.example.com/listen",
                    response("https://radio.example.com/listen", "audio/x-scpls"));
    client->respond("https://radio.example.com/listen",
                    response("https://radio.example.com/listen", "audio/x-scpls",
                             "[playlist]\nFile2=https://media.example.com/two\n"
                             "File1=https://media.example.com/one\n"));
    directResponse(*client, "https://media.example.com/one");
    saors::PlaylistResolver resolver(client);

    const auto result = resolver.resolve("https://radio.example.com/listen");

    REQUIRE(result);
    CHECK(result.value.mediaUrl == "https://media.example.com/one");
    CHECK(result.value.playlistType == "pls");
    CHECK(client->bodyRequests == std::vector<bool>{false, true, false});
}

TEST_CASE("Simple M3U8 is supported but HLS is explicitly rejected") {
    auto simpleClient = std::make_shared<FakeHttpClient>();
    simpleClient->respond("https://radio.example.com/simple.m3u8",
                          response("https://radio.example.com/simple.m3u8",
                                   "application/vnd.apple.mpegurl",
                                   "#EXTM3U\nhttps://media.example.com/live\n"));
    directResponse(*simpleClient, "https://media.example.com/live");
    saors::PlaylistResolver simpleResolver(simpleClient);
    REQUIRE(simpleResolver.resolve("https://radio.example.com/simple.m3u8"));

    auto hlsClient = std::make_shared<FakeHttpClient>();
    hlsClient->respond("https://radio.example.com/hls.m3u8",
                       response("https://radio.example.com/hls.m3u8",
                                "application/vnd.apple.mpegurl",
                                "#EXTM3U\n#EXT-X-TARGETDURATION:10\nsegment.aac\n"));
    saors::PlaylistResolver hlsResolver(hlsClient);
    const auto hls = hlsResolver.resolve("https://radio.example.com/hls.m3u8");

    CHECK_FALSE(hls);
    CHECK(hls.error == "HLS playlist detected but HLS resolution is not implemented");
}

TEST_CASE("text/plain must contain a syntactically valid playlist") {
    auto validClient = std::make_shared<FakeHttpClient>();
    validClient->respond("https://radio.example.com/listen",
                         response("https://radio.example.com/listen", "text/plain"));
    validClient->respond(
        "https://radio.example.com/listen",
        response("https://radio.example.com/listen", "text/plain", "#EXTM3U\n/live\n"));
    directResponse(*validClient, "https://radio.example.com/live");
    saors::PlaylistResolver validResolver(validClient);
    REQUIRE(validResolver.resolve("https://radio.example.com/listen"));

    auto invalidClient = std::make_shared<FakeHttpClient>();
    invalidClient->respond("https://radio.example.com/listen",
                           response("https://radio.example.com/listen", "text/plain"));
    invalidClient->respond(
        "https://radio.example.com/listen",
        response("https://radio.example.com/listen", "text/plain", "this is not a URL\n"));
    saors::PlaylistResolver invalidResolver(invalidClient);
    const auto invalid = invalidResolver.resolve("https://radio.example.com/listen");
    CHECK_FALSE(invalid);
    CHECK(invalid.error == "text/plain response is not a valid playlist");
}

TEST_CASE("HTTP entry in an HTTPS playlist requires explicit policy") {
    const auto playlist = response("https://radio.example.com/list.m3u", "audio/x-mpegurl",
                                   "#EXTM3U\nhttp://media.example.com/live\n");

    auto blockedClient = std::make_shared<FakeHttpClient>();
    blockedClient->respond("https://radio.example.com/list.m3u", playlist);
    saors::PlaylistResolver blockedResolver(blockedClient);
    const auto blocked = blockedResolver.resolve("https://radio.example.com/list.m3u");
    CHECK_FALSE(blocked);
    CHECK(blocked.error == "playlist contains no usable stream");

    auto allowedClient = std::make_shared<FakeHttpClient>();
    allowedClient->respond("https://radio.example.com/list.m3u", playlist);
    directResponse(*allowedClient, "http://media.example.com/live");
    saors::PlaylistResolver allowedResolver(allowedClient);
    saors::ResolveOptions options;
    options.allowHttpStreams = true;
    const auto allowed = allowedResolver.resolve("https://radio.example.com/list.m3u", options);
    REQUIRE(allowed);
    CHECK(allowed.value.mediaUrl == "http://media.example.com/live");
}

TEST_CASE("Relative entries use the redirected final playlist URL") {
    auto client = std::make_shared<FakeHttpClient>();
    client->respond("https://radio.example.com/list.m3u",
                    response("https://cdn.example.com/radio/listen/stream.m3u", "audio/x-mpegurl",
                             "#EXTM3U\n../live/audio%20one.aac?x=1\n"));
    directResponse(*client, "https://cdn.example.com/radio/live/audio%20one.aac?x=1");
    saors::PlaylistResolver resolver(client);

    const auto result = resolver.resolve("https://radio.example.com/list.m3u");

    REQUIRE(result);
    CHECK(result.value.mediaUrl == "https://cdn.example.com/radio/live/audio%20one.aac?x=1");
}

TEST_CASE("Redirect transport downgrade is rejected independently of the HTTP client") {
    auto downgradeClient = std::make_shared<FakeHttpClient>();
    downgradeClient->respond("https://radio.example.com/listen",
                             response("http://radio.example.com/list.m3u", "audio/x-mpegurl"));
    saors::PlaylistResolver downgradeResolver(downgradeClient);
    const auto downgrade = downgradeResolver.resolve("https://radio.example.com/listen");
    CHECK_FALSE(downgrade);
    CHECK(downgrade.error == "HTTPS to HTTP redirect is blocked");

    auto upgradeClient = std::make_shared<FakeHttpClient>();
    upgradeClient->respond("http://radio.example.com/listen",
                           response("https://radio.example.com/live", "audio/aac"));
    saors::PlaylistResolver upgradeResolver(upgradeClient);
    const auto upgrade = upgradeResolver.resolve("http://radio.example.com/listen");
    REQUIRE(upgrade);
    CHECK(upgrade.value.mediaUrl == "https://radio.example.com/live");
}

TEST_CASE("Nested M3U to PLS resolution is bounded and detects cycles") {
    auto nestedClient = std::make_shared<FakeHttpClient>();
    nestedClient->respond(
        "https://radio.example.com/a.m3u",
        response("https://radio.example.com/a.m3u", "audio/x-mpegurl", "#EXTM3U\nb.pls\n"));
    nestedClient->respond(
        "https://radio.example.com/b.pls",
        response("https://radio.example.com/b.pls", "audio/x-scpls", "[playlist]\nFile1=/live\n"));
    directResponse(*nestedClient, "https://radio.example.com/live");
    saors::PlaylistResolver nestedResolver(nestedClient);
    REQUIRE(nestedResolver.resolve("https://radio.example.com/a.m3u"));

    auto depthClient = std::make_shared<FakeHttpClient>();
    depthClient->respond("https://radio.example.com/a.m3u",
                         response("https://radio.example.com/a.m3u", "audio/x-mpegurl", "b.m3u\n"));
    depthClient->respond("https://radio.example.com/b.m3u",
                         response("https://radio.example.com/b.m3u", "audio/x-mpegurl", "c.m3u\n"));
    saors::PlaylistResolver depthResolver(depthClient);
    saors::ResolveOptions shallow;
    shallow.maximumDepth = 2;
    const auto depth = depthResolver.resolve("https://radio.example.com/a.m3u", shallow);
    CHECK_FALSE(depth);
    CHECK(depth.error == "playlist nesting limit exceeded");

    auto cycleClient = std::make_shared<FakeHttpClient>();
    cycleClient->respond("https://radio.example.com/a.m3u",
                         response("https://radio.example.com/a.m3u", "audio/x-mpegurl", "b.pls\n"));
    cycleClient->respond(
        "https://radio.example.com/b.pls",
        response("https://radio.example.com/b.pls", "audio/x-scpls", "[playlist]\nFile1=a.m3u\n"));
    cycleClient->respond("https://radio.example.com/a.m3u",
                         response("https://radio.example.com/a.m3u", "audio/x-mpegurl", "b.pls\n"));
    saors::PlaylistResolver cycleResolver(cycleClient);
    const auto cycle = cycleResolver.resolve("https://radio.example.com/a.m3u");
    CHECK_FALSE(cycle);
    CHECK(cycle.error == "playlist cycle detected");
}

TEST_CASE("Playlist limits statuses transport errors cancellation BOM and UTF-8 are handled") {
    SECTION("too many entries") {
        auto client = std::make_shared<FakeHttpClient>();
        client->respond("https://radio.example.com/list.m3u",
                        response("https://radio.example.com/list.m3u", "audio/x-mpegurl",
                                 "https://one.example.com/live\n"
                                 "https://two.example.com/live\n"));
        saors::PlaylistResolver resolver(client);
        saors::ResolveOptions options;
        options.maximumEntries = 1;
        const auto result = resolver.resolve("https://radio.example.com/list.m3u", options);
        CHECK_FALSE(result);
        CHECK(result.error == "playlist entry limit exceeded");
    }

    SECTION("HTTP statuses") {
        auto client = std::make_shared<FakeHttpClient>();
        client->respond("https://radio.example.com/missing.m3u",
                        response("https://radio.example.com/missing.m3u", "text/plain", {}, 404));
        saors::PlaylistResolver resolver(client);
        const auto result = resolver.resolve("https://radio.example.com/missing.m3u");
        CHECK_FALSE(result);
        CHECK(result.error == "playlist request returned HTTP status 404");
    }

    SECTION("transport and cancellation errors remain sanitized") {
        auto client = std::make_shared<FakeHttpClient>();
        client->fail("https://radio.example.com/list.m3u", "TLS certificate validation failed");
        saors::PlaylistResolver resolver(client);
        const auto tls = resolver.resolve("https://radio.example.com/list.m3u");
        CHECK_FALSE(tls);
        CHECK(tls.error == "TLS certificate validation failed");

        auto cancelledClient = std::make_shared<FakeHttpClient>();
        cancelledClient->fail("https://radio.example.com/list.m3u", "HTTP request cancelled");
        saors::PlaylistResolver cancelledResolver(cancelledClient);
        const auto cancelled = cancelledResolver.resolve("https://radio.example.com/list.m3u");
        CHECK_FALSE(cancelled);
        CHECK(cancelled.error == "HTTP request cancelled");
    }

    SECTION("UTF-8 BOM is removed and invalid UTF-8 is rejected") {
        auto bomClient = std::make_shared<FakeHttpClient>();
        bomClient->respond("https://radio.example.com/list.m3u",
                           response("https://radio.example.com/list.m3u", "audio/x-mpegurl",
                                    "\xEF\xBB\xBF#EXTM3U\r\n/live\r\n"));
        directResponse(*bomClient, "https://radio.example.com/live");
        saors::PlaylistResolver bomResolver(bomClient);
        REQUIRE(bomResolver.resolve("https://radio.example.com/list.m3u"));

        auto invalidClient = std::make_shared<FakeHttpClient>();
        invalidClient->respond(
            "https://radio.example.com/list.m3u",
            response("https://radio.example.com/list.m3u", "audio/x-mpegurl",
                     std::string("#EXTM3U\n") + static_cast<char>(0xC3) + "(\n"));
        saors::PlaylistResolver invalidResolver(invalidClient);
        const auto invalid = invalidResolver.resolve("https://radio.example.com/list.m3u");
        CHECK_FALSE(invalid);
        CHECK(invalid.error == "playlist response is not valid UTF-8");
    }
}
