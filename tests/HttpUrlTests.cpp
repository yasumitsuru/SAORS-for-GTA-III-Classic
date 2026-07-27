#include "saors_gta3/HttpUrl.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("HTTP URLs are parsed and normalized") {
    const auto result =
        saors::parseHttpUrl("HTTPS://Example.COM:443/radio/../live%20audio.aac?token=public#now");

    REQUIRE(result);
    CHECK(result.value.scheme == "https");
    CHECK(result.value.host == "Example.COM");
    CHECK(result.value.port == 443);
    CHECK(result.value.explicitPort);
    CHECK(result.value.path == "/radio/../live%20audio.aac");
    CHECK(result.value.query == "token=public");
    CHECK(result.value.normalized == "https://example.com/live%20audio.aac?token=public");
}

TEST_CASE("HTTP URL parser supports IPv4 IPv6 and non-default ports") {
    const auto ipv4 = saors::parseHttpUrl("http://127.0.0.1:8080/live");
    const auto ipv6 = saors::parseHttpUrl("https://[2001:db8::1]:8443/live");

    REQUIRE(ipv4);
    CHECK(ipv4.value.normalized == "http://127.0.0.1:8080/live");
    REQUIRE(ipv6);
    CHECK(ipv6.value.ipv6Literal);
    CHECK(ipv6.value.host == "2001:db8::1");
    CHECK(ipv6.value.normalized == "https://[2001:db8::1]:8443/live");
}

TEST_CASE("Relative HTTP references use the final playlist URL as base") {
    CHECK(saors::resolveHttpUrl("https://radio.example.com/lists/current/index.m3u",
                                "../audio/live.aac")
              .value == "https://radio.example.com/lists/audio/live.aac");
    CHECK(saors::resolveHttpUrl("https://radio.example.com/lists/index.m3u", "/stream").value ==
          "https://radio.example.com/stream");
    CHECK(saors::resolveHttpUrl("https://radio.example.com/lists/index.m3u?old=1", "?token=public")
              .value == "https://radio.example.com/lists/index.m3u?token=public");
    CHECK(
        saors::resolveHttpUrl("https://old.example.com/list.m3u", "//new.example.com/live").value ==
        "https://new.example.com/live");
}

TEST_CASE("Unsafe or unsupported HTTP URL forms are rejected") {
    CHECK_FALSE(saors::parseHttpUrl("file:///C:/radio/live.mp3"));
    CHECK_FALSE(saors::parseHttpUrl("ftp://example.com/live"));
    CHECK_FALSE(saors::parseHttpUrl("data:text/plain,live"));
    CHECK_FALSE(saors::parseHttpUrl(R"(\\server\share\live.mp3)"));
    CHECK_FALSE(saors::parseHttpUrl("https://user:password@example.com/live"));
    CHECK_FALSE(saors::parseHttpUrl("https://example.com/bad%0Aheader"));
    CHECK_FALSE(saors::parseHttpUrl("https://example.com/bad%escape"));
    CHECK_FALSE(saors::parseHttpUrl("https://example.com/white space"));
    CHECK_FALSE(saors::parseHttpUrl("https:///missing-host"));
}

TEST_CASE("Relative HTTP references reject local and control-character forms") {
    CHECK_FALSE(saors::resolveHttpUrl("https://example.com/list.m3u", R"(C:\radio\live.mp3)"));
    CHECK_FALSE(saors::resolveHttpUrl("https://example.com/list.m3u",
                                      "//example.com/live\r\nHeader: value"));
    CHECK_FALSE(saors::resolveHttpUrl("https://example.com/list.m3u", "file:///tmp/live"));
}
