#pragma once

#include <cstddef>
#include <string>

namespace saors {

struct ResolvedStream {
    std::string configuredUrl;
    std::string finalPlaylistUrl;
    std::string mediaUrl;
    std::string playlistType;
    std::string contentType;
    std::size_t selectedEntryIndex{0};
    std::size_t playlistEntries{0};
};

} // namespace saors
