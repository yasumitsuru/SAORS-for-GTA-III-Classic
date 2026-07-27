#pragma once

#include <string>
#include <utility>

namespace saors {

template <typename T> struct Result {
    T value{};
    bool success{false};
    std::string error;

    [[nodiscard]] static Result ok(T resultValue) {
        Result result;
        result.value = std::move(resultValue);
        result.success = true;
        return result;
    }

    [[nodiscard]] static Result fail(std::string message) {
        Result result;
        result.error = std::move(message);
        return result;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

} // namespace saors
