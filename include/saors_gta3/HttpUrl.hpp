#pragma once

#include "saors_gta3/Result.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace saors {

struct HttpUrl {
    std::string scheme;
    std::string host;
    std::uint16_t port{0};
    bool explicitPort{false};
    bool ipv6Literal{false};
    std::string path;
    std::string query;
    std::string normalized;

    [[nodiscard]] bool secure() const noexcept {
        return scheme == "https";
    }

    [[nodiscard]] std::string pathAndQuery() const;
};

[[nodiscard]] Result<HttpUrl> parseHttpUrl(std::string_view value);
[[nodiscard]] Result<std::string> resolveHttpUrl(std::string_view baseUrl,
                                                 std::string_view reference);
[[nodiscard]] bool hasHttpUrlControlCharacters(std::string_view value) noexcept;

} // namespace saors
