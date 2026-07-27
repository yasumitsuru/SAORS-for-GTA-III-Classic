#pragma once

#include "saors_gta3/HttpClient.hpp"
#include "saors_gta3/ResolvedStream.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace saors {

struct ResolveOptions {
    bool allowHttpStreams{false};
    std::uint32_t connectTimeoutMilliseconds{5000};
    std::uint32_t receiveTimeoutMilliseconds{10000};
    std::size_t maximumPlaylistBytes{256U * 1024U};
    std::size_t maximumEntries{128};
    std::size_t maximumDepth{3};
    std::size_t maximumRedirects{5};
    const std::atomic_bool* cancelRequested{nullptr};
};

class PlaylistResolver {
  public:
    explicit PlaylistResolver(std::shared_ptr<HttpClient> httpClient);

    [[nodiscard]] Result<ResolvedStream> resolve(const std::string& configuredUrl,
                                                 const ResolveOptions& options = {}) noexcept;

  private:
    std::shared_ptr<HttpClient> httpClient_;
};

} // namespace saors
