#include "saors_gta3/ExecutableReport.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace saors {
namespace {

std::string pathForReport(const std::filesystem::path& path, const bool redactPath) {
    return redactPath ? "[redacted]" : path.u8string();
}

std::string jsonEscape(const std::string& value) {
    std::ostringstream output;
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (byte < 0x20U) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned int>(byte) << std::dec;
            } else {
                output << character;
            }
            break;
        }
    }
    return output.str();
}

std::string hexadecimal(const std::uint64_t value, const int width) {
    std::ostringstream output;
    output << "0x" << std::hex << std::uppercase << std::setw(width) << std::setfill('0')
           << value;
    return output.str();
}

} // namespace

std::string formatExecutableTextReport(const std::filesystem::path& path,
                                       const ExecutableFingerprint& fingerprint,
                                       const ExecutableReportContext& context,
                                       const bool redactPath) {
    std::ostringstream output;
    output << "SAORS executable probe\n"
           << "File path: " << pathForReport(path, redactPath) << '\n'
           << "File type: PE32\n"
           << "Machine: Intel 386\n"
           << "File SHA-256: " << fingerprint.fileSha256 << '\n'
           << ".text SHA-256: " << fingerprint.textSectionSha256 << '\n'
           << "PE timestamp: " << hexadecimal(fingerprint.timeDateStamp, 8) << '\n'
           << "Entry point RVA: " << hexadecimal(fingerprint.entryPointRva, 8) << '\n'
           << "Size of image: " << hexadecimal(fingerprint.sizeOfImage, 8) << '\n'
           << "Checksum: " << hexadecimal(fingerprint.checksum, 8) << '\n'
           << "File size: " << fingerprint.fileSize << '\n'
           << "Profile match: " << context.profileName << '\n'
           << "Match type: " << context.matchDescription << '\n'
           << "Verification status: " << context.verificationStatus << '\n'
           << "Status: " << (context.supported ? "recognized executable"
                                               : "unsupported executable")
           << '\n';
    return output.str();
}

std::string formatExecutableJsonReport(const std::filesystem::path& path,
                                       const ExecutableFingerprint& fingerprint,
                                       const ExecutableReportContext& context,
                                       const bool redactPath) {
    std::ostringstream output;
    output << "{\n"
           << "  \"schema\": \"saors-executable-profile-v1\",\n"
           << "  \"file_path\": \"" << jsonEscape(pathForReport(path, redactPath)) << "\",\n"
           << "  \"file_type\": \"PE32\",\n"
           << "  \"machine\": " << fingerprint.machine << ",\n"
           << "  \"optional_header_magic\": " << fingerprint.optionalHeaderMagic << ",\n"
           << "  \"file_sha256\": \"" << jsonEscape(fingerprint.fileSha256) << "\",\n"
           << "  \"text_sha256\": \"" << jsonEscape(fingerprint.textSectionSha256) << "\",\n"
           << "  \"time_date_stamp\": " << fingerprint.timeDateStamp << ",\n"
           << "  \"entry_point_rva\": " << fingerprint.entryPointRva << ",\n"
           << "  \"size_of_image\": " << fingerprint.sizeOfImage << ",\n"
           << "  \"checksum\": " << fingerprint.checksum << ",\n"
           << "  \"file_size\": " << fingerprint.fileSize << ",\n"
           << "  \"profile\": \"" << jsonEscape(context.profileName) << "\",\n"
           << "  \"match\": \"" << jsonEscape(context.matchDescription) << "\",\n"
           << "  \"verification_status\": \"" << jsonEscape(context.verificationStatus)
           << "\",\n"
           << "  \"supported\": " << (context.supported ? "true" : "false") << ",\n"
           << "  \"sections\": [\n";

    for (std::size_t index = 0; index < fingerprint.sections.size(); ++index) {
        const auto& section = fingerprint.sections[index];
        output << "    {\n"
               << "      \"name\": \"" << jsonEscape(section.name) << "\",\n"
               << "      \"virtual_address\": " << section.virtualAddress << ",\n"
               << "      \"virtual_size\": " << section.virtualSize << ",\n"
               << "      \"raw_offset\": " << section.rawOffset << ",\n"
               << "      \"raw_size\": " << section.rawSize << ",\n"
               << "      \"sha256\": \"" << jsonEscape(section.sha256) << "\"\n"
               << "    }" << (index + 1U == fingerprint.sections.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
    return output.str();
}

} // namespace saors
