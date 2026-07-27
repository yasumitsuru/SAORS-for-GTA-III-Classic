#include "saors_gta3/FileHasher.hpp"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace saors {
namespace {

bool ntSuccess(const NTSTATUS status) noexcept {
    return status >= 0;
}

std::string windowsError(const char* operation, const DWORD code) {
    return std::string(operation) + " (Windows error " + std::to_string(code) + ")";
}

std::string cngError(const char* operation, const NTSTATUS status) {
    return std::string(operation) + " (CNG status " +
           std::to_string(static_cast<long>(status)) + ")";
}

class FileHandle {
  public:
    explicit FileHandle(const HANDLE value) noexcept : value_(value) {}
    ~FileHandle() {
        if (value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept {
        return value_;
    }

  private:
    HANDLE value_{INVALID_HANDLE_VALUE};
};

class AlgorithmHandle {
  public:
    ~AlgorithmHandle() {
        if (value != nullptr) {
            BCryptCloseAlgorithmProvider(value, 0);
        }
    }

    AlgorithmHandle(const AlgorithmHandle&) = delete;
    AlgorithmHandle& operator=(const AlgorithmHandle&) = delete;
    AlgorithmHandle() = default;

    BCRYPT_ALG_HANDLE value{nullptr};
};

class HashHandle {
  public:
    ~HashHandle() {
        if (value != nullptr) {
            BCryptDestroyHash(value);
        }
    }

    HashHandle(const HashHandle&) = delete;
    HashHandle& operator=(const HashHandle&) = delete;
    HashHandle() = default;

    BCRYPT_HASH_HANDLE value{nullptr};
};

class SecureBytes {
  public:
    explicit SecureBytes(const std::size_t size) : bytes(size) {}
    ~SecureBytes() {
        if (!bytes.empty()) {
            SecureZeroMemory(bytes.data(), bytes.size());
        }
    }

    SecureBytes(const SecureBytes&) = delete;
    SecureBytes& operator=(const SecureBytes&) = delete;

    std::vector<UCHAR> bytes;
};

Result<ULONG> readDwordProperty(const BCRYPT_ALG_HANDLE algorithm, const wchar_t* property) {
    ULONG value = 0;
    ULONG copied = 0;
    const auto status = BCryptGetProperty(
        algorithm, property, reinterpret_cast<PUCHAR>(&value), sizeof(value), &copied, 0);
    if (!ntSuccess(status) || copied != sizeof(value)) {
        return Result<ULONG>::fail(cngError("unable to read a SHA-256 provider property",
                                           status));
    }
    return Result<ULONG>::ok(value);
}

std::string lowercaseHex(const std::vector<UCHAR>& bytes) {
    constexpr std::array<char, 16> digits{'0', '1', '2', '3', '4', '5', '6', '7',
                                          '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::string result;
    result.resize(bytes.size() * 2U);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        result[index * 2U] = digits[(bytes[index] >> 4U) & 0x0FU];
        result[index * 2U + 1U] = digits[bytes[index] & 0x0FU];
    }
    return result;
}

class WindowsFileHasher final : public FileHasher {
  public:
    Result<std::string> sha256File(const std::filesystem::path& path) const override {
        const auto widePath = path.wstring();
        FileHandle file(CreateFileW(widePath.c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
        if (file.get() == INVALID_HANDLE_VALUE) {
            return Result<std::string>::fail(
                windowsError("unable to open file for hashing", GetLastError()));
        }

        LARGE_INTEGER size{};
        if (GetFileSizeEx(file.get(), &size) == FALSE || size.QuadPart < 0) {
            return Result<std::string>::fail(
                windowsError("unable to determine file size for hashing", GetLastError()));
        }
        return hashOpenFile(file.get(), 0, static_cast<std::uint64_t>(size.QuadPart));
    }

    Result<std::string> sha256Range(const std::filesystem::path& path,
                                    const std::uint64_t offset,
                                    const std::uint64_t length) const override {
        const auto widePath = path.wstring();
        FileHandle file(CreateFileW(widePath.c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
        if (file.get() == INVALID_HANDLE_VALUE) {
            return Result<std::string>::fail(
                windowsError("unable to open file for hashing", GetLastError()));
        }

        LARGE_INTEGER size{};
        if (GetFileSizeEx(file.get(), &size) == FALSE || size.QuadPart < 0) {
            return Result<std::string>::fail(
                windowsError("unable to determine file size for hashing", GetLastError()));
        }
        const auto fileSize = static_cast<std::uint64_t>(size.QuadPart);
        if (offset > fileSize || length > fileSize - offset) {
            return Result<std::string>::fail("hash range is outside the file");
        }
        return hashOpenFile(file.get(), offset, length);
    }

  private:
    static Result<std::string> hashOpenFile(const HANDLE file, const std::uint64_t offset,
                                            const std::uint64_t length) {
        if (offset > static_cast<std::uint64_t>(std::numeric_limits<LONGLONG>::max())) {
            return Result<std::string>::fail("hash range offset is too large");
        }
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<LONGLONG>(offset);
        if (SetFilePointerEx(file, position, nullptr, FILE_BEGIN) == FALSE) {
            return Result<std::string>::fail(
                windowsError("unable to seek to hash range", GetLastError()));
        }

        AlgorithmHandle algorithm;
        auto status =
            BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        if (!ntSuccess(status)) {
            return Result<std::string>::fail(
                cngError("unable to open the Windows SHA-256 provider", status));
        }

        const auto objectLength = readDwordProperty(algorithm.value, BCRYPT_OBJECT_LENGTH);
        if (!objectLength) {
            return Result<std::string>::fail(objectLength.error);
        }
        const auto digestLength = readDwordProperty(algorithm.value, BCRYPT_HASH_LENGTH);
        if (!digestLength || digestLength.value != 32U) {
            return Result<std::string>::fail(
                digestLength ? "Windows SHA-256 provider returned an unexpected digest length"
                             : digestLength.error);
        }

        SecureBytes hashObject(objectLength.value);
        HashHandle hash;
        status = BCryptCreateHash(algorithm.value, &hash.value, hashObject.bytes.data(),
                                  objectLength.value, nullptr, 0, 0);
        if (!ntSuccess(status)) {
            return Result<std::string>::fail(
                cngError("unable to initialize SHA-256 hashing", status));
        }

        std::array<UCHAR, 64U * 1024U> buffer{};
        std::uint64_t remaining = length;
        while (remaining > 0U) {
            const auto requested = static_cast<DWORD>(
                std::min<std::uint64_t>(remaining, static_cast<std::uint64_t>(buffer.size())));
            DWORD received = 0;
            if (ReadFile(file, buffer.data(), requested, &received, nullptr) == FALSE) {
                SecureZeroMemory(buffer.data(), buffer.size());
                return Result<std::string>::fail(
                    windowsError("unable to read file while hashing", GetLastError()));
            }
            if (received == 0U) {
                SecureZeroMemory(buffer.data(), buffer.size());
                return Result<std::string>::fail(
                    "file ended before the requested hash range was complete");
            }
            status = BCryptHashData(hash.value, buffer.data(), received, 0);
            if (!ntSuccess(status)) {
                SecureZeroMemory(buffer.data(), buffer.size());
                return Result<std::string>::fail(
                    cngError("unable to update SHA-256 hashing", status));
            }
            remaining -= received;
        }
        SecureZeroMemory(buffer.data(), buffer.size());

        SecureBytes digest(digestLength.value);
        status = BCryptFinishHash(hash.value, digest.bytes.data(), digestLength.value, 0);
        if (!ntSuccess(status)) {
            return Result<std::string>::fail(
                cngError("unable to finish SHA-256 hashing", status));
        }
        return Result<std::string>::ok(lowercaseHex(digest.bytes));
    }
};

} // namespace

std::unique_ptr<FileHasher> createPlatformFileHasher() {
    return std::make_unique<WindowsFileHasher>();
}

} // namespace saors
