cmake_minimum_required(VERSION 3.25)

# Must be set before project() so every target, SDKs included, picks them up.
set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "Minimum macOS version")
if (NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")
endif()

project(p31-plugins VERSION 0.1.1 LANGUAGES C CXX OBJC OBJCXX)

# SDKs and clap-wrapper build as C++17 (the VST3 wrapper does not compile as
# C++20). Our own targets opt into C++20 through p31_cxx20() instead, which
# does not propagate to the wrapper targets that link them.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(P31_COPY_AFTER_BUILD "Copy built plugins into ~/Library/Audio/Plug-Ins" ON)
option(P31_BUILD_TESTS "Build unit tests" ON)
option(P31_BUILD_DEV_TOOLS "Build developer tools (dev/)" ON)
set(P31_SANITIZER "" CACHE STRING "Build everything with a sanitizer: thread, address or empty")

if (P31_SANITIZER)
    add_compile_options(-fsanitize=${P31_SANITIZER} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${P31_SANITIZER})
endif()

# --- SDKs (git submodules in libs/, see scripts/bootstrap.sh) ---
set(CLAP_SDK_ROOT      "${CMAKE_SOURCE_DIR}/libs/clap"         CACHE PATH "")
set(VST3_SDK_ROOT      "${CMAKE_SOURCE_DIR}/libs/vst3sdk"      CACHE PATH "")
set(AUDIOUNIT_SDK_ROOT "${CMAKE_SOURCE_DIR}/libs/AudioUnitSDK" CACHE PATH "")
set(RTAUDIO_SDK_ROOT   "${CMAKE_SOURCE_DIR}/libs/rtaudio"      CACHE PATH "")
set(RTMIDI_SDK_ROOT    "${CMAKE_SOURCE_DIR}/libs/rtmidi"       CACHE PATH "")
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "" FORCE)

add_subdirectory(libs/clap EXCLUDE_FROM_ALL)
add_subdirectory(libs/clap-wrapper EXCLUDE_FROM_ALL)
if (P31_BUILD_TESTS)
    enable_testing()
    add_subdirectory(libs/doctest EXCLUDE_FROM_ALL)
endif()

include(cmake/P31Plugin.cmake)

add_subdirectory(core)
add_subdirectory(render)
add_subdirectory(plugins/common)
add_subdirectory(plugins/spectrum)
add_subdirectory(plugins/scope)

if (P31_BUILD_DEV_TOOLS)
    add_subdirectory(dev)
endif()
