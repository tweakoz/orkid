---
name: obt-build
description: Answer questions about orkid's OBT build system, ork.build.py, CMake structure, module layout, ork.python environment, staging directories, pyext builds, and environment variables. Use when the user asks about building, compiling, CMake, or project structure.
user-invocable: false
---

# OBT Build System Reference

When answering questions about building orkid, consult these files.

## Key Files

| Component | Location |
|-----------|----------|
| Build Script | `obt.project/bin/ork.build.py` |
| Root CMake | `CMakeLists.txt` |
| Orkid CMake Macros | `orkid.cmake` |
| Core CMake | `ork.core/CMakeLists.txt` |
| Lev2 CMake | `ork.lev2/CMakeLists.txt` |
| ECS CMake | `ork.ecs/CMakeLists.txt` |
| Core Pyext CMake | `ork.core/pyext/CMakeLists.txt` |
| Lev2 Pyext CMake | `ork.lev2/pyext/CMakeLists.txt` |
| Env Init | `obt.project/scripts/init_env.py` |

## Build Commands

```bash
ork.build.py              # Parallel build (default)
ork.build.py --serial     # Single-threaded build (-j1)
ork.build.py --clean      # Clean build
ork.build.py --debug      # Debug build
ork.build.py --verbose    # Verbose make output
ork.build.py --sanitize address  # AddressSanitizer
ork.build.py --sanitize thread  # ThreadSanitizer
ork.build.py --xcode      # Generate Xcode project
ork.build.py --cmakeenv   # Display cmake flags and exit
```

## Module Build Order

```
ork.utpp → ork.core → ork.lev2 → ork.eda → ork.ecs → ork.ftxui
```

Each module: `add_subdirectory()` in root CMakeLists.txt.

## Source File Discovery

**Automatic via GLOB_RECURSE** — no need to manually list new .cpp files:
```cmake
file(GLOB_RECURSE src_math ${SRCD}/math/*.cpp)
file(GLOB_RECURSE src_kernel ${SRCD}/kernel/*.cpp)
# ... combined into target sources
```

## Module Structure

```
ork.MODULE/
├── CMakeLists.txt       # Module build config
├── inc/                 # Public headers
├── src/                 # Implementation
└── pyext/               # Python extension (if applicable)
    ├── CMakeLists.txt
    ├── *.cpp            # Binding source files
    └── pyfiles/         # __init__.py templates
```

## Python Extension Build

Extensions built as shared libraries, installed to `$OBT_PYPKG/orkengine/`:

```
$OBT_PYPKG/orkengine/
├── __init__.py
├── core/
│   ├── __init__.py
│   └── _core.so       # ork.core bindings
├── lev2/
│   ├── __init__.py
│   └── _lev2.so       # ork.lev2 bindings
└── ecs/
    ├── __init__.py
    └── _ecs.so         # ork.ecs bindings
```

Import: `from orkengine import core, lev2`

## ork.python

Symlink to Python 3.12 in OBT's isolated venv. Includes torch, numpy, and orkengine modules.

```bash
ork.python script.py     # Run with orkid environment
#!/usr/bin/env ork.python # Shebang for orkid scripts
```

## Key Environment Variables

| Variable | Purpose |
|----------|---------|
| `OBT_STAGE` | Staging/install directory |
| `OBT_SUBSPACE_LIB_DIR` | Library install path |
| `OBT_PYPKG` | Python packages directory |
| `OBT_PYTHONHOME` | Python installation root |
| `ORKID_WORKSPACE_DIR` | Project root |
| `ORKID_GRAPHICS_API` | `VULKAN` or `OPENGL` |
| `ORKID_ASSET_MANIFEST_DIRS` | Colon-separated manifest dirs |
| `ASSETCACHE` | Asset cache directory |

## Staging Directory

```
$OBT_STAGE/
├── lib/           # Shared libraries
├── bin/           # Executables, ork.python
├── include/       # Headers
├── assetcache/    # Cached assets
└── build/         # Build artifacts
```

## PATH Setup

`obt.project/scripts/init_env.py` prepends `obt.project/bin/` to PATH, making all `ork.*.py` scripts available.

## How to Answer

1. For build issues: check `ork.build.py` arguments and CMake output
2. For new files: just add to the right directory — GLOB_RECURSE picks them up
3. For pyext: check the module's `pyext/CMakeLists.txt`
4. For env vars: check `init_env.py` and `application.cpp`
5. Never suggest manual CMakeLists.txt edits for adding source files
