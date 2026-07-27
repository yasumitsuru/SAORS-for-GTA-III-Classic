#pragma once

#include "saors_gta3/Result.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace saors {

class FileHasher {
  public:
    virtual ~FileHasher() = default;

    [[nodiscard]] virtual Result<std::string>
    sha256File(const std::filesystem::path& path) const = 0;

    [[nodiscard]] virtual Result<std::string>
    sha256Range(const std::filesystem::path& path, std::uint64_t offset,
                std::uint64_t length) const = 0;
};

[[nodiscard]] std::unique_ptr<FileHasher> createPlatformFileHasher();

} // namespace saors
