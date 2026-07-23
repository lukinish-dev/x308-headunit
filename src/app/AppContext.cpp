#include "x308/AppContext.hpp"

#include "x308/BluetoothCtlManager.hpp"
#include "x308/BluezDbusMediaController.hpp"
#include "x308/Cli.hpp"
#include "x308/Configuration.hpp"
#include "x308/LinuxAudioOutputController.hpp"
#include "x308/InteractiveMenu.hpp"
#include "x308/Logger.hpp"
#include "x308/MpdClient.hpp"
#include "x308/ProcessRunner.hpp"
#include "x308/PlaybackSourceMonitor.hpp"
#include "x308/SourceManager.hpp"
#include "x308/SystemStatusService.hpp"

namespace x308 {

AppContext::~AppContext() = default;

}  // namespace x308
