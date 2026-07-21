#include "x308/Cli.hpp"
#include "x308/Configuration.hpp"
#include "x308/SourceManager.hpp"
#include "x308/MpdClient.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

class FakeMediaPlayer final : public x308::IMediaPlayer {
public:
    std::vector<std::string>& calls;
    explicit FakeMediaPlayer(std::vector<std::string>& value) : calls(value) {}
    x308::MediaStatus status() override { return {}; }
    x308::Result play() override { return x308::Result::ok(); }
    x308::Result pause() override { calls.emplace_back("mpd.pause"); return x308::Result::ok(); }
    x308::Result stop() override { return x308::Result::ok(); }
    x308::Result togglePause() override { return x308::Result::ok(); }
    x308::Result next() override { return x308::Result::ok(); }
    x308::Result previous() override { return x308::Result::ok(); }
    std::vector<x308::Track> queue() override { return {}; }
    x308::Result clearQueue() override { return x308::Result::ok(); }
    std::vector<x308::LibraryEntry> library(std::string_view) override { return {}; }
    x308::Result add(std::string_view) override { return x308::Result::ok(); }
    x308::Result addFolder(std::string_view) override { return x308::Result::ok(); }
    x308::Result setRandom(bool) override { return x308::Result::ok(); }
    x308::Result setRepeat(bool) override { return x308::Result::ok(); }
    x308::Result update() override { return x308::Result::ok(); }
    x308::Result activateAudio() override { calls.emplace_back("mpd.activate"); return x308::Result::ok(); }
    x308::Result releaseAudio() override { calls.emplace_back("mpd.release"); return x308::Result::ok(); }
    std::string lastError() const override { return {}; }
};

class FakeBluetooth final : public x308::IBluetoothManager {
public:
    std::vector<std::string>& calls;
    explicit FakeBluetooth(std::vector<std::string>& value) : calls(value) {}
    x308::BluetoothStatus status() override { return {}; }
    x308::Result setPower(bool) override { return x308::Result::ok(); }
    x308::Result startScan() override { return x308::Result::ok(); }
    x308::Result stopScan() override { return x308::Result::ok(); }
    std::vector<x308::BluetoothDevice> devices() override { return {}; }
    x308::Result pair(std::string_view) override { return x308::Result::ok(); }
    x308::Result trust(std::string_view, bool) override { return x308::Result::ok(); }
    x308::Result connect(std::string_view) override { return x308::Result::ok(); }
    x308::Result disconnect(std::string_view) override { return x308::Result::ok(); }
    x308::Result remove(std::string_view) override { return x308::Result::ok(); }
    x308::Result setPairingMode(bool) override { return x308::Result::ok(); }
    x308::Result autoConnect() override { return x308::Result::ok(); }
    x308::Result activateAudio() override { calls.emplace_back("bt.activate"); return x308::Result::ok(); }
    x308::Result releaseAudio() override { calls.emplace_back("bt.release"); return x308::Result::ok(); }
    std::string lastError() const override { return {}; }
};

class FakeAudioOutput final : public x308::IAudioOutput {
public:
    std::vector<std::string>& calls;
    explicit FakeAudioOutput(std::vector<std::string>& value) : calls(value) {}
    x308::Result selectSource(const x308::AudioSource source) override {
        calls.emplace_back(source == x308::AudioSource::mpd ? "audio.mpd" : "audio.bluetooth");
        return x308::Result::ok();
    }
    std::string currentDevice() const override { return "fake"; }
};

void expect(const bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testConfigurationDefaults() {
    std::istringstream input{""};
    const auto config = x308::ConfigurationLoader::parse(input);
    expect(config.mpd.host == "localhost", "default MPD host");
    expect(config.mpd.port == 6600, "default MPD port");
    expect(config.bluetooth.deviceName == "Jaguar XJR", "default Bluetooth name");
}

void testConfigurationParsing() {
    std::istringstream input{
        "[mpd]\nport = 6601\nhost = \"music.local\"\n"
        "[bluetooth]\nauto_connect = false\n"};
    const auto config = x308::ConfigurationLoader::parse(input);
    expect(config.mpd.host == "music.local", "custom MPD host");
    expect(config.mpd.port == 6601, "custom MPD port");
    expect(!config.bluetooth.autoConnect, "custom auto-connect");
}

void testCliParsing() {
    const char* argv[] = {"x308-headunit", "--config", "custom.toml", "mpd", "status"};
    const auto result = x308::CliParser::parse(5, argv);
    expect(result.configPath.has_value(), "config path parsed");
    expect(result.command.size() == 2, "command path parsed");
    expect(result.command.at(0) == "mpd" && result.command.at(1) == "status", "command values");
}

void testCliErrors() {
    const char* argv[] = {"x308-headunit", "--config"};
    try {
        static_cast<void>(x308::CliParser::parse(2, argv));
        expect(false, "missing config value rejected");
    } catch (const x308::CliError&) {
        expect(true, "missing config value rejected");
    }
}

void testSourceManagerSwitchesToBluetoothInOrder() {
    std::vector<std::string> calls;
    FakeMediaPlayer mpd{calls};
    FakeBluetooth bluetooth{calls};
    FakeAudioOutput output{calls};
    x308::SourceManager manager{mpd, bluetooth, output};
    const auto result = manager.setSource(x308::AudioSource::bluetooth);
    expect(result.success, "switch to Bluetooth succeeds");
    expect(manager.activeSource() == x308::AudioSource::bluetooth, "Bluetooth becomes active");
    const std::vector<std::string> expected{
        "mpd.pause", "mpd.release", "audio.bluetooth", "bt.activate"};
    expect(calls == expected, "MPD releases audio before Bluetooth activation");
}

void testSourceManagerSwitchesToMpdWithoutPowerOff() {
    std::vector<std::string> calls;
    FakeMediaPlayer mpd{calls};
    FakeBluetooth bluetooth{calls};
    FakeAudioOutput output{calls};
    x308::SourceManager manager{mpd, bluetooth, output, x308::AudioSource::bluetooth};
    const auto result = manager.setSource(x308::AudioSource::mpd);
    expect(result.success, "switch to MPD succeeds");
    const std::vector<std::string> expected{"bt.release", "audio.mpd", "mpd.activate"};
    expect(calls == expected, "Bluetooth stream releases without powering adapter off");
}

void testMpdModelConversionHandlesMissingTags() {
    const auto track = x308::MpdClient::trackFromMetadata("folder/song.flac", nullptr, "Artist", nullptr);
    expect(track.uri == "folder/song.flac", "MPD URI converted");
    expect(track.title.empty(), "missing title is empty");
    expect(track.artist == "Artist", "MPD artist converted");
    expect(track.album.empty(), "missing album is empty");
}

}  // namespace

int main() {
    testConfigurationDefaults();
    testConfigurationParsing();
    testCliParsing();
    testCliErrors();
    testSourceManagerSwitchesToBluetoothInOrder();
    testSourceManagerSwitchesToMpdWithoutPowerOff();
    testMpdModelConversionHandlesMissingTags();
    if (failures == 0) {
        std::cout << "All unit tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
