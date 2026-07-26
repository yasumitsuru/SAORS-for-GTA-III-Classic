#include "saors_gta3/PlaylistParser.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("Direct HTTP stream URL is recognized") {
    const auto result = saors::PlaylistParser::parse("http://radio.example.com/live.mp3");

    REQUIRE(result.success);
    CHECK(result.kind == saors::PlaylistKind::directUrl);
    REQUIRE(result.urls.size() == 1);
    CHECK(result.urls.front() == "http://radio.example.com/live.mp3");
}

TEST_CASE("Direct HTTPS stream URL is recognized") {
    const auto result =
        saors::PlaylistParser::parse("https://radio.example.com:8443/live?token=public");

    REQUIRE(result.success);
    CHECK(result.kind == saors::PlaylistKind::directUrl);
    CHECK(result.urls.front().find("https://") == 0);
}

TEST_CASE("M3U with EXTM3U header ignores metadata") {
    const auto result = saors::PlaylistParser::parse(R"m3u(
#EXTM3U
#EXTINF:-1,Example Radio
https://radio.example.com/live
)m3u",
                                                     "stations.m3u");

    REQUIRE(result.success);
    CHECK(result.kind == saors::PlaylistKind::m3u);
    REQUIRE(result.urls.size() == 1);
    CHECK(result.urls.front() == "https://radio.example.com/live");
}

TEST_CASE("M3U without header accepts multiple URLs in order") {
    const auto result = saors::PlaylistParser::parse(R"m3u(
http://one.example.com/stream

# a comment
https://two.example.com/stream
)m3u");

    REQUIRE(result.success);
    REQUIRE(result.urls.size() == 2);
    CHECK(result.urls.at(0) == "http://one.example.com/stream");
    CHECK(result.urls.at(1) == "https://two.example.com/stream");
}

TEST_CASE("M3U8 is parsed as a text playlist when content contains URLs") {
    const auto result = saors::PlaylistParser::parse(R"m3u8(
#EXTM3U
#EXT-X-STREAM-INF:BANDWIDTH=128000
https://cdn.example.com/audio/playlist.m3u8
)m3u8",
                                                     "master.m3u8");

    REQUIRE(result.success);
    CHECK(result.kind == saors::PlaylistKind::m3u);
    REQUIRE(result.urls.size() == 1);
}

TEST_CASE("PLS File entries are ordered by numeric suffix") {
    const auto result = saors::PlaylistParser::parse(R"pls(
[playlist]
NumberOfEntries=2
File2=https://two.example.com/live
Title2=Second
File1=http://one.example.com/live
Version=2
)pls",
                                                     "stations.pls");

    REQUIRE(result.success);
    CHECK(result.kind == saors::PlaylistKind::pls);
    REQUIRE(result.urls.size() == 2);
    CHECK(result.urls.at(0) == "http://one.example.com/live");
    CHECK(result.urls.at(1) == "https://two.example.com/live");
}

TEST_CASE("Invalid playlist gives a comprehensible error") {
    const auto result = saors::PlaylistParser::parse(
        "#EXTM3U\nrelative/path.mp3\nftp://example.com/stream\n", "invalid.m3u");

    CHECK_FALSE(result.success);
    CHECK(result.urls.empty());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors.front().find("HTTP(S)") != std::string::npos);
    CHECK(result.warnings.size() == 2);
}

TEST_CASE("PLS without File URL is rejected") {
    const auto result = saors::PlaylistParser::parse(
        "[playlist]\nTitle1=Missing URL\nNumberOfEntries=1\n", "missing.pls");

    CHECK_FALSE(result.success);
    CHECK(result.kind == saors::PlaylistKind::pls);
    CHECK(result.urls.empty());
    REQUIRE_FALSE(result.errors.empty());
}

TEST_CASE("Empty playlist is rejected") {
    const auto result = saors::PlaylistParser::parse(" \r\n ");

    CHECK_FALSE(result.success);
    CHECK(result.urls.empty());
    REQUIRE(result.errors.size() == 1);
    CHECK(result.errors.front() == "playlist is empty");
}

TEST_CASE("Unsupported URL schemes and missing hosts are rejected") {
    CHECK_FALSE(saors::PlaylistParser::isSupportedUrl("ftp://example.com/live"));
    CHECK_FALSE(saors::PlaylistParser::isSupportedUrl("https://"));
    CHECK_FALSE(saors::PlaylistParser::isSupportedUrl("relative/live.mp3"));
    CHECK_FALSE(saors::PlaylistParser::isSupportedUrl(""));
}
