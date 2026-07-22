#include "x308/BluezDbusMediaController.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace x308 {
namespace {

struct JsonValue {
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;
    std::variant<std::monostate, bool, std::uint64_t, std::string, Array, Object> value;
};

class JsonParser {
public:
    explicit JsonParser(const std::string_view input) : input_(input) {}

    JsonValue parse() {
        auto result = parseValue();
        skipWhitespace();
        if (position_ != input_.size()) throw std::runtime_error("trailing JSON data");
        return result;
    }

private:
    void skipWhitespace() {
        while (position_ < input_.size() &&
               std::isspace(static_cast<unsigned char>(input_[position_])) != 0) {
            ++position_;
        }
    }

    char take() {
        if (position_ >= input_.size()) throw std::runtime_error("unexpected end of JSON");
        return input_[position_++];
    }

    bool consume(const std::string_view token) {
        if (!input_.substr(position_).starts_with(token)) return false;
        position_ += token.size();
        return true;
    }

    JsonValue parseValue() {
        skipWhitespace();
        if (position_ >= input_.size()) throw std::runtime_error("missing JSON value");
        if (input_[position_] == '{') return JsonValue{parseObject()};
        if (input_[position_] == '[') return JsonValue{parseArray()};
        if (input_[position_] == '"') return JsonValue{parseString()};
        if (consume("true")) return JsonValue{true};
        if (consume("false")) return JsonValue{false};
        if (consume("null")) return JsonValue{std::monostate{}};
        return JsonValue{parseUnsigned()};
    }

    JsonValue::Object parseObject() {
        static_cast<void>(take());
        JsonValue::Object result;
        skipWhitespace();
        if (position_ < input_.size() && input_[position_] == '}') {
            ++position_;
            return result;
        }
        while (true) {
            skipWhitespace();
            if (take() != '"') throw std::runtime_error("expected JSON object key");
            --position_;
            auto key = parseString();
            skipWhitespace();
            if (take() != ':') throw std::runtime_error("expected colon after JSON key");
            result.emplace(std::move(key), parseValue());
            skipWhitespace();
            const char delimiter = take();
            if (delimiter == '}') return result;
            if (delimiter != ',') throw std::runtime_error("expected comma in JSON object");
        }
    }

    JsonValue::Array parseArray() {
        static_cast<void>(take());
        JsonValue::Array result;
        skipWhitespace();
        if (position_ < input_.size() && input_[position_] == ']') {
            ++position_;
            return result;
        }
        while (true) {
            result.push_back(parseValue());
            skipWhitespace();
            const char delimiter = take();
            if (delimiter == ']') return result;
            if (delimiter != ',') throw std::runtime_error("expected comma in JSON array");
        }
    }

    static void appendCodePoint(std::string& target, const unsigned codePoint) {
        if (codePoint <= 0x7FU) {
            target.push_back(static_cast<char>(codePoint));
        } else if (codePoint <= 0x7FFU) {
            target.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
            target.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        } else {
            target.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
            target.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
            target.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
    }

    std::string parseString() {
        if (take() != '"') throw std::runtime_error("expected JSON string");
        std::string result;
        while (true) {
            const char character = take();
            if (character == '"') return result;
            if (character != '\\') {
                result.push_back(character);
                continue;
            }
            const char escaped = take();
            switch (escaped) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u': {
                    if (position_ + 4 > input_.size()) {
                        throw std::runtime_error("incomplete JSON unicode escape");
                    }
                    unsigned codePoint = 0;
                    const auto first = input_.data() + position_;
                    const auto last = first + 4;
                    const auto converted = std::from_chars(first, last, codePoint, 16);
                    if (converted.ec != std::errc{} || converted.ptr != last) {
                        throw std::runtime_error("invalid JSON unicode escape");
                    }
                    position_ += 4;
                    appendCodePoint(result, codePoint);
                    break;
                }
                default: throw std::runtime_error("invalid JSON escape");
            }
        }
    }

