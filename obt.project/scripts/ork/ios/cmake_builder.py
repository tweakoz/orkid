################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS CMake Builder
# Generic module for building iOS libraries from source
################################################################

import os
import subprocess
import glob
from pathlib import Path

def generate_cmake_for_ios_library(
    library_name,
    source_dir,
    source_patterns=None,
    include_dirs=None,
    defines=None,
    compile_options=None,
    public_headers=None,
    library_type="SHARED",
    frameworks=None
):
    """Generate a CMakeLists.txt for building an iOS library

    Args:
        library_name: Name of the library (e.g., "lz4")
        source_dir: Directory containing source files
        source_patterns: List of glob patterns for source files (e.g., ["*.c", "*.cpp"])
        include_dirs: List of include directories
        defines: List of preprocessor defines
        compile_options: List of compiler options
        public_headers: List of header files to install
        library_type: "SHARED" or "STATIC"
        frameworks: List of iOS frameworks to link (e.g., ["Foundation"])

    Returns:
        String containing CMakeLists.txt content
    """
    source_dir = Path(source_dir)

    if source_patterns is None:
        source_patterns = ["*.c", "*.cpp", "*.cc", "*.cxx"]
    if include_dirs is None:
        include_dirs = []
    if defines is None:
        defines = []
    if compile_options is None:
        compile_options = []
    if public_headers is None:
        public_headers = []
    if frameworks is None:
        frameworks = []

    # Find source files (absolute paths since they're in host staging)
    sources = []
    for pattern in source_patterns:
        sources.extend(glob.glob(str(source_dir / pattern)))

    # Use absolute paths for sources since they're in a different directory
    sources_abs = [Path(s).resolve() for s in sources]

    cmake_content = f"""cmake_minimum_required(VERSION 3.20)
project({library_name} C CXX)

# Source files (absolute paths to host staging)
set(SOURCES
"""
    for src in sources_abs:
        cmake_content += f"    {src}\n"

    cmake_content += f""")

# Create library
add_library({library_name} {library_type} ${{SOURCES}})

# Include directories
"""
    if include_dirs:
        cmake_content += "target_include_directories(" + library_name + " PUBLIC\n"
        for inc_dir in include_dirs:
            inc_path = Path(inc_dir).resolve()
            cmake_content += f"    {inc_path}\n"
        cmake_content += ")\n\n"

    # Defines
    if defines:
        cmake_content += "target_compile_definitions(" + library_name + " PRIVATE\n"
        for define in defines:
            cmake_content += f"    {define}\n"
        cmake_content += ")\n\n"

    # Compile options
    if compile_options:
        cmake_content += "target_compile_options(" + library_name + " PRIVATE\n"
        for opt in compile_options:
            cmake_content += f"    {opt}\n"
        cmake_content += ")\n\n"

    # iOS frameworks
    if frameworks:
        cmake_content += "target_link_libraries(" + library_name + " PRIVATE\n"
        for fw in frameworks:
            cmake_content += f"    \"-framework {fw}\"\n"
        cmake_content += ")\n\n"

    # Install rules
    cmake_content += f"""
# Install library
install(TARGETS {library_name}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

"""

    # Install headers (use absolute paths from source_dir)
    if public_headers:
        cmake_content += "# Install headers\n"
        for header in public_headers:
            header_path = (source_dir / header).resolve()
            cmake_content += f"install(FILES {header_path} DESTINATION include)\n"

    return cmake_content


def build_ios_library(
    library_name,
    source_dir,
    build_dir,
    install_prefix,
    is_simulator,
    toolchain_file,
    cmake_content=None,
    num_cores=8,
    verbose=False,
    clean=False
):
    """Build an iOS library using CMake

    Args:
        library_name: Name of the library
        source_dir: Directory containing source files and CMakeLists.txt
        build_dir: Build output directory
        install_prefix: Installation prefix
        is_simulator: True for simulator, False for device
        toolchain_file: Path to iOS CMake toolchain file
        cmake_content: Optional CMakeLists.txt content (will be written to source_dir)
        num_cores: Number of parallel build jobs
        verbose: Enable verbose output
        clean: Clean build directory before building

    Returns:
        True if successful, False otherwise
    """
    source_dir = Path(source_dir)
    build_dir = Path(build_dir)
    install_prefix = Path(install_prefix)
    toolchain_file = Path(toolchain_file)

    # Clean if requested (before creating new content)
    if clean and build_dir.exists():
        import shutil
        print(f"Cleaning build directory: {build_dir}")
        shutil.rmtree(build_dir)

    # Create build directory
    build_dir.mkdir(parents=True, exist_ok=True)

    # Write CMakeLists.txt to build directory if provided
    if cmake_content:
        cmake_path = build_dir / "CMakeLists.txt"
        print(f"Generating CMakeLists.txt at {cmake_path}")
        with open(cmake_path, 'w') as f:
            f.write(cmake_content)

    # Configure with CMake
    print(f"\nConfiguring {library_name} for iOS...")

    cmake_cmd = [
        "cmake",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
        f"-DCMAKE_INSTALL_PREFIX={install_prefix}",
        "-DBUILD_IOS_MINIMAL=ON",
        "-DIOS_BUILD=ON",
        f"-DIOS_SIMULATOR={'ON' if is_simulator else 'OFF'}",
        "-DARCHITECTURE=AARCH64",
        "-DCMAKE_BUILD_TYPE=Release",
        "."  # CMakeLists.txt is in build_dir
    ]

    result = subprocess.run(cmake_cmd, cwd=build_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"✗ CMake configuration failed for {library_name} (return code: {result.returncode})")
        if result.stdout:
            print("STDOUT:", result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        return False
    else:
        # Print output on success too
        if result.stdout:
            print(result.stdout)

    # Build
    print(f"\nBuilding {library_name}...")

    build_cmd = ["cmake", "--build", str(build_dir), "--parallel", str(num_cores)]
    if verbose:
        build_cmd.append("--verbose")

    result = subprocess.run(build_cmd)
    if result.returncode != 0:
        print(f"✗ Build failed for {library_name}")
        return False

    # Install
    print(f"\nInstalling {library_name}...")

    install_cmd = ["cmake", "--install", str(build_dir)]
    result = subprocess.run(install_cmd)
    if result.returncode != 0:
        print(f"✗ Install failed for {library_name}")
        return False

    print(f"✓ {library_name} built and installed successfully")
    return True
