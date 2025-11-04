# Orkid iOS Build Guide

## Overview

This guide covers building a minimal subset of `ork.core` as a shared library (`.dylib`) for iOS platforms. The iOS build focuses on core math and utility functionality without heavy dependencies like Python, OpenCL, or networking.

## Prerequisites

- macOS with Xcode installed
- Xcode Command Line Tools: `xcode-select --install`
- iOS SDK (comes with Xcode)
- OBT (Orkid Build Tools) environment configured
- Boost source at `$OBT_STAGE/builds/boost/boost-X.Y.Z/` (automatically cross-compiled for iOS)

## What's Included in iOS Build

The minimal iOS build (`libork_core_ios.dylib`) includes:

### Math Library
- Vector math (2D, 3D, 4D)
- Matrix math (3x3, 4x4)
- Quaternions
- Planes, boxes, frustums
- Collision detection

### Kernel Utilities
- Operation queues (opq)
- String utilities
- Memory utilities
- Property system
- Data blocks and caching
- Environment handling

### File System
- Path manipulation
- Basic file I/O

### Utilities
- CRC32/CRC64
- MD5 hashing
- Hex dump utilities

## What's Excluded

- Python bindings
- OpenCL support
- Networking (ZMQ, cURL)
- Asset management system
- Full dataflow/event systems
- Graphics (lev2) components

## Build Instructions

The build script automatically handles Boost cross-compilation for iOS. On first run, it will build Boost for iOS into a separate directory that doesn't interfere with the host build.

### Basic Build (iOS Device - arm64)

```bash
cd /path/to/orkid
ork.build.ios.py
```

This will:
1. Check if Boost is built for iOS (via manifest at `$OBT_STAGE/manifests/boost_ios_device`)
2. If not, automatically cross-compile Boost filesystem and system libraries for iOS
3. Configure CMake with iOS toolchain
4. Build `libork_core_ios.dylib`

**Note**: Boost is only built once per staging folder. The build creates a manifest file to track completion.

### Build for iOS Simulator

```bash
ork.build.ios.py --simulator
```

Builds for iOS Simulator (x86_64 + arm64). Boost will be built separately at `$OBT_STAGE/builds/boost-ios-simulator`.

### Debug Build

```bash
ork.build.ios.py --debug
```

### Clean Build

```bash
ork.build.ios.py --clean
```

### Force Rebuild Boost

```bash
ork.build.ios.py --rebuild-boost
```

Force rebuild of Boost libraries for iOS even if the manifest exists. This ignores the `$OBT_STAGE/manifests/boost_ios_device` (or `boost_ios_simulator`) marker.

Alternatively, manually remove the manifest:
```bash
rm $OBT_STAGE/manifests/boost_ios_device
# Or for simulator:
rm $OBT_STAGE/manifests/boost_ios_simulator
```

### Verbose Output

```bash
ork.build.ios.py --verbose
```

## Build Output

### Build Directories

- **Orkid Device Build**: `$OBT_STAGE/orkid-ios/`
- **Orkid Simulator Build**: `$OBT_STAGE/orkid-ios-simulator/`
- **Boost iOS Device**: `$OBT_STAGE/builds/boost-ios/`
- **Boost iOS Simulator**: `$OBT_STAGE/builds/boost-ios-simulator/`
- **Build Manifests**: `$OBT_STAGE/manifests/` (tracks completed builds)

### Library Output

- `libork_core_ios.dylib` - The minimal Orkid core shared library
- `libboost_filesystem.a` - Boost filesystem (static, iOS-specific)
- `libboost_system.a` - Boost system (static, iOS-specific)

### Installation

To install to the iOS SDK location:

```bash
cd $OBT_STAGE/orkid-ios  # or orkid-ios-simulator
make install
```

This installs to: `$OBT_STAGE/ios-sdk/`

## iOS SDK Structure

After installation, the SDK is organized as:

```
$OBT_STAGE/ios-sdk/
├── lib/
│   └── libork_core_ios.dylib
└── include/
    └── ork/
        ├── math/           # Math headers
        ├── kernel/         # Kernel headers
        ├── file/           # File system headers
        ├── util/           # Utility headers
        └── core_types.h    # Core type definitions
```

## Using in iOS Projects

### CMake Integration

```cmake
# In your iOS app's CMakeLists.txt
set(ORKID_IOS_SDK "$ENV{OBT_STAGE}/ios-sdk")

target_include_directories(MyApp PRIVATE ${ORKID_IOS_SDK}/include)
target_link_directories(MyApp PRIVATE ${ORKID_IOS_SDK}/lib)
target_link_libraries(MyApp PRIVATE ork_core_ios)
```

### Xcode Integration

1. Add to **Header Search Paths**: `$OBT_STAGE/ios-sdk/include`
2. Add to **Library Search Paths**: `$OBT_STAGE/ios-sdk/lib`
3. Link binary with: `libork_core_ios.dylib`

### Code Example

```cpp
#include <ork/math/cvector3.h>
#include <ork/math/cmatrix4.h>
#include <ork/kernel/string/string.h>

using namespace ork;

// Use vector math
fvec3 position(1.0f, 2.0f, 3.0f);
fvec3 velocity(0.1f, 0.0f, 0.0f);
fvec3 newPos = position + velocity;

// Use matrix math
fmtx4 transform;
transform.setTranslation(position);

// Use string utilities
std::string formatted = FormatString("Position: %f, %f, %f",
    newPos.x, newPos.y, newPos.z);
```

