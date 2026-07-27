#include "saors_gta3/FileHasher.hpp"

#include <catch2/catch_test_macros.hpp>

#if defined(_WIN32)

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

class TemporaryData {
  public:
    explicit TemporaryData(const std::vector<std::uint8_t>& bytes,
                           const bool unicodeName = false) {
        static std::atomic_uint64_t counter{0};
        const auto id = counter.fetch_add(1U, std::memory_order_relaxed);
        const auto name =
            unicodeName
                ? std::filesystem::path(std::wstring(L"saors-hash-\u00E7-") +
                                        std::to_wstring(id) + L".bin")
                : std::filesystem::path(L"saors-hash-" + std::to_wstring(id) + L".bin");
        path_ = std::filesystem::temp_directory_path() / name;
        std::ofstream file(path_, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }

    ~TemporaryData() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("Windows CNG hasher returns the standard SHA-256 digest") {
    const TemporaryData file(
        {static_cast<std::uint8_t>('a'), static_cast<std::uint8_t>('b'),
         static_cast<std::uint8_t>('c')});
    const auto hasher = saors::createPlatformFileHasher();
    REQUIRE(hasher);

    const auto result = hasher->sha256File(file.path());
    REQUIRE(result);
    CHECK(result.value ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE("Windows CNG hasher processes large files incrementally and deterministically") {
    std::vector<std::uint8_t> bytes(1024U * 1024U);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>((index * 17U) & 0xFFU);
    }
    const TemporaryData file(bytes, true);
    const auto hasher = saors::createPlatformFileHasher();
    REQUIRE(hasher);

    const auto first = hasher->sha256File(file.path());
    const auto second = hasher->sha256File(file.path());
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first.value == second.value);
    CHECK(first.value.size() == 64U);
}

TEST_CASE("Windows CNG range hashing equals the same standalone bytes") {
    const TemporaryData source(
        {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U});
    const TemporaryData expected({2U, 3U, 4U, 5U, 6U});
    const auto hasher = saors::createPlatformFileHasher();
    REQUIRE(hasher);

    const auto range = hasher->sha256Range(source.path(), 2U, 5U);
    const auto whole = hasher->sha256File(expected.path());
    REQUIRE(range);
    REQUIRE(whole);
    CHECK(range.value == whole.value);
}

TEST_CASE("Windows CNG hasher reports missing files and invalid ranges") {
    const auto hasher = saors::createPlatformFileHasher();
    REQUIRE(hasher);
    CHECK_FALSE(hasher->sha256File(
        std::filesystem::temp_directory_path() / "saors-definitely-missing.bin"));

    const TemporaryData file({1U, 2U, 3U});
    const auto range = hasher->sha256Range(file.path(), 2U, 2U);
    CHECK_FALSE(range);
    CHECK(range.error == "hash range is outside the file");
}

#endif
