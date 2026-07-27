#pragma once

#include "saors_gta3/Result.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace saors {

struct HttpResponse {
    int statusCode{0};
    std::string finalUrl;
    std::string contentType;
    std::string body;
    std::size_t redirectCount{0};
};

struct HttpRequestOptions {
    std::uint32_t connectTimeoutMilliseconds{5000};
    std::uint32_t receiveTimeoutMilliseconds{10000};
    std::size_t maximumResponseBytes{256U * 1024U};
    std::size_t maximumRedirects{5};
    bool readBody{true};
    const std::atomic_bool* cancelRequested{nullptr};

    [[nodiscard]] bool cancelled() const noexcept {
        return cancelRequested != nullptr && cancelRequested->load(std::memory_order_relaxed);
    }
};

class HttpClient {
  public:
    virtual ~HttpClient() = default;

    [[nodiscard]] virtual Result<HttpResponse> get(const std::string& url,
                                                   const HttpRequestOptions& options) noexcept = 0;
};

} // namespace saors
