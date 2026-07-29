#include "saors_gta3/RadioStationEvidence.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace saors {
namespace {

struct JsonValue {
    enum class Type { nullValue, boolean, number, string, object, array };
    Type type{Type::nullValue};
    bool booleanValue{false};
    long long numberValue{0};
    std::string stringValue;
    std::map<std::string, JsonValue> objectValue;
    std::vector<JsonValue> arrayValue;
};

class JsonParser {
  public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    bool parse(JsonValue& value) {
        skipWhitespace();
        if (!parseValue(value)) {
            return false;
        }
        skipWhitespace();
        return position_ == input_.size();
    }

  private:
    void skipWhitespace() noexcept {
        while (position_ < input_.size() &&
               std::isspace(static_cast<unsigned char>(input_[position_])) != 0) {
            ++position_;
        }
    }

    bool consume(const char expected) noexcept {
        skipWhitespace();
        if (position_ >= input_.size() || input_[position_] != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    bool parseValue(JsonValue& value) {
        skipWhitespace();
        if (position_ >= input_.size()) {
            return false;
        }
        switch (input_[position_]) {
        case '{':
            return parseObject(value);
        case '[':
            return parseArray(value);
        case '"':
            value.type = JsonValue::Type::string;
            return parseString(value.stringValue);
        case 't':
            return parseLiteral("true", JsonValue::Type::boolean, value, true);
        case 'f':
            return parseLiteral("false", JsonValue::Type::boolean, value, false);
        case 'n':
            return parseNull(value);
        default:
            return parseNumber(value);
        }
    }

    bool parseObject(JsonValue& value) {
        if (!consume('{')) {
            return false;
        }
        value.type = JsonValue::Type::object;
        skipWhitespace();
        if (position_ < input_.size() && input_[position_] == '}') {
            ++position_;
            return true;
        }
        for (;;) {
            std::string key;
            if (!parseString(key) || !consume(':')) {
                return false;
            }
            JsonValue child;
            if (!parseValue(child) || !value.objectValue.emplace(std::move(key), std::move(child)).second) {
                return false;
            }
            skipWhitespace();
            if (position_ < input_.size() && input_[position_] == '}') {
                ++position_;
                return true;
            }
            if (!consume(',')) {
                return false;
            }
        }
    }

    bool parseArray(JsonValue& value) {
        if (!consume('[')) {
            return false;
        }
        value.type = JsonValue::Type::array;
        skipWhitespace();
        if (position_ < input_.size() && input_[position_] == ']') {
            ++position_;
            return true;
        }
        for (;;) {
            JsonValue child;
            if (!parseValue(child)) {
                return false;
            }
            value.arrayValue.push_back(std::move(child));
            skipWhitespace();
            if (position_ < input_.size() && input_[position_] == ']') {
                ++position_;
                return true;
            }
            if (!consume(',')) {
                return false;
            }
        }
    }

    bool parseString(std::string& value) {
        if (!consume('"')) {
            return false;
        }
        value.clear();
        while (position_ < input_.size()) {
            const char character = input_[position_++];
            if (character == '"') {
                return true;
            }
            if (character == '\\') {
                if (position_ >= input_.size()) {
                    return false;
                }
                const char escaped = input_[position_++];
                switch (escaped) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: return false;
                }
            } else if (static_cast<unsigned char>(character) < 0x20U) {
                return false;
            } else {
                value.push_back(character);
            }
        }
        return false;
    }

    bool parseLiteral(std::string_view literal, const JsonValue::Type type,
                      JsonValue& value, const bool booleanValue) {
        if (input_.substr(position_, literal.size()) != literal) {
            return false;
        }
        position_ += literal.size();
        value.type = type;
        value.booleanValue = booleanValue;
        return true;
    }

    bool parseNull(JsonValue& value) {
        if (input_.substr(position_, 4U) != "null") {
            return false;
        }
        position_ += 4U;
        value.type = JsonValue::Type::nullValue;
        return true;
    }

    bool parseNumber(JsonValue& value) {
        skipWhitespace();
        const auto start = position_;
        if (position_ < input_.size() && input_[position_] == '-') {
            ++position_;
        }
        if (position_ >= input_.size() ||
            std::isdigit(static_cast<unsigned char>(input_[position_])) == 0) {
            return false;
        }
        while (position_ < input_.size() &&
               std::isdigit(static_cast<unsigned char>(input_[position_])) != 0) {
            ++position_;
        }
        const auto text = input_.substr(start, position_ - start);
        try {
            std::size_t consumed = 0;
            value.numberValue = std::stoll(std::string(text), &consumed, 10);
            if (consumed != text.size()) {
                return false;
            }
        } catch (...) {
            return false;
        }
        value.type = JsonValue::Type::number;
        return true;
    }

    std::string_view input_;
    std::size_t position_{0};
};

const JsonValue* member(const JsonValue& value, const char* key) noexcept {
    if (value.type != JsonValue::Type::object) {
        return nullptr;
    }
    const auto found = value.objectValue.find(key);
    return found == value.objectValue.end() ? nullptr : &found->second;
}

bool readString(const JsonValue& value, std::string& output) {
    if (value.type != JsonValue::Type::string) {
        return false;
    }
    output = value.stringValue;
    return true;
}

bool readNumber(const JsonValue& value, long long& output) {
    if (value.type != JsonValue::Type::number) {
        return false;
    }
    output = value.numberValue;
    return true;
}

bool readBoolean(const JsonValue& value, bool& output) {
    if (value.type != JsonValue::Type::boolean) {
        return false;
    }
    output = value.booleanValue;
    return true;
}

std::string escapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 2U);
    for (const char character : value) {
        switch (character) {
        case '"': escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped.push_back(character); break;
        }
    }
    return escaped;
}

