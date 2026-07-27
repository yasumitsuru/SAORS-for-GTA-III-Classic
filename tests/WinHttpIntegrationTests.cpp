#include "saors_gta3/PlaylistResolver.hpp"
#include "saors_gta3/WinHttpClient.hpp"

#include <catch2/catch_test_macros.hpp>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {

class LocalHttpServer {
  public:
    LocalHttpServer() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
        socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket_ == INVALID_SOCKET) {
            WSACleanup();
            throw std::runtime_error("test server socket creation failed");
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        if (bind(socket_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) ==
                SOCKET_ERROR ||
            listen(socket_, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(socket_);
            WSACleanup();
            throw std::runtime_error("test server bind failed");
        }

        int addressSize = sizeof(address);
        if (getsockname(socket_, reinterpret_cast<sockaddr*>(&address), &addressSize) ==
            SOCKET_ERROR) {
            closesocket(socket_);
            WSACleanup();
            throw std::runtime_error("test server port lookup failed");
        }
        port_ = ntohs(address.sin_port);
        thread_ = std::thread([this] { serve(); });
    }

    ~LocalHttpServer() {
        stopping_.store(true, std::memory_order_relaxed);
        if (socket_ != INVALID_SOCKET) {
            static_cast<void>(shutdown(socket_, SD_BOTH));
            closesocket(socket_);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
        WSACleanup();
    }

    LocalHttpServer(const LocalHttpServer&) = delete;
    LocalHttpServer& operator=(const LocalHttpServer&) = delete;

    [[nodiscard]] std::string url(const std::string& path) const {
        return "http://127.0.0.1:" + std::to_string(port_) + path;
    }

  private:
    struct Reply {
        int status{200};
        std::string reason{"OK"};
        std::string contentType{"text/plain"};
        std::string body;
        std::string location;
    };

    void serve() noexcept {
        while (!stopping_.load(std::memory_order_relaxed)) {
            const SOCKET client = accept(socket_, nullptr, nullptr);
            if (client == INVALID_SOCKET) {
                break;
            }
            handle(client);
            closesocket(client);
        }
    }

    static std::string requestPath(const std::string& request) {
        const auto firstSpace = request.find(' ');
        if (firstSpace == std::string::npos) {
            return {};
        }
        const auto secondSpace = request.find(' ', firstSpace + 1);
        if (secondSpace == std::string::npos) {
            return {};
        }
        return request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    }

    Reply route(const std::string& path) const {
        if (path == "/direct-stream") {
            return {200, "OK", "audio/aac", "test-media", {}};
        }
        if (path == "/list.m3u") {
            return {
                200, "OK", "audio/x-mpegurl", "#EXTM3U\r\n" + url("/direct-stream") + "\r\n", {}};
        }
        if (path == "/list.pls") {
            return {200,
                    "OK",
                    "audio/x-scpls",
                    "[playlist]\r\nFile1=" + url("/direct-stream") + "\r\nNumberOfEntries=1\r\n",
                    {}};
        }
        if (path == "/relative/list.m3u") {
            return {200, "OK", "audio/x-mpegurl", "#EXTM3U\n../direct-stream\n", {}};
        }
        if (path == "/nested-a.m3u") {
            return {200, "OK", "audio/x-mpegurl", "nested-b.pls\n", {}};
        }
        if (path == "/nested-b.pls") {
            return {200, "OK", "audio/x-scpls", "[playlist]\nFile1=/direct-stream\n", {}};
        }
        if (path == "/redirect") {
            return {302, "Found", "text/plain", {}, "/list.m3u"};
        }
        if (path == "/cycle-a.m3u") {
            return {200, "OK", "audio/x-mpegurl", "cycle-b.m3u\n", {}};
        }
        if (path == "/cycle-b.m3u") {
            return {200, "OK", "audio/x-mpegurl", "cycle-a.m3u\n", {}};
        }
        if (path == "/too-large.m3u") {
            return {200, "OK", "audio/x-mpegurl", std::string(300U * 1024U, 'a'), {}};
        }
        if (path == "/hls.m3u8") {
            return {200,
                    "OK",
                    "application/vnd.apple.mpegurl",
                    "#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:1\nsegment.aac\n",
                    {}};
        }
        return {404, "Not Found", "text/plain", "missing", {}};
    }

    static void sendAll(const SOCKET client, const std::string& data) noexcept {
        std::size_t sent = 0;
        while (sent < data.size()) {
            const auto remaining = data.size() - sent;
            const auto chunk = static_cast<int>((remaining > 16384U) ? 16384U : remaining);
            const int result = send(client, data.data() + sent, chunk, 0);
            if (result <= 0) {
                return;
            }
            sent += static_cast<std::size_t>(result);
        }
    }

    void handle(const SOCKET client) const noexcept {
        std::string request;
        std::array<char, 2048> buffer{};
        while (request.find("\r\n\r\n") == std::string::npos && request.size() < 8192U) {
            const int received = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
            if (received <= 0) {
                return;
            }
            request.append(buffer.data(), static_cast<std::size_t>(received));
        }

        const auto reply = route(requestPath(request));
        std::string headers = "HTTP/1.1 " + std::to_string(reply.status) + " " + reply.reason +
                              "\r\nContent-Type: " + reply.contentType +
                              "\r\nContent-Length: " + std::to_string(reply.body.size()) +
                              "\r\nConnection: close\r\n";
        if (!reply.location.empty()) {
            headers += "Location: " + reply.location + "\r\n";
        }
        headers += "\r\n";
        sendAll(client, headers);
        sendAll(client, reply.body);
    }

    SOCKET socket_{INVALID_SOCKET};
    std::uint16_t port_{0};
    std::atomic_bool stopping_{false};
    std::thread thread_;
};

} // namespace

