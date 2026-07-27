#pragma once

#include "saors_gta3/HttpClient.hpp"

namespace saors {

class WinHttpClient final : public HttpClient {
  public:
    [[nodiscard]] Result<HttpResponse> get(const std::string& url,
                                           const HttpRequestOptions& options) noexcept override;
};

} // namespace saors
