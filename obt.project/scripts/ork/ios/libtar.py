################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS libtar Provider
# Clones libtar from git and builds static library using generated CMakeLists.txt
################################################################

import datetime
import os
import obt.path
import obt.git
from pathlib import Path

######################################################################
# libtar iOS Provider
######################################################################

def build_libtar_for_ios(
    ios_subspace,
    ios_builds_dir,
    ios_include_dir,
    ios_lib_dir,
    is_simulator,
    manifest_dir=None,
    force_rebuild=False,
    verbose=False,
    num_cores=8
):
    """Build libtar for iOS from git source using generated CMakeLists.txt

    Args:
        ios_subspace: iOS subspace root directory
        ios_builds_dir: iOS builds directory
        ios_include_dir: iOS include directory
        ios_lib_dir: iOS library directory
        is_simulator: True for simulator, False for device
        manifest_dir: Directory for storing build manifests (optional)
        force_rebuild: Force rebuild even if manifest exists
        verbose: Enable verbose build output
        num_cores: Number of parallel build jobs

    Returns:
        True if successful, False otherwise
    """
    import subprocess
    ios_subspace = Path(ios_subspace)
    ios_builds_dir = Path(ios_builds_dir)
    ios_include_dir = Path(ios_include_dir)
    ios_lib_dir = Path(ios_lib_dir)

    print("\n=== Building libtar for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "libtar"

        # Check if already built (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ libtar for iOS already built (manifest: libtar)")
            print(f"  Location: {ios_lib_dir}/libtar.a")
            return True

    # Clone libtar from git
    libtar_src_dir = ios_subspace / "builds" / "libtar"

    if not libtar_src_dir.exists() or force_rebuild:
        # Remove existing directory if force rebuild
        if force_rebuild and libtar_src_dir.exists():
            import shutil
            print(f"Removing existing source directory: {libtar_src_dir}")
            shutil.rmtree(libtar_src_dir)

        print(f"\nCloning libtar from git...")
        success = obt.git.Clone(
            url="git@github.com:tweakoz/libtar.git",
            dest=str(libtar_src_dir),
            rev="master",  # Clone master branch
            cache=False,
            shallow=False
        )
        if not success:
            print(f"✗ Failed to clone libtar")
            return False

        # Checkout the specific tag
        print(f"Checking out tag v1.2.20...")
        result = subprocess.run(
            ["git", "checkout", "v1.2.20"],
            cwd=str(libtar_src_dir),
            capture_output=True,
            text=True
        )
        if result.returncode != 0:
            print(f"✗ Failed to checkout tag v1.2.20")
            print(f"STDERR: {result.stderr}")
            return False

        print(f"✓ Cloned libtar and checked out tag v1.2.20")
    else:
        print(f"libtar source already exists at {libtar_src_dir}")

    # Generate CMakeLists.txt for libtar
    print(f"\nGenerating CMakeLists.txt for libtar...")
    cmake_content = """# Generated CMakeLists.txt for libtar iOS build
cmake_minimum_required(VERSION 3.14)
project(libtar C)

# Create a minimal config.h
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/config.h "
#ifndef CONFIG_H
#define CONFIG_H
/* Minimal config for iOS build */
#define PACKAGE_VERSION \\"1.2.20\\"
#define HAVE_UNISTD_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_FCNTL_H 1
#define HAVE_ERRNO_H 1
#define HAVE_STRING_H 1
#define HAVE_CTYPE_H 1
#define HAVE_LIBGEN_H 1
#define HAVE_SYS_SYSMACROS_H 0
#define NEED_MAKEDEV 1
#define NEED_FNMATCH 1
#define STDC_HEADERS 1
#endif
")

# Generate libtar_listhash.h from template
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/listhash/listhash.h.in LISTHASH_H_TEMPLATE)
string(REPLACE "@LISTHASH_PREFIX@" "libtar" LISTHASH_H_CONTENT "${LISTHASH_H_TEMPLATE}")
string(REPLACE "@configure_input@" "Generated from listhash.h.in for iOS" LISTHASH_H_CONTENT "${LISTHASH_H_CONTENT}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/libtar_listhash.h "${LISTHASH_H_CONTENT}")

# Generate libtar_hash.c from hash.c.in template
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/listhash/hash.c.in HASH_C_TEMPLATE)
string(REPLACE "@LISTHASH_PREFIX@" "libtar" HASH_C_CONTENT "${HASH_C_TEMPLATE}")
string(REPLACE "@configure_input@" "Generated from hash.c.in for iOS" HASH_C_CONTENT "${HASH_C_CONTENT}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/libtar_hash.c "${HASH_C_CONTENT}")

# Generate libtar_list.c from list.c.in template
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/listhash/list.c.in LIST_C_TEMPLATE)
string(REPLACE "@LISTHASH_PREFIX@" "libtar" LIST_C_CONTENT "${LIST_C_TEMPLATE}")
string(REPLACE "@configure_input@" "Generated from list.c.in for iOS" LIST_C_CONTENT "${LIST_C_CONTENT}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/libtar_list.c "${LIST_C_CONTENT}")

# Source files
set(LIBTAR_SOURCES
    lib/append.c
    lib/block.c
    lib/decode.c
    lib/encode.c
    lib/extract.c
    lib/handle.c
    lib/output.c
    lib/util.c
    lib/wrapper.c
    ${CMAKE_CURRENT_BINARY_DIR}/libtar_hash.c
    ${CMAKE_CURRENT_BINARY_DIR}/libtar_list.c
    compat/dirname.c
    compat/basename.c
    compat/fnmatch.c
)

# Create static library
add_library(tar STATIC ${LIBTAR_SOURCES})

# Include directories
target_include_directories(tar PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/listhash
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(tar PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>
    $<INSTALL_INTERFACE:include>
)

# Install library
install(TARGETS tar
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

# Install headers
install(FILES
    lib/libtar.h
    DESTINATION include
)

# Install generated header
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libtar_listhash.h
    DESTINATION include
)
"""

    cmake_file = libtar_src_dir / "CMakeLists.txt"
    with open(cmake_file, 'w') as f:
        f.write(cmake_content)
    print(f"✓ Generated CMakeLists.txt")

    # Build directory
    build_dir = ios_builds_dir / "libtar_build"
    build_dir.mkdir(parents=True, exist_ok=True)

    # Clean if requested
    if force_rebuild and build_dir.exists():
        import shutil
        print(f"Cleaning build directory: {build_dir}")
        shutil.rmtree(build_dir)
        build_dir.mkdir(parents=True, exist_ok=True)

    # Toolchain file
    prj_root = Path(os.environ["ORKID_WORKSPACE_DIR"])
    toolchain_file = prj_root / "cmake" / "toolchains" / "ios.toolchain.cmake"

    # Configure with CMake
    print(f"\nConfiguring libtar for iOS...")

    cmake_cmd = [
        "cmake",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
        f"-DCMAKE_INSTALL_PREFIX={ios_subspace}",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DIOS_SIMULATOR={'ON' if is_simulator else 'OFF'}",
        "-DARCHITECTURE=AARCH64",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
        str(libtar_src_dir)
    ]

    result = subprocess.run(cmake_cmd, cwd=build_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"✗ CMake configuration failed for libtar (return code: {result.returncode})")
        if result.stdout:
            print("STDOUT:", result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        return False
    else:
        # Always print output to see what's configured
        if result.stdout:
            print(result.stdout)

    # Build
    print(f"\nBuilding libtar...")

    build_cmd = ["cmake", "--build", str(build_dir), "--parallel", str(num_cores)]
    if verbose:
        build_cmd.append("--verbose")

    result = subprocess.run(build_cmd, capture_output=not verbose)
    if result.returncode != 0:
        print(f"✗ Build failed for libtar")
        return False

    # Install
    print(f"\nInstalling libtar...")

    install_cmd = ["cmake", "--install", str(build_dir)]
    result = subprocess.run(install_cmd, capture_output=not verbose)
    if result.returncode != 0:
        print(f"✗ Install failed for libtar")
        return False

    # Create manifest file to mark successful build
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# libtar iOS build completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {libtar_src_dir}\n")
            f.write(f"# Build: {build_dir}\n")
            f.write(f"# Install: {ios_subspace}\n")

        print(f"\n✓ libtar for iOS built successfully")
        print(f"  Library: {ios_lib_dir}/libtar.a")
        print(f"  Headers: {ios_include_dir}/libtar.h")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ libtar for iOS built successfully")
        print(f"  Library: {ios_lib_dir}/libtar.a")
        print(f"  Headers: {ios_include_dir}/libtar.h")

    return True