## Architecture Support

- **iOS Device**: arm64 only (modern iOS devices)
- **iOS Simulator**: arm64 + x86_64 (Apple Silicon + Intel Macs)

Minimum deployment target: **iOS 15.0**

## Dependencies

### Required Frameworks (Linked Automatically)
- Foundation
- Accelerate (for optimized math)

### Required Libraries (Built Automatically)
- Boost filesystem (static) - File path operations
- Boost system (static) - System utilities

The build script automatically cross-compiles Boost for iOS on first run. Boost source must be present at `$OBT_STAGE/builds/boost` (installed by host Orkid build).

## Dynamic Libraries on iOS

**Note**: iOS traditionally restricted dynamic libraries (`.dylib`) in App Store apps, but:

- **Simulators**: Full `.dylib` support for testing
- **Modern iOS**: Framework embedding allows dynamic libraries
- **Alternative**: Can be built as static library (`.a`) by modifying CMake

To use dynamic libraries in iOS apps:
1. Embed the `.dylib` in your app bundle
2. Set appropriate code signing
3. Ensure `@rpath` is configured correctly

## Troubleshooting

### "iOS SDK not found"

Ensure Xcode is installed:
```bash
xcode-select --install
xcrun --sdk iphoneos --show-sdk-path
```

### Missing Boost Source

If you see "Boost source directory not found":

```bash
# Ensure host Orkid build has been done first
ork.build.py

# This downloads/builds Boost for the host and places source at:
# $OBT_STAGE/builds/boost/boost-X.Y.Z/
# The iOS build script uses this same source to cross-compile for iOS
```

You can verify Boost source exists:
```bash
ls $OBT_STAGE/builds/boost/
# Should show: boost-1.81.0/ (or similar version)

ls $OBT_STAGE/builds/boost/boost-*/bootstrap.sh
# Should find the bootstrap script
```

### "Architecture mismatch"

Ensure you're building for the correct target:
- Device builds require arm64
- Simulator builds support arm64 + x86_64

### Code Signing Issues

For device deployment, the `.dylib` must be code-signed:
```bash
codesign -s "Your Identity" libork_core_ios.dylib
```

## Boost Cross-Compilation Details

The iOS build script handles Boost cross-compilation automatically using Boost's `b2` build system:

1. **Detection**: Checks manifest file (`$OBT_STAGE/manifests/boost_ios_device` or `boost_ios_simulator`)
2. **Bootstrap**: Runs `bootstrap.sh` if `b2` not present (one-time)
3. **Configuration**: Creates `user-config-ios.jam` with iOS toolchain settings
4. **Build**: Compiles Boost filesystem and system as static libraries
5. **Install**: Places libraries in `$OBT_STAGE/builds/boost-ios/`
6. **Manifest**: Creates manifest file to mark successful build (prevents rebuilds)

### What Gets Built

- `libboost_filesystem.a` - Compiled for iOS with proper SDK and architecture
- `libboost_system.a` - Static library for iOS linking

### Build Flags Used

```
-isysroot <iOS SDK path>
-arch arm64                    # Device
-arch x86_64 -arch arm64       # Simulator
-mios-version-min=15.0
```

### Separate Build Directories

Device and Simulator use separate Boost builds because they target different architectures:
- Device: `boost-ios/` (arm64 only)
- Simulator: `boost-ios-simulator/` (x86_64 + arm64 universal)

This ensures no conflicts between host, device, and simulator builds.

## Customization

### Adding More Files

Edit `ork.core/CMakeLists.txt` in the `BUILD_IOS_MINIMAL` section:

```cmake
IF(BUILD_IOS_MINIMAL)
  file(GLOB src_math_ios
    ${SRCD}/math/cvector2_imp.cpp
    ${SRCD}/math/your_new_file.cpp  # Add here
    ...
  )
  ...
ENDIF()
```

### Building as Static Library

Modify `cmake/toolchains/ios.toolchain.cmake`:

```cmake
# Change this line:
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)

# To:
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
```

## Advanced Options

### Manual CMake Configuration

```bash
mkdir build-ios
cd build-ios

cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/ios.toolchain.cmake \
  -DBUILD_IOS_MINIMAL=ON \
  -DIOS_BUILD=ON \
  -DARCHITECTURE=AARCH64 \
  -DCMAKE_BUILD_TYPE=Release

make -j8 ork_core_ios
```

### Cross-Compilation Variables

The iOS toolchain sets:
- `CMAKE_SYSTEM_NAME=iOS`
- `CMAKE_OSX_DEPLOYMENT_TARGET=15.0`
- `CMAKE_OSX_ARCHITECTURES=arm64`
- `IOS_BUILD=ON`

## Platform Definitions

When building for iOS, these preprocessor symbols are defined:

- `ORK_IOS` - iOS platform
- `ORK_CONFIG_IOS` - iOS configuration
- `ORK_ARCHITECTURE_ARM_64` - ARM64 architecture
- `USING_ORKID` - Using Orkid as external library

## Next Steps

- **Phase 2**: Package script (`ork.package.ios.py`) for SDK distribution
- **Phase 3**: Static library variant for App Store submissions
- **Phase 4**: Swift Package Manager integration
- **Phase 5**: Metal compute shader support (replacing OpenCL)

## Support

For issues or questions:
- Check session notes: `ork.data/misc/session_notes.md`
- Review build logs in build directory
- Ensure all prerequisites are met

---

Generated for Orkid Engine iOS Support - Phase 1
