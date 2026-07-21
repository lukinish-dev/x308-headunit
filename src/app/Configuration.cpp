#include "x308/Configuration.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>

namespace x308 {
namespace {

std::string trim(std::string value) {
    const auto notSpace = [](const unsigned char character) { return std::isspace(character) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string parseString(const std::string& value, const std::size_t line) {
    if (value.size() < 2 || value.front() != '"' || value.back() != '"') {
        throw ConfigurationError("Expected quoted string at line " + std::to_string(line));
    }
    return value.substr(1, value.size() - 2);
}

bool parseBool(const std::string& value, const std::size_t line) {
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    throw ConfigurationError("Expected boolean at line " + std::to_string(line));
}

int parseInt(const std::string& value, const std::size_t line) {
    std::size_t consumed = 0;
    try {
        const int result = std::stoi(value, &consumed);
        if (consumed != value.size()) {
            throw ConfigurationError("Expected integer at line " + std::to_string(line));
        }
        return result;
    } catch (const std::invalid_argument&) {
        throw ConfigurationError("Expected integer at line " + std::to_string(line));
    } catch (const std::out_of_range&) {
        throw ConfigurationError("Integer out of range at line " + std::to_string(line));
    }
}

void apply(Configuration& config, const std::string& section, const std::string& key,
           const std::string& value, const std::size_t line) {
    if (section == "application" && key == "language") {
        config.application.language = parseString(value, line);
    } else if (section == "application" && key == "startup_timeout_seconds") {
        config.application.startupTimeoutSeconds = parseInt(value, line);
    } else if (section == "mpd" && key == "host") {
        config.mpd.host = parseString(value, line);
    } else if (section == "mpd" && key == "port") {
        const int port = parseInt(value, line);
        if (port < 1 || port > static_cast<int>(std::numeric_limits<unsigned short>::max())) {
            throw ConfigurationError("MPD port out of range at line " + std::to_string(line));
        }
        config.mpd.port = static_cast<unsigned>(port);
    } else if (section == "mpd" && key == "music_directory") {
        config.mpd.musicDirectory = parseString(value, line);
    } else if (section == "bluetooth" && key == "adapter") {
        config.bluetooth.adapter = parseString(value, line);
    } else if (section == "bluetooth" && key == "device_name") {
        config.bluetooth.deviceName = parseString(value, line);
    } else if (section == "bluetooth" && key == "auto_connect") {
        config.bluetooth.autoConnect = parseBool(value, line);
    } else if (section == "bluetooth" && key == "discoverable_timeout_seconds") {
        config.bluetooth.discoverableTimeoutSeconds = parseInt(value, line);
    } else if (section == "bluetooth" && key == "auto_accept_pairing") {
        config.bluetooth.autoAcceptPairing = parseBool(value, line);
    } else if (section == "audio" && key == "default_source") {
        config.audio.defaultSource = parseString(value, line);
    } else if (section == "logging" && key == "level") {
        config.logging.level = parseString(value, line);
    }
}

Configuration loadFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw ConfigurationError("Cannot open configuration: " + path.string());
    }
    Configuration config = ConfigurationLoader::parse(input);
    config.loadedFrom = path;
    return config;
}

}  // namespace

Configuration ConfigurationLoader::load(const std::optional<std::filesystem::path>& explicitPath) {
    if (explicitPath.has_value()) {
        return loadFile(*explicitPath);
    }

    const std::filesystem::path systemPath{"/etc/x308-headunit/config.toml"};
    if (std::filesystem::exists(systemPath)) {
        return loadFile(systemPath);
    }

    const std::filesystem::path developmentPath{"config/config.toml"};
    if (std::filesystem::exists(developmentPath)) {
        return loadFile(developmentPath);
    }
    return {};
}

Configuration ConfigurationLoader::parse(std::istream& input) {
    Configuration config;
    std::string section;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(std::move(line));
        if (line.empty()) {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            throw ConfigurationError("Expected key/value pair at line " + std::to_string(lineNumber));
        }
        apply(config, section, trim(line.substr(0, equals)), trim(line.substr(equals + 1)), lineNumber);
    }
    if (config.application.startupTimeoutSeconds < 1 ||
        config.bluetooth.discoverableTimeoutSeconds < 1) {
        throw ConfigurationError("Timeout values must be positive");
    }
    return config;
}

}  // namespace x308