bool hasSensitiveField(const JsonValue& value) {
    if (value.type == JsonValue::Type::object) {
        for (const auto& entry : value.objectValue) {
            auto key = entry.first;
            std::transform(key.begin(), key.end(), key.begin(), [](const unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
            if (key == "url" || key == "uri" || key == "path" || key == "filepath" ||
                key == "executablepath" || key == "filename" || key == "hostname" ||
                key == "host" || key == "sha256" || key == "hash" || key == "address" ||
                key == "pointer" || key == "bytes" || key == "audio" || key == "dump" ||
                key == "username" || key == "user" || key == "computer" || key == "machine" ||
                key == "serial") {
                return true;
            }
            if (hasSensitiveField(entry.second)) {
                return true;
            }
        }
    } else if (value.type == JsonValue::Type::array) {
        for (const auto& child : value.arrayValue) {
            if (hasSensitiveField(child)) {
                return true;
            }
        }
    }
    return false;
}

bool isSafeIdentifier(std::string_view value) noexcept {
    if (value.empty() || value.size() > 64U) {
        return false;
    }
    for (const char rawCharacter : value) {
        const auto character = static_cast<unsigned char>(rawCharacter);
        if (!(std::isalnum(character) != 0 || character == '_' || character == '-' || character == '.')) {
            return false;
        }
    }
    return true;
}

bool isSafeVisibleLabel(std::string_view value) noexcept {
    if (value.empty() || value.size() > 128U) {
        return false;
    }
    for (const char rawCharacter : value) {
        const auto character = static_cast<unsigned char>(rawCharacter);
        if (character < 0x20U || character == 0x7FU || character == '/' || character == '\\') {
            return false;
        }
    }
    return value.find("://") == std::string_view::npos;
}

bool parseObservation(const JsonValue& value, const RadioStationEvidenceDocument& document,
                      RadioStationObservation& observation, std::vector<std::string>& errors,
                      std::size_t index) {
    if (value.type != JsonValue::Type::object) {
        errors.push_back("observations[" + std::to_string(index) + "] must be an object");
        return false;
    }
    observation.sessionId = document.sessionId;
    observation.executableProfile = document.executableProfile;
    auto readRequiredNumber = [&](const char* key, long long& output) {
        const auto* item = member(value, key);
        return item != nullptr && readNumber(*item, output);
    };
    long long number = 0;
    bool valid = true;
    if (!readRequiredNumber("ordinal", number)) {
        errors.push_back("observations[" + std::to_string(index) + "] has invalid ordinal");
        valid = false;
    } else if (number < 0 || number > static_cast<long long>(std::numeric_limits<std::uint32_t>::max())) {
        observation.ordinal = 0U;
        errors.push_back("observations[" + std::to_string(index) + "] has out-of-range ordinal");
        valid = false;
    } else {
        observation.ordinal = static_cast<std::uint32_t>(number);
    }
    if (!readRequiredNumber("raw", number)) {
        errors.push_back("observations[" + std::to_string(index) + "] has invalid raw");
        valid = false;
    } else if (number < 0 || number > 255) {
        observation.rawValue = -1;
        errors.push_back("observations[" + std::to_string(index) + "] has out-of-range raw");
        valid = false;
    } else {
        observation.rawValue = static_cast<int>(number);
    }
    if (const auto* item = member(value, "snapshotSequence"); item != nullptr) {
        if (readNumber(*item, number) && number >= 0) {
            observation.snapshotSequence = static_cast<std::uint64_t>(number);
        } else {
            errors.push_back("observations[" + std::to_string(index) + "] has invalid snapshotSequence");
            valid = false;
        }
    }
    if (!readRequiredNumber("stableFrames", number)) {
        errors.push_back("observations[" + std::to_string(index) + "] has invalid stableFrames");
        valid = false;
    } else if (number < 0 || number > static_cast<long long>(std::numeric_limits<std::uint32_t>::max())) {
        observation.stableFrames = 0U;
        errors.push_back("observations[" + std::to_string(index) + "] has out-of-range stableFrames");
        valid = false;
    } else {
        observation.stableFrames = static_cast<std::uint32_t>(number);
    }
    if (const auto* item = member(value, "playerInVehicle"); item == nullptr ||
        !readBoolean(*item, observation.playerInVehicle)) {
        errors.push_back("observations[" + std::to_string(index) + "] has invalid playerInVehicle");
        valid = false;
    }
    if (const auto* item = member(value, "visibleLabel"); item != nullptr) {
        std::string label;
        if (!readString(*item, label)) {
            errors.push_back("observations[" + std::to_string(index) + "] has invalid visibleLabel");
            valid = false;
        } else {
            observation.visibleLabel = std::move(label);
        }
    }
    if (const auto* item = member(value, "identity"); item != nullptr) {
        std::string identity;
        if (!readString(*item, identity)) {
            errors.push_back("observations[" + std::to_string(index) + "] has invalid identity");
            valid = false;
        } else {
            const auto parsed = radioStationIdentityFromName(identity);
            if (!parsed) {
                errors.push_back("observations[" + std::to_string(index) + "] has invalid identity");
                valid = false;
            } else {
                observation.annotatedIdentity = *parsed;
            }
        }
    }
    return valid;
}

void appendObservationJson(std::ostringstream& output, const RadioStationObservation& observation,
                           bool includeSession) {
    output << "{\n";
    if (includeSession) {
        output << "      \"sessionId\": \"" << escapeJson(observation.sessionId) << "\",\n";
    }
    output << "      \"ordinal\": " << observation.ordinal << ",\n"
           << "      \"raw\": " << observation.rawValue << ",\n"
           << "      \"snapshotSequence\": " << observation.snapshotSequence << ",\n"
           << "      \"stableFrames\": " << observation.stableFrames << ",\n"
           << "      \"playerInVehicle\": " << (observation.playerInVehicle ? "true" : "false");
    if (observation.visibleLabel) {
        output << ",\n      \"visibleLabel\": \"" << escapeJson(*observation.visibleLabel) << "\"";
    }
    if (observation.annotatedIdentity) {
        output << ",\n      \"identity\": \""
               << radioStationIdentityName(*observation.annotatedIdentity) << "\"";
    }
    output << "\n    }";
}

} // namespace

const char* radioStationIdentityName(const RadioStationIdentity identity) noexcept {
    switch (identity) {
    case RadioStationIdentity::unknown: return "unknown";
    case RadioStationIdentity::headRadio: return "headRadio";
    case RadioStationIdentity::doubleClefFm: return "doubleClefFm";
    case RadioStationIdentity::jahRadio: return "jahRadio";
    case RadioStationIdentity::riseFm: return "riseFm";
    case RadioStationIdentity::lips106: return "lips106";
    case RadioStationIdentity::gameFm: return "gameFm";
    case RadioStationIdentity::msxFm: return "msxFm";
    case RadioStationIdentity::flashback956: return "flashback956";
    case RadioStationIdentity::chatterbox109: return "chatterbox109";
    case RadioStationIdentity::mp3Player: return "mp3Player";
    case RadioStationIdentity::policeRadio: return "policeRadio";
    case RadioStationIdentity::radioOff: return "radioOff";
    }
    return "unknown";
}

std::optional<RadioStationIdentity>
radioStationIdentityFromName(const std::string_view name) noexcept {
    constexpr RadioStationIdentity values[] = {
        RadioStationIdentity::unknown, RadioStationIdentity::headRadio,
        RadioStationIdentity::doubleClefFm, RadioStationIdentity::jahRadio,
        RadioStationIdentity::riseFm, RadioStationIdentity::lips106, RadioStationIdentity::gameFm,
        RadioStationIdentity::msxFm, RadioStationIdentity::flashback956,
        RadioStationIdentity::chatterbox109, RadioStationIdentity::mp3Player,
        RadioStationIdentity::policeRadio, RadioStationIdentity::radioOff,
    };
    for (const auto value : values) {
        if (name == radioStationIdentityName(value)) {
            return value;
        }
    }
    return std::nullopt;
}

const char* radioStationEvidenceLevelName(const RadioStationEvidenceLevel level) noexcept {
    switch (level) {
    case RadioStationEvidenceLevel::unverified: return "unverified";
    case RadioStationEvidenceLevel::observedOnce: return "observedOnce";
    case RadioStationEvidenceLevel::locallyReproduced: return "locallyReproduced";
    case RadioStationEvidenceLevel::independentlyReproduced: return "independentlyReproduced";
    }
    return "unverified";
}

const char* executableProfileIdName(const ExecutableProfileId profile) noexcept {
    switch (profile) {
    case ExecutableProfileId::unsupported: return "unsupported";
    case ExecutableProfileId::gta3_classic_local_candidate: return "gta3_classic_local_candidate";
    case ExecutableProfileId::gta3_10_us_candidate: return "gta3_10_us_candidate";
    }
    return "unsupported";
}

std::optional<ExecutableProfileId>
executableProfileIdFromName(const std::string_view name) noexcept {
    constexpr ExecutableProfileId values[] = {
        ExecutableProfileId::unsupported, ExecutableProfileId::gta3_classic_local_candidate,
        ExecutableProfileId::gta3_10_us_candidate,
    };
    for (const auto value : values) {
        if (name == executableProfileIdName(value)) {
            return value;
        }
    }
    return std::nullopt;
}

RadioStationEvidenceValidation
validateRadioStationEvidence(const RadioStationEvidenceDocument& document) {
    RadioStationEvidenceValidation result;
    if (document.schemaVersion != 1) {
        result.errors.push_back("unsupported schemaVersion");
    }
    if (document.executableProfile == ExecutableProfileId::unsupported) {
        result.errors.push_back("executableProfile must identify a registered profile");
    }
    if (!isSafeIdentifier(document.sessionId)) {
        result.errors.push_back("sessionId must be a short identifier");
    }
    if (!document.sourceId.empty() && !isSafeIdentifier(document.sourceId)) {
        result.errors.push_back("sourceId must be a short identifier");
    }
    std::set<std::uint32_t> ordinals;
    std::map<int, std::set<RadioStationIdentity>> identitiesByRaw;
    std::map<int, std::set<std::string>> labelsByRaw;
    std::map<std::string, std::set<int>> rawsByLabel;
    for (const auto& observation : document.observations) {
        if (observation.ordinal == 0U || !ordinals.insert(observation.ordinal).second) {
            result.errors.push_back("ordinal is zero or duplicated");
        }
        if (observation.rawValue < 0 || observation.rawValue > 255) {
            result.errors.push_back("raw must be in the range 0..255");
        }
        if (observation.stableFrames == 0U) {
            result.errors.push_back("stableFrames must be positive");
        }
        if (!observation.playerInVehicle) {
            result.errors.push_back("observation must be recorded while playerInVehicle is true");
        }
        if (observation.sessionId != document.sessionId) {
            result.errors.push_back("observation sessionId does not match document sessionId");
        }
        if (observation.visibleLabel && !isSafeVisibleLabel(*observation.visibleLabel)) {
            result.errors.push_back("visibleLabel contains forbidden content");
        }
        if (observation.annotatedIdentity && *observation.annotatedIdentity != RadioStationIdentity::unknown) {
            identitiesByRaw[observation.rawValue].insert(*observation.annotatedIdentity);
            if (!observation.visibleLabel || observation.visibleLabel->empty()) {
                result.errors.push_back("known identity requires a visibleLabel");
            }
        }
        if (observation.visibleLabel && !observation.visibleLabel->empty()) {
            labelsByRaw[observation.rawValue].insert(*observation.visibleLabel);
            rawsByLabel[*observation.visibleLabel].insert(observation.rawValue);
        }
    }
    for (const auto& entry : identitiesByRaw) {
        if (entry.second.size() > 1U) {
            result.errors.push_back("raw is associated with two identities: " +
                                    std::to_string(entry.first));
        }
    }
    for (const auto& entry : labelsByRaw) {
        if (entry.second.size() > 1U) {
            result.errors.push_back("raw is associated with two visible labels: " +
                                    std::to_string(entry.first));
        }
    }
    for (const auto& entry : rawsByLabel) {
        if (entry.second.size() > 1U) {
            result.errors.push_back("visible label is associated with two raws: " + entry.first);
        }
    }
    result.valid = result.errors.empty();
    return result;
}

RadioStationEvidenceParseResult parseRadioStationEvidenceJson(const std::string_view json) {
    RadioStationEvidenceParseResult result;
    JsonValue root;
    JsonParser parser(json);
    if (!parser.parse(root) || root.type != JsonValue::Type::object) {
        result.validation.errors.emplace_back("invalid JSON document");
        return result;
    }
    if (hasSensitiveField(root)) {
        result.validation.errors.emplace_back("document contains a forbidden sensitive field");
        return result;
    }
    long long number = 0;
    if (const auto* item = member(root, "schemaVersion"); item != nullptr && readNumber(*item, number) &&
        number >= 0 && number <= 1) {
        result.document.schemaVersion = static_cast<int>(number);
    } else {
        result.validation.errors.emplace_back("schemaVersion is required or invalid");
    }
    std::string text;
    if (const auto* item = member(root, "executableProfile"); item != nullptr && readString(*item, text)) {
        const auto parsed = executableProfileIdFromName(text);
        if (parsed) {
            result.document.executableProfile = *parsed;
        } else {
            result.validation.errors.emplace_back("invalid executableProfile");
        }
    } else {
        result.validation.errors.emplace_back("executableProfile is required");
    }
    if (const auto* item = member(root, "sessionId"); item != nullptr && readString(*item, text)) {
        result.document.sessionId = text;
    } else {
        result.validation.errors.emplace_back("sessionId is required");
    }
    if (const auto* item = member(root, "sourceId"); item != nullptr && readString(*item, text)) {
        result.document.sourceId = text;
    }
    const auto* observations = member(root, "observations");
    if (observations == nullptr || observations->type != JsonValue::Type::array) {
        result.validation.errors.emplace_back("observations must be an array");
    } else {
        for (std::size_t index = 0; index < observations->arrayValue.size(); ++index) {
            RadioStationObservation observation;
            if (parseObservation(observations->arrayValue[index], result.document, observation,
                                  result.validation.errors, index)) {
                result.document.observations.push_back(std::move(observation));
            }
        }
    }
    const auto semantic = validateRadioStationEvidence(result.document);
    result.validation.errors.insert(result.validation.errors.end(), semantic.errors.begin(), semantic.errors.end());
    result.validation.warnings = semantic.warnings;
    result.validation.valid = result.validation.errors.empty();
    result.success = result.validation.valid;
    return result;
}

std::string serializeRadioStationEvidenceJson(const RadioStationEvidenceDocument& document) {
    std::ostringstream output;
    output << "{\n"
           << "  \"schemaVersion\": " << document.schemaVersion << ",\n"
           << "  \"executableProfile\": \"" << executableProfileIdName(document.executableProfile) << "\",\n"
           << "  \"sessionId\": \"" << escapeJson(document.sessionId) << "\",\n"
           << "  \"sourceId\": \"" << escapeJson(document.sourceId) << "\",\n"
           << "  \"observations\": [\n";
    for (std::size_t index = 0; index < document.observations.size(); ++index) {
        appendObservationJson(output, document.observations[index], false);
        if (index + 1U != document.observations.size()) {
            output << ',';
        }
        output << '\n';
    }
    output << "  ]\n}\n";
    return output.str();
}

RadioStationEvidenceComparison compareRadioStationEvidence(
    const RadioStationEvidenceDocument& first, const RadioStationEvidenceDocument& second) {
    RadioStationEvidenceComparison result;
    const auto firstValidation = validateRadioStationEvidence(first);
    const auto secondValidation = validateRadioStationEvidence(second);
    if (!firstValidation.valid || !secondValidation.valid) {
        result.conflicts.emplace_back("one or both evidence documents are invalid");
    }
    if (first.executableProfile != second.executableProfile) {
        result.sameExecutableProfile = false;
        result.conflicts.emplace_back("executable profiles differ");
    }
    std::map<int, std::pair<RadioStationIdentity, std::string>> firstByRaw;
    std::map<int, std::pair<RadioStationIdentity, std::string>> secondByRaw;
    auto collect = [](const RadioStationEvidenceDocument& document,
                      std::map<int, std::pair<RadioStationIdentity, std::string>>& output) {
        for (const auto& observation : document.observations) {
            if (observation.annotatedIdentity && *observation.annotatedIdentity != RadioStationIdentity::unknown) {
                output[observation.rawValue] = {*observation.annotatedIdentity,
                                                observation.visibleLabel.value_or("")};
            }
        }
    };
    collect(first, firstByRaw);
    collect(second, secondByRaw);
    for (const auto& entry : firstByRaw) {
        const auto found = secondByRaw.find(entry.first);
        if (found != secondByRaw.end() && found->second != entry.second) {
            result.conflicts.push_back("raw " + std::to_string(entry.first) + " differs between sessions");
        } else if (found == secondByRaw.end()) {
            result.warnings.push_back("raw " + std::to_string(entry.first) + " is absent from the second session");
        }
    }
    for (const auto& entry : secondByRaw) {
        const auto found = firstByRaw.find(entry.first);
        if (found != firstByRaw.end() && found->second != entry.second) {
            continue;
        }
        if (found == firstByRaw.end()) {
            result.warnings.push_back("raw " + std::to_string(entry.first) + " is absent from the first session");
        }
    }
    result.equivalent = result.conflicts.empty();
    return result;
}

RadioStationMapProfile buildRadioStationMap(
    const ExecutableProfileId profile, const std::vector<RadioStationEvidenceDocument>& documents,
    std::string evidenceRevision) {
    RadioStationMapProfile result;
    result.executableProfile = profile;
    result.evidenceRevision = std::move(evidenceRevision);
    struct Aggregate {
        std::set<std::string> sessions;
        std::set<std::string> sources;
        std::set<RadioStationIdentity> identities;
        std::set<std::string> labels;
    };
    std::map<int, Aggregate> aggregate;
    for (const auto& document : documents) {
        if (document.executableProfile != profile || !validateRadioStationEvidence(document).valid) {
            continue;
        }
        for (const auto& observation : document.observations) {
            auto& entry = aggregate[observation.rawValue];
            entry.sessions.insert(document.sessionId);
            if (!document.sourceId.empty()) {
                entry.sources.insert(document.sourceId);
            }
            if (observation.annotatedIdentity && *observation.annotatedIdentity != RadioStationIdentity::unknown) {
                entry.identities.insert(*observation.annotatedIdentity);
            }
            if (observation.visibleLabel && !observation.visibleLabel->empty()) {
                entry.labels.insert(*observation.visibleLabel);
            }
        }
    }
    for (const auto& item : aggregate) {
        RadioStationMapEntry entry;
        entry.rawValue = item.first;
        entry.localSessionCount = item.second.sessions.size();
        entry.independentSourceCount = item.second.sources.size();
        if (item.second.identities.size() == 1U && item.second.labels.size() <= 1U) {
            entry.identity = *item.second.identities.begin();
        }
        if (entry.identity == RadioStationIdentity::unknown) {
            entry.evidence = RadioStationEvidenceLevel::unverified;
        } else if (entry.independentSourceCount >= 2U) {
            entry.evidence = RadioStationEvidenceLevel::independentlyReproduced;
        } else if (entry.localSessionCount >= 2U) {
            entry.evidence = RadioStationEvidenceLevel::locallyReproduced;
        } else {
            entry.evidence = RadioStationEvidenceLevel::observedOnce;
        }
        result.entries.push_back(entry);
    }
    return result;
}

} // namespace saors
