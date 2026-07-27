#include "saors_gta3/UrlSanitizer.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("URL sanitizer redacts credentials while preserving the host") {
    const auto sanitized = saors::sanitizeUrlForLogging("https://alice:secret@radio.example/live");

    CHECK(sanitized == "https://[redacted]@radio.example/live");
    CHECK(sanitized.find("secret") == std::string::npos);
}

TEST_CASE("URL sanitizer redacts sensitive query values case-insensitively") {
    const auto sanitized = saors::sanitizeUrlForLogging(
        "https://radio.example/live?token=one&mode=audio&KEY=two&Auth=three#now");

    CHECK(sanitized == "https://radio.example/live?token=[redacted]&mode=audio&KEY=[redacted]&"
                       "Auth=[redacted]#now");
}

TEST_CASE("Text sanitizer protects every embedded URL") {
    const auto sanitized = saors::sanitizeTextForLogging(
        "first=https://u:p@one.example/live second http://two.example/?auth=hidden");

    CHECK(sanitized.find("u:p") == std::string::npos);
    CHECK(sanitized.find("hidden") == std::string::npos);
    CHECK(sanitized.find("https://[redacted]@one.example/live") != std::string::npos);
    CHECK(sanitized.find("http://two.example/?auth=[redacted]") != std::string::npos);
}

TEST_CASE("URL sanitizer leaves ordinary query parameters intact") {
    CHECK(saors::sanitizeUrlForLogging("https://radio.example/live?format=mp3&quality=high") ==
          "https://radio.example/live?format=mp3&quality=high");
}