TEST_CASE("WinHTTP follows safe local redirects and enforces response limits") {
    LocalHttpServer server;
    saors::WinHttpClient client;

    const auto redirected = client.get(server.url("/redirect"), {});
    REQUIRE(redirected);
    CHECK(redirected.value.statusCode == 200);
    CHECK(redirected.value.finalUrl == server.url("/list.m3u"));
    CHECK(redirected.value.redirectCount == 1);

    saors::HttpRequestOptions limited;
    limited.maximumResponseBytes = 1024;
    const auto tooLarge = client.get(server.url("/too-large.m3u"), limited);
    CHECK_FALSE(tooLarge);
    CHECK(tooLarge.error == "HTTP response exceeds the configured size limit");
}

TEST_CASE("Playlist resolver works against a controlled local HTTP server") {
    LocalHttpServer server;
    auto client = std::make_shared<saors::WinHttpClient>();
    saors::PlaylistResolver resolver(client);

    CHECK(resolver.resolve(server.url("/direct-stream")));
    CHECK(resolver.resolve(server.url("/list.m3u")));
    CHECK(resolver.resolve(server.url("/list.pls")));
    CHECK(resolver.resolve(server.url("/relative/list.m3u")));
    CHECK(resolver.resolve(server.url("/nested-a.m3u")));
    CHECK(resolver.resolve(server.url("/redirect")));

    const auto cycle = resolver.resolve(server.url("/cycle-a.m3u"));
    CHECK_FALSE(cycle);
    CHECK(cycle.error == "playlist cycle detected");

    const auto hls = resolver.resolve(server.url("/hls.m3u8"));
    CHECK_FALSE(hls);
    CHECK(hls.error == "HLS playlist detected but HLS resolution is not implemented");

    saors::ResolveOptions limited;
    limited.maximumPlaylistBytes = 1024;
    const auto tooLarge = resolver.resolve(server.url("/too-large.m3u"), limited);
    CHECK_FALSE(tooLarge);
    CHECK(tooLarge.error == "HTTP response exceeds the configured size limit");
}
