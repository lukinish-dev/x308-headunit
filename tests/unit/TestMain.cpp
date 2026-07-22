#include "x308/Cli.hpp"
#include "x308/Configuration.hpp"
#include "x308/SourceManager.hpp"
#include "x308/MpdClient.hpp"
#include "x308/BluetoothCtlManager.hpp"
#include "x308/SystemStatusPresenter.hpp"
#include "x308/SystemStatusService.hpp"
#include "x308/InteractiveMenu.hpp"

#include <iostream>
#include <chrono>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

int failures = 0;

class FakeProcessRunner final : public x308::IProcessRunner {
public:
    x308::ProcessResult nextResult;
    std::string executable;
    std::vector<std::string> arguments;
    x308::ProcessResult run(std::string_view value, const std::vector<std::string>& args,
                            std::chrono::milliseconds) override {
        executable = value;
        arguments = args;
        return nextResult;
    }
};

class FakeMediaPlayer final : public x308::IMediaPlayer {
public:
    std::vector<std::string>& calls;
    x308::MediaStatus currentStatus;
    std::chrono::milliseconds statusDelay{0};
    explicit FakeMediaPlayer(std::vector<std::string>& value) : calls(value) {}
    x308::MediaStatus status() override {
        std::this_thread::sleep_for(statusDelay);
        return currentStatus;
    }
    x308::Result play() override { calls.emplace_back("mpd.play"); return x308::Result::ok(); }
    x308::Result pause() override { calls.emplace_back("mpd.pause"); return x308::Result::ok(); }
    x308::Result stop() override { calls.emplace_back("mpd.stop"); return x308::Result::ok(); }
    x308::Result togglePause() override { calls.emplace_back("mpd.toggle"); return x308::Result::ok(); }
    x308::Result next() override { calls.emplace_back("mpd.next"); return x308::Result::ok(); }
    x308::Result previous() override { calls.emplace_back("mpd.previous"); return x308::Result::ok(); }
    std::vector<x308::Track> queue() override { return {}; }
    x308::Result clearQueue() override { calls.emplace_back("mpd.clear"); return x308::Result::ok(); }
    std::vector<x308::LibraryEntry> library(std::string_view) override { return {}; }
    x308::Result add(std::string_view) override { calls.emplace_back("mpd.add"); return x308::Result::ok(); }
    x308::Result addFolder(std::string_view) override { calls.emplace_back("mpd.add-folder"); return x308::Result::ok(); }
    x308::Result setRandom(bool) override { calls.emplace_back("mpd.random"); return x308::Result::ok(); }
    x308::Result setRepeat(bool) override { calls.emplace_back("mpd.repeat"); return x308::Result::ok(); }
    x308::Result update() override { calls.emplace_back("mpd.update"); return x308::Result::ok(); }
    x308::Result activateAudio() override { calls.emplace_back("mpd.activate"); return x308::Result::ok(); }
    x308::Result releaseAudio() override { calls.emplace_back("mpd.release"); return x308::Result::ok(); }
    std::string lastError() const override { return {}; }
};

