#pragma once

#include <string>
#include <string_view>

namespace saors {

[[nodiscard]] std::string sanitizeUrlForLogging(std::string_view url);
[[nodiscard]] std::string sanitizeTextForLogging(std::string_view text);

} // namespace saors