    std::uint64_t parseUnsigned() {
        const auto start = position_;
        while (position_ < input_.size() &&
               std::isdigit(static_cast<unsigned char>(input_[position_])) != 0) {
            ++position_;
        }
        if (start == position_) throw std::runtime_error("invalid JSON number");
        std::uint64_t result = 0;
        const auto converted = std::from_chars(input_.data() + start,
                                               input_.data() + position_, result);
        if (converted.ec != std::errc{}) throw std::runtime_error("JSON number out of range");
        return result;
    }

    std::string_view input_;
    std::size_t position_{0};
};

const JsonValue::Object* asObject(const JsonValue* value) {
    return value == nullptr ? nullptr : std::get_if<JsonValue::Object>(&value->value);
}

const JsonValue::Array* asArray(const JsonValue* value) {
    return value == nullptr ? nullptr : std::get_if<JsonValue::Array>(&value->value);
}

const JsonValue* member(const JsonValue::Object* object, const std::string_view name) {
    if (object == nullptr) return nullptr;
    const auto found = object->find(std::string{name});
    return found == object->end() ? nullptr : &found->second;
}

const JsonValue::Object* interfaceProperties(const JsonValue& object,
                                             const std::string_view interfaceName) {
    return asObject(member(asObject(&object), interfaceName));
}

const JsonValue* propertyData(const JsonValue::Object* properties,
                              const std::string_view propertyName) {
    return member(asObject(member(properties, propertyName)), "data");
}

std::string stringProperty(const JsonValue::Object* properties,
                           const std::string_view propertyName) {
    const auto* value = propertyData(properties, propertyName);
    if (value == nullptr) return {};
    const auto* text = std::get_if<std::string>(&value->value);
    return text == nullptr ? std::string{} : *text;
}

bool boolProperty(const JsonValue::Object* properties, const std::string_view propertyName) {
    const auto* value = propertyData(properties, propertyName);
    if (value == nullptr) return false;
    const auto* boolean = std::get_if<bool>(&value->value);
    return boolean != nullptr && *boolean;
}

std::optional<std::uint64_t> unsignedProperty(const JsonValue::Object* properties,
                                              const std::string_view propertyName) {
    const auto* value = propertyData(properties, propertyName);
    if (value == nullptr) return std::nullopt;
    const auto* number = std::get_if<std::uint64_t>(&value->value);
    return number == nullptr ? std::nullopt : std::optional<std::uint64_t>{*number};
}

PlaybackState playbackState(const std::string_view status) {
    if (status == "playing") return PlaybackState::playing;
    if (status == "paused") return PlaybackState::paused;
    if (status == "stopped") return PlaybackState::stopped;
    return PlaybackState::unknown;
}

std::string processError(const ProcessResult& result, const std::string_view action) {
    if (result.timedOut) return std::string{action} + " timed out";
    if (!result.standardError.empty()) return result.standardError;
    if (!result.standardOutput.empty()) return result.standardOutput;
    return std::string{action} + " failed with exit code " + std::to_string(result.exitCode);
}

}  // namespace

BluezDbusMediaController::BluezDbusMediaController(
    std::shared_ptr<IProcessRunner> processRunner, const std::chrono::milliseconds timeout)
    : processRunner_(std::move(processRunner)), timeout_(timeout) {
    if (processRunner_ == nullptr) throw std::invalid_argument("process runner is required");
    if (timeout_ <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("D-Bus timeout must be positive");
    }
}

BluetoothMediaStatus BluezDbusMediaController::status() {
    const auto result = processRunner_->run(
        "busctl",
        {"--system", "--json=short", "call", "org.bluez", "/",
         "org.freedesktop.DBus.ObjectManager", "GetManagedObjects"},
        timeout_);
    if (result.exitCode != 0 || result.timedOut) {
        BluetoothMediaStatus statusResult;
        statusResult.error = processError(result, "BlueZ ObjectManager request");
        return statusResult;
    }
    return parseManagedObjects(result.standardOutput);
}

