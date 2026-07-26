#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace saors {

enum class PlaylistKind {
    directUrl,
    m3u,
    pls,
};

struct PlaylistParseResult {
    PlaylistKind kind{PlaylistKind::m3u};
    bool success{false};
    std::vector<std::string> urls;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

class PlaylistParser {
  public:
    [[nodiscard]] static PlaylistParseResult parse(std::string_view content,
                                                   std::string_view sourceName = {});
    [[nodiscard]] static bool isSupportedUrl(std::string_view value);
};

} // namespace saors