class FakeBluetooth final : public x308::IBluetoothManager {
public:
    std::vector<std::string>& calls;
    x308::BluetoothStatus currentStatus;
    std::chrono::milliseconds statusDelay{0};
    explicit FakeBluetooth(std::vector<std::string>& value) : calls(value) {}
    x308::BluetoothStatus status() override {
        std::this_thread::sleep_for(statusDelay);
        return currentStatus;
    }
    x308::Result setPower(bool) override { calls.emplace_back("bt.power"); return x308::Result::ok(); }
    x308::Result startScan() override { calls.emplace_back("bt.scan-start"); return x308::Result::ok(); }
    x308::Result stopScan() override { calls.emplace_back("bt.scan-stop"); return x308::Result::ok(); }
    std::vector<x308::BluetoothDevice> devices() override { return {}; }
    x308::Result pair(std::string_view) override { calls.emplace_back("bt.pair"); return x308::Result::ok(); }
    x308::Result trust(std::string_view, bool) override { calls.emplace_back("bt.trust"); return x308::Result::ok(); }
    x308::Result connect(std::string_view) override { calls.emplace_back("bt.connect"); return x308::Result::ok(); }
    x308::Result disconnect(std::string_view) override { calls.emplace_back("bt.disconnect"); return x308::Result::ok(); }
    x308::Result remove(std::string_view) override { calls.emplace_back("bt.remove"); return x308::Result::ok(); }
    x308::Result setPairingMode(bool) override { calls.emplace_back("bt.pairing-mode"); return x308::Result::ok(); }
    x308::Result autoConnect() override { calls.emplace_back("bt.auto-connect"); return x308::Result::ok(); }
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

void testMacValidation() {
    expect(x308::BluetoothCtlManager::isValidMac("AA:bb:01:23:45:67"), "valid MAC accepted");
    expect(!x308::BluetoothCtlManager::isValidMac("AA:BB:CC;DD:EE:FF"), "invalid separator rejected");
    expect(!x308::BluetoothCtlManager::isValidMac("AA:BB:CC:DD:EE"), "short MAC rejected");
}

void testBluetoothParsing() {
    const auto devices = x308::BluetoothCtlManager::parseDeviceList(
        "Device AA:BB:CC:DD:EE:FF Phone One\nDevice 12:34:56:78:9A:BC Speaker\n");
    expect(devices.size() == 2, "Bluetooth device list parsed");
    auto device = x308::BluetoothCtlManager::parseDeviceInfo(
        "Device AA:BB:CC:DD:EE:FF (public)\n\tName: Phone One\n\tPaired: yes\n"
        "\tTrusted: yes\n\tConnected: no\n", devices.front());
    expect(device.name == "Phone One", "Bluetooth device name parsed");
    expect(device.paired && device.trusted && !device.connected, "Bluetooth flags parsed");

    const auto status = x308::BluetoothCtlManager::parseControllerStatus(
        "Controller 54:78:C9:69:E6:1B (public)\n\tPowered: yes\n\tDiscoverable: no\n"
        "\tPairable: no\n\tDiscovering: no\n");
    expect(status.adapterAvailable && status.powered, "saved controller output parsed");
    expect(!status.discoverable && !status.pairable && !status.discovering,
           "saved controller boolean flags parsed");
}

void testBluetoothUsesSeparatedArgumentsAndRejectsInvalidMac() {
    auto runner = std::make_shared<FakeProcessRunner>();
    runner->nextResult.exitCode = 0;
    x308::BluetoothCtlManager manager{x308::BluetoothConfig{}, runner};
    const auto valid = manager.trust("AA:BB:CC:DD:EE:FF", true);
    expect(valid.success, "valid trust operation succeeds through fake");
    expect(runner->executable == "bluetoothctl", "bluetoothctl executable is separate");
    expect(runner->arguments.size() == 2 && runner->arguments.at(0) == "trust" &&
           runner->arguments.at(1) == "AA:BB:CC:DD:EE:FF", "MAC is a separate process argument");
    const auto invalid = manager.connect("AA:BB;touch /tmp/x");
    expect(!invalid.success, "unsafe MAC rejected before process execution");
}

void testFirstAvailableTrustedDevice() {
    std::vector<x308::BluetoothDevice> devices{
        {"offline", "00:00:00:00:00:01", true, true, false, false},
        {"untrusted", "00:00:00:00:00:02", true, false, false, true},
        {"phone", "00:00:00:00:00:03", true, true, false, true}};
    const auto selected = x308::BluetoothCtlManager::firstAvailableTrusted(devices);
    expect(selected.has_value() && selected->name == "phone", "first available trusted device selected");
}

void testBluetoothTimeoutIsReported() {
    auto runner = std::make_shared<FakeProcessRunner>();
    runner->nextResult.timedOut = true;
    runner->nextResult.exitCode = 143;
    x308::BluetoothCtlManager manager{x308::BluetoothConfig{}, runner};
    const auto result = manager.setPower(true);
    expect(!result.success && manager.lastError().find("timed out") != std::string::npos,
           "process timeout is converted to module error");
}

void testProcessRunnerCapturesSeparateStreams() {
    x308::PosixProcessRunner runner;
    const auto result = runner.run(X308_PROCESS_FIXTURE_PATH, {"output"}, std::chrono::seconds{1});
    expect(result.exitCode == 7 && !result.timedOut, "process exit code captured");
    expect(result.standardOutput == "fixture stdout\n", "process stdout captured");
    expect(result.standardError == "fixture stderr\n", "process stderr captured");
}

void testProcessRunnerKillsProcessTreeAtDeadline() {
    x308::PosixProcessRunner runner;
    const auto started = std::chrono::steady_clock::now();
    const auto result = runner.run(X308_PROCESS_FIXTURE_PATH, {"hang-tree"},
                                   std::chrono::milliseconds{150});
    const auto elapsed = std::chrono::steady_clock::now() - started;
    expect(result.timedOut, "process tree timeout reported");
    expect(elapsed < std::chrono::seconds{2}, "process tree and inherited pipes terminate promptly");
    expect(result.standardOutput.find("fixture ready") != std::string::npos,
           "output before timeout is retained");
}

void testProcessRunnerCapsRequestedTimeout() {
    x308::PosixProcessRunner runner{
        std::chrono::milliseconds{50}, std::chrono::milliseconds{10}};
    const auto started = std::chrono::steady_clock::now();
    const auto result = runner.run(X308_PROCESS_FIXTURE_PATH, {"hang-tree"},
                                   std::chrono::seconds{5});
    const auto elapsed = std::chrono::steady_clock::now() - started;
    expect(result.timedOut, "configured process timeout cap is applied");
    expect(elapsed < std::chrono::milliseconds{500}, "timeout cap returns promptly");
}

void testSystemStatusAggregatesReadOnlyData() {
    std::vector<std::string> calls;
    FakeMediaPlayer mpd{calls};
    mpd.currentStatus.available = true;
    mpd.currentStatus.state = x308::PlaybackState::playing;
    mpd.currentStatus.currentTrack = x308::Track{
        "artist/song.flac", "Test Song", "Test Artist", "Test Album"};

    FakeBluetooth bluetooth{calls};
    bluetooth.currentStatus.serviceAvailable = true;
    bluetooth.currentStatus.adapterAvailable = true;
    bluetooth.currentStatus.powered = true;
    bluetooth.currentStatus.activeAudioDevice = x308::BluetoothDevice{
        "Test Phone", "AA:BB:CC:DD:EE:FF", true, true, true, true};

    FakeAudioOutput output{calls};
    x308::SourceManager sourceManager{mpd, bluetooth, output};
    const auto startedAt = std::chrono::steady_clock::now() - std::chrono::seconds{2};
    x308::SystemStatusService service{
        mpd, bluetooth, sourceManager, "/tmp", "test-version", "Test", startedAt};

    const auto report = service.collect();
    expect(report.application.version == "test-version" && report.application.buildType == "Test",
           "application build information aggregated");
    expect(report.application.uptime >= std::chrono::seconds{2}, "application uptime aggregated");
    expect(!report.system.hostname.empty() && !report.system.kernel.empty(),
           "system identity aggregated");
    expect(report.system.uptime > std::chrono::seconds{0}, "system uptime aggregated");
    expect(report.storage.present && report.storage.accessible && report.storage.freeBytes.has_value(),
           "storage status aggregated");
    expect(report.mpd.available && report.mpd.state == x308::PlaybackState::playing,
           "MPD status aggregated");
    expect(report.mpd.currentTrack == "Test Song" && report.mpd.artist == "Test Artist",
           "MPD metadata aggregated");
    expect(report.bluetooth.adapterPresent && report.bluetooth.adapterPowered,
           "Bluetooth adapter status aggregated");
    expect(report.bluetooth.connectedDevice == "AA:BB:CC:DD:EE:FF" &&
           report.bluetooth.connectedDeviceName == "Test Phone",
           "connected Bluetooth device aggregated");
    expect(report.sourceManager.activeSource == x308::AudioSource::mpd,
           "active source aggregated");
    expect(report.collectionDuration < x308::SystemStatusService::collectionBudget,
           "unit status collection stays within budget");
    expect(calls.empty(), "status collection does not invoke state-changing operations");

    std::ostringstream rendered;
    x308::SystemStatusPresenter::print(report, rendered);
    expect(rendered.str().find("Test Song") != std::string::npos &&
           rendered.str().find("Test Phone") != std::string::npos,
           "status presenter renders shared report");
}

void testSystemStatusHandlesMissingStorage() {
    std::vector<std::string> calls;
    FakeMediaPlayer mpd{calls};
    FakeBluetooth bluetooth{calls};
    FakeAudioOutput output{calls};
    x308::SourceManager sourceManager{mpd, bluetooth, output};
    x308::SystemStatusService service{
        mpd, bluetooth, sourceManager, "/x308-headunit-missing-storage", "test", "Test"};
    const auto report = service.collect();
    expect(!report.storage.present && !report.storage.accessible &&
           !report.storage.freeBytes.has_value(), "missing storage is an expected status");
}

void testInteractiveMenuUsesSystemStatusService() {
    std::vector<std::string> calls;
    FakeMediaPlayer mpd{calls};
    FakeBluetooth bluetooth{calls};
    FakeAudioOutput output{calls};
    x308::SourceManager sourceManager{mpd, bluetooth, output};
    x308::SystemStatusService service{
        mpd, bluetooth, sourceManager, "/tmp", "menu-version", "Test"};
    x308::InteractiveMenu menu{&mpd, &bluetooth, &sourceManager, &service};
    std::istringstream input{"4\n0\n"};
    std::ostringstream rendered;
    const int result = menu.run(input, rendered);
    expect(result == 0 && rendered.str().find("menu-version") != std::string::npos,
           "interactive system status uses shared service and presenter");
    expect(calls.empty(), "interactive status does not invoke state-changing operations");
}

void testSystemStatusCollectsModuleStatusesConcurrently() {
    std::vector<std::string> calls;
    FakeMediaPlayer mpd{calls};
    FakeBluetooth bluetooth{calls};
    mpd.statusDelay = std::chrono::milliseconds{80};
    bluetooth.statusDelay = std::chrono::milliseconds{80};
    FakeAudioOutput output{calls};
    x308::SourceManager sourceManager{mpd, bluetooth, output};
    x308::SystemStatusService service{
        mpd, bluetooth, sourceManager, "/tmp", "test", "Test"};
    const auto report = service.collect();
    expect(report.collectionDuration >= std::chrono::milliseconds{75} &&
           report.collectionDuration < std::chrono::milliseconds{140},
           "independent module status probes run concurrently");
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
    testMacValidation();
    testBluetoothParsing();
    testBluetoothUsesSeparatedArgumentsAndRejectsInvalidMac();
    testFirstAvailableTrustedDevice();
    testBluetoothTimeoutIsReported();
    testProcessRunnerCapturesSeparateStreams();
    testProcessRunnerKillsProcessTreeAtDeadline();
    testProcessRunnerCapsRequestedTimeout();
    testSystemStatusAggregatesReadOnlyData();
    testSystemStatusHandlesMissingStorage();
    testInteractiveMenuUsesSystemStatusService();
    testSystemStatusCollectsModuleStatusesConcurrently();
    if (failures == 0) {
        std::cout << "All unit tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