BluetoothMediaStatus BluezDbusMediaController::parseManagedObjects(const std::string_view json) {
    BluetoothMediaStatus result;
    try {
        const auto root = JsonParser{json}.parse();
        const auto* data = asArray(member(asObject(&root), "data"));
        if (data == nullptr || data->empty()) {
            result.error = "BlueZ ObjectManager returned no objects";
            return result;
        }
        const auto* objects = asObject(&data->front());
        if (objects == nullptr) {
            result.error = "BlueZ ObjectManager returned an invalid object map";
            return result;
        }

        const JsonValue::Object* selectedPlayer = nullptr;
        for (const auto& [path, interfaces] : *objects) {
            const auto* device = interfaceProperties(interfaces, "org.bluez.Device1");
            const auto* control = interfaceProperties(interfaces, "org.bluez.MediaControl1");
            if (device != nullptr && control != nullptr && boolProperty(control, "Connected")) {
                result.connected = true;
                result.devicePath = path;
                result.deviceAddress = stringProperty(device, "Address");
                result.deviceName = stringProperty(device, "Name");
            }
            const auto* player = interfaceProperties(interfaces, "org.bluez.MediaPlayer1");
            if (player != nullptr && selectedPlayer == nullptr) {
                selectedPlayer = player;
                result.playerPath = path;
            }
        }

        if (selectedPlayer == nullptr) {
            result.error = "BlueZ MediaPlayer1 is unavailable";
            return result;
        }

        result.available = true;
        result.state = playbackState(stringProperty(selectedPlayer, "Status"));
        result.positionMilliseconds = unsignedProperty(selectedPlayer, "Position");
        const auto playerDevicePath = stringProperty(selectedPlayer, "Device");
        if (!playerDevicePath.empty()) result.devicePath = playerDevicePath;

        if (!result.devicePath.empty()) {
            const auto deviceObject = objects->find(result.devicePath);
            if (deviceObject != objects->end()) {
                const auto* device = interfaceProperties(deviceObject->second, "org.bluez.Device1");
                const auto* control =
                    interfaceProperties(deviceObject->second, "org.bluez.MediaControl1");
                result.deviceAddress = stringProperty(device, "Address");
                result.deviceName = stringProperty(device, "Name");
                result.connected = boolProperty(device, "Connected") ||
                                   boolProperty(control, "Connected");
            }
        }

        const auto* track = asObject(propertyData(selectedPlayer, "Track"));
        if (track != nullptr) {
            Track metadata;
            metadata.title = stringProperty(track, "Title");
            metadata.artist = stringProperty(track, "Artist");
            metadata.album = stringProperty(track, "Album");
            result.durationMilliseconds = unsignedProperty(track, "Duration");
            if (!metadata.title.empty() || !metadata.artist.empty() || !metadata.album.empty()) {
                result.currentTrack = std::move(metadata);
            }
        }
    } catch (const std::exception& error) {
        result = {};
        result.error = std::string{"Cannot parse BlueZ ObjectManager response: "} + error.what();
    }
    return result;
}

Result BluezDbusMediaController::invokeOnPlayer(const std::string_view playerPath,
                                                const std::string_view method) {
    const auto process = processRunner_->run(
        "busctl",
        {"--system", "call", "org.bluez", std::string{playerPath},
         "org.bluez.MediaPlayer1", std::string{method}},
        timeout_);
    if (process.exitCode != 0 || process.timedOut) {
        return Result::error(processError(process, std::string{"BlueZ MediaPlayer1."} +
                                                       std::string{method}));
    }
    return Result::ok(std::string{"BlueZ MediaPlayer1."} + std::string{method} + " completed");
}

Result BluezDbusMediaController::invoke(const std::string_view method) {
    const auto current = status();
    if (!current.available) return Result::error(current.error);
    return invokeOnPlayer(current.playerPath, method);
}

Result BluezDbusMediaController::play() { return invoke("Play"); }
Result BluezDbusMediaController::pause() { return invoke("Pause"); }
Result BluezDbusMediaController::next() { return invoke("Next"); }
Result BluezDbusMediaController::previous() { return invoke("Previous"); }

Result BluezDbusMediaController::togglePause() {
    const auto current = status();
    if (!current.available) return Result::error(current.error);
    return invokeOnPlayer(current.playerPath,
                          current.state == PlaybackState::playing ? "Pause" : "Play");
}

}  // namespace x308
