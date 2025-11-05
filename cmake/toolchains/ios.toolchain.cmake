################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS Toolchain File
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ios.toolchain.cmake
################################################################

set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_SYSTEM_VERSION 15.0)
set(CMAKE_OSX_DEPLOYMENT_TARGET 15.0)

# Modern iOS devices only (arm64)
set(CMAKE_OSX_ARCHITECTURES arm64)

# Detect iOS SDK path
execute_process(
  COMMAND xcrun --sdk iphoneos --show-sdk-path
  OUTPUT_VARIABLE IOS_SDK_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

if(NOT IOS_SDK_PATH)
  message(FATAL_ERROR "iOS SDK not found. Please install Xcode and command line tools.")
endif()

set(CMAKE_OSX_SYSROOT ${IOS_SDK_PATH})

# Set iOS-specific flags
set(IOS_BUILD ON CACHE BOOL "Building for iOS" FORCE)

# Use clang for iOS
set(CMAKE_C_COMPILER /usr/bin/clang)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)

# iOS-specific compiler flags
set(CMAKE_C_FLAGS_INIT "-mios-version-min=15.0")
set(CMAKE_CXX_FLAGS_INIT "-mios-version-min=15.0")

# Enable bitcode (optional, can be disabled if problematic)
# set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -fembed-bitcode")
# set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -fembed-bitcode")

# Set library type to shared by default
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)

# Skip trying to link executables during configuration
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# iOS Simulator support (alternative)
option(IOS_SIMULATOR "Build for iOS Simulator" OFF)

if(IOS_SIMULATOR)
  execute_process(
    COMMAND xcrun --sdk iphonesimulator --show-sdk-path
    OUTPUT_VARIABLE IOS_SIM_SDK_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )

  if(IOS_SIM_SDK_PATH)
    set(CMAKE_OSX_SYSROOT ${IOS_SIM_SDK_PATH})
    set(CMAKE_OSX_ARCHITECTURES "arm64")  # Apple Silicon only
  endif()
endif()

message(STATUS "iOS Toolchain Configuration:")
message(STATUS "  System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  SDK: ${CMAKE_OSX_SYSROOT}")
message(STATUS "  Architectures: ${CMAKE_OSX_ARCHITECTURES}")
message(STATUS "  Deployment Target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
message(STATUS "  Simulator Mode: ${IOS_SIMULATOR}")
