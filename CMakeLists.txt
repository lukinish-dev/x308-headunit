cmake_minimum_required(VERSION 3.25)

project(x308_headunit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

add_library(x308_core
    src/app/App.cpp
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
endif()

