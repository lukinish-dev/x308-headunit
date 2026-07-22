cmake_minimum_required(VERSION 3.25)

project(x308_headunit VERSION 0.1.0 LANGUAGES CXX)

option(X308_ENABLE_INTEGRATION_TESTS "Build tests that access host services" OFF)
set(MPDCLIENT_ROOT "" CACHE PATH "Optional libmpdclient development package sysroot")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
find_package(Threads REQUIRED)

find_path(MPDCLIENT_INCLUDE_DIR mpd/client.h
    HINTS ${MPDCLIENT_ROOT}/usr/include
)
find_library(MPDCLIENT_LIBRARY NAMES mpdclient libmpdclient.so.2
    HINTS ${MPDCLIENT_ROOT}/usr/lib/aarch64-linux-gnu
)
if(NOT MPDCLIENT_INCLUDE_DIR OR NOT MPDCLIENT_LIBRARY)
    message(FATAL_ERROR "libmpdclient development files are required (Debian: libmpdclient-dev)")
endif()

add_library(x308_core
    src/app/App.cpp
    src/app/Configuration.cpp
    src/app/Logger.cpp
    src/app/SystemStatusService.cpp
    src/cli/Cli.cpp
    src/cli/InteractiveMenu.cpp
    src/cli/SystemStatusPresenter.cpp
    src/mpd/MpdClient.cpp
    src/bluetooth/BluetoothCtlManager.cpp
    src/system/ProcessRunner.cpp
    src/source/SourceManager.cpp
    src/system/HardwareStubs.cpp
)

target_include_directories(x308_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${MPDCLIENT_INCLUDE_DIR}
)

target_link_libraries(x308_core PRIVATE ${MPDCLIENT_LIBRARY} Threads::Threads)

if(CMAKE_BUILD_TYPE)
    set(X308_BUILD_TYPE_VALUE "${CMAKE_BUILD_TYPE}")
else()
    set(X308_BUILD_TYPE_VALUE "Unknown")
endif()
target_compile_definitions(x308_core PRIVATE
    X308_VERSION="${PROJECT_VERSION}"
    X308_BUILD_TYPE="${X308_BUILD_TYPE_VALUE}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(x308_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(x308-headunit src/main.cpp)
target_link_libraries(x308-headunit PRIVATE x308_core)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(x308-headunit PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(BUILD_TESTING)
    add_test(NAME app_smoke_test COMMAND x308-headunit --version)

    add_executable(x308_process_fixture tests/unit/ProcessFixture.cpp)
    add_executable(x308_unit_tests
        tests/unit/TestMain.cpp
    )
    target_link_libraries(x308_unit_tests PRIVATE x308_core)
    target_compile_definitions(x308_unit_tests PRIVATE
        X308_PROCESS_FIXTURE_PATH="$<TARGET_FILE:x308_process_fixture>"
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(x308_unit_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME unit_tests COMMAND x308_unit_tests)
    set_tests_properties(unit_tests PROPERTIES LABELS "unit")

    if(X308_ENABLE_INTEGRATION_TESTS)
        add_executable(x308_integration_tests tests/integration/IntegrationMain.cpp)
        target_link_libraries(x308_integration_tests PRIVATE x308_core)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(x308_integration_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        endif()
        add_test(NAME mpd_integration_test COMMAND x308_integration_tests mpd)
        set_tests_properties(mpd_integration_test PROPERTIES LABELS "integration")
        add_test(NAME bluetooth_integration_test COMMAND x308_integration_tests bluetooth)
        set_tests_properties(bluetooth_integration_test PROPERTIES LABELS "integration" TIMEOUT 6)
        add_test(NAME system_status_integration_test COMMAND x308_integration_tests status)
        set_tests_properties(system_status_integration_test PROPERTIES LABELS "integration" TIMEOUT 1)
    endif()
endif()
