cmake_minimum_required(VERSION 3.25)

project(x308_headunit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

add_library(x308_core
    src/app/App.cpp
    src/app/Configuration.cpp
    src/app/Logger.cpp
    src/cli/Cli.cpp
    src/cli/InteractiveMenu.cpp
    src/source/SourceManager.cpp
    src/system/HardwareStubs.cpp
)

target_include_directories(x308_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
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

    add_executable(x308_unit_tests
        tests/unit/TestMain.cpp
    )
    target_link_libraries(x308_unit_tests PRIVATE x308_core)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(x308_unit_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    add_test(NAME unit_tests COMMAND x308_unit_tests)
    set_tests_properties(unit_tests PROPERTIES LABELS "unit")
endif()
