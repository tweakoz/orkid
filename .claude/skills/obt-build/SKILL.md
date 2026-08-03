---
name: obt-build
description: Answer questions about OBT (Orkid Build Tools), dependency management (obt.dep.*.py commands), pydefaults, ork.build.py, CMake structure, ork.python environment, staging directories, pyext builds, environment variables, and the ork.build git repo. Use when the user asks about building, compiling, dependencies, pip packages, CMake, or project structure.
user-invocable: false
---

# OBT Build System Reference

OBT (Orkid Build Tools) is a Python-based build/dependency management system. It lives in the `ork.build` git repo and is installed as a pip package into a Python venv.

## Repository & Installation

| Component | Path |
|-----------|------|
| ork.build repo (source) | `../ork.build` (relative to orkid workspace) |
| Installed OBT modules | `$OBT_VENV_DATA/modules/` |
| OBT Python package | `$OBT_ORIGINAL_PYTHONPATH/obt/` |
| OBT scripts (bin_pub, always on PATH) | `$OBT_VENV_DIR/bin/obt.*.py` |
| OBT scripts (bin_priv, staging-only) | `$OBT_VENV_DATA/bin_priv/` |

**To mirror live changes** from ork.build repo to the running installation:
manually copy modified files from the repo to the installed location. Do NOT use
`pip install -e .` — it has caused issues and risks breaking the twine/PyPI
deployment path that others depend on.

```bash
# Example: after editing modules/dep/pydefaults.py in ork.build repo
cp /path/to/ork.build/modules/dep/pydefaults.py $OBT_VENV_DIR/obt/modules/dep/pydefaults.py
```

| Repo path | Installed path |
|-----------|---------------|
| `ork.build/modules/dep/*.py` | `$OBT_VENV_DATA/modules/dep/` |
| `ork.build/modules/obt/*.py` | `$OBT_ORIGINAL_PYTHONPATH/obt/` |
| `ork.build/bin_priv/*.py` | `$OBT_VENV_DATA/bin_priv/` |
| `ork.build/bin_pub/*.py` | `$OBT_VENV_DATA/bin_pub/` |

**PyPI deployment** is managed via the ork.build repo's `setup.py` (package name: `ork.build`).
Do NOT break the twine path — many users depend on pip-deployed OBT.
CAUTION: a PyPI release reflects the tree it was cut from — `pip install --upgrade`
onto a seat carrying newer hand-mirrored files REGRESSES them (observed 2026-07-29:
0.0.319 reverted a seat's class-routing obt/net/client.py). Check what the release
contains before upgrading a patched seat.

## Key Files (orkid side)

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
| Env Init | `obt.project/scripts/init_env.py` (prepends `obt.project/bin/` to PATH, making all `ork.*.py` scripts available) |

## Dependency Management Commands

### obt.dep.list.py
List all available dependencies (170+ deps):
```bash
obt.dep.list.py
```

### obt.dep.info.py
Show detailed info about a dependency:
```bash
obt.dep.info.py pydefaults    # Python default packages
obt.dep.info.py python         # Python interpreter
obt.dep.info.py boost          # Boost libraries
```

### obt.dep.status.py
Check if a dependency is built (manifest, source, binaries):
```bash
obt.dep.status.py pydefaults
```

### obt.dep.build.py
Build a dependency:
```bash
obt.dep.build.py pydefaults              # Normal build
obt.dep.build.py pydefaults --force       # Force rebuild
obt.dep.build.py pydefaults --wipe        # Wipe and rebuild
obt.dep.build.py boost --serial           # Single-threaded
obt.dep.build.py llvm --verbose --debug   # Verbose debug build
obt.dep.build.py cmake --incremental      # Incremental build
obt.dep.build.py python --nofetch         # Build without re-fetching source
```

### obt.dep.findtext.py
Search text within a dependency's source tree:
```bash
obt.dep.findtext.py boost "shared_mutex"
obt.dep.findtext.py llvm "LLVMContext" --regex --context 3
```

### obt.dep.shell.py
Enter interactive build shell for a dependency:
```bash
obt.dep.shell.py boost
```

### obt.versions.py
Show comprehensive version/environment info:
```bash
obt.versions.py    # Python paths, OBT version, git repo states
```

## Dependency Provider Pattern

Each dep is a Python class in `modules/dep/<name>.py`:

```python
from obt import dep, pip, path, host
from obt.command import Command

class mydep(dep.Provider):
    def __init__(self):
        super().__init__("mydep")
        self.build_dest = path.builds()/"mydep"
        self.python = self.declareDep("python")  # declare dependency

    def build(self):
        pip.install(["package1", "package2"])
        ret = Command([self.python.executable, "-m", "pip", "install", "pkg"]).exec()
        return (ret == 0)

    def areRequiredSourceFilesPresent(self):
        return (self.python.site_packages_dir/"package1").exists()

    def areRequiredBinaryFilesPresent(self):
        return self.areRequiredSourceFilesPresent()
```

**Key concepts:**
- `declareDep("name")` — declares a build dependency (resolved topologically)
- `path.builds()` — `$OBT_STAGE/builds/`
- `path.stage()` — `$OBT_STAGE/`
- `path.manifests()` — `$OBT_STAGE/manifests/`
- `pip.install([...])` — pip install wrapper
- `Command([...]).exec()` — run shell command, returns exit code
- `host.IsDarwin` / `host.IsLinux` / `host.IsOsx` / `host.IsIx` — platform detection
- `self.shlib_extension` — `"dylib"` on macOS, `"so"` on Linux (use in `areRequiredBinaryFilesPresent`)
- Build scopes: CONTAINER, INIT, HOST, SUBSPACE

**Module search path** (`$OBT_MODULES_PATH`):
1. `$OBT_VENV_DIR/obt/modules/` (OBT core)
2. `orkid/obt.project/modules/` (orkid overrides)
3. `impcore/obt.project/modules/` (project overrides)

## pydefaults — Python Default Packages

Source: `ork.build/modules/dep/pydefaults.py`

Installs core scientific Python packages plus playwright:
- pytest, numpy, scipy, numba, pyopencl, matplotlib, pyzmq, opencv-python
- Pillow, jupyter, plotly, trimesh, asciidoc, pyudev, playwright
- pysqlite3 (Linux only)
- Post-install: `playwright install chromium` (downloads Chromium browser)

## orkid Build Commands

```bash
ork.build.py              # Parallel build (default)
ork.build.py --serial     # Single-threaded build (-j1)
ork.build.py --clean      # Clean build
ork.build.py --debug      # Debug build
ork.build.py --verbose    # Verbose make output
ork.build.py --sanitize address  # AddressSanitizer
ork.build.py --sanitize thread   # ThreadSanitizer
ork.build.py --xcode      # Generate Xcode project
ork.build.py --cmakeenv   # Display cmake flags and exit
```

## Module Build Order

```
ork.utpp -> ork.core -> ork.lev2 -> ork.eda -> ork.ecs -> ork.ftxui
```

## Source File Discovery

**Automatic via GLOB_RECURSE** — no need to manually list new .cpp files:
```cmake
file(GLOB_RECURSE src_math ${SRCD}/math/*.cpp)
```

## Module Structure

```
ork.MODULE/
  CMakeLists.txt       # Module build config
  inc/                 # Public headers
  src/                 # Implementation
  pyext/               # Python extension
    CMakeLists.txt
    *.cpp              # Binding source files
    pyfiles/           # __init__.py templates
```

## Python Extension Build

Extensions built as shared libraries, installed to `$OBT_PYPKG/orkengine/`:
```
$OBT_PYPKG/orkengine/
  core/_core.so    # ork.core bindings
  lev2/_lev2.so    # ork.lev2 bindings
  ecs/_ecs.so      # ork.ecs bindings
```

Import: `from orkengine import core, lev2`

## ork.python

Symlink to Python in OBT's isolated venv. Includes torch, numpy, and orkengine modules.
```bash
ork.python script.py           # Run with orkid environment
#!/usr/bin/env ork.python      # Shebang for orkid scripts
```

## Key Environment Variables

| Variable | Purpose |
|----------|---------|
| `OBT_STAGE` | Staging/install directory |
| `OBT_VENV_DIR` | Python venv root |
| `OBT_PYTHONHOME` | Python installation root |
| `OBT_PYPKG` | Python site-packages directory |
| `OBT_MODULES_PATH` | Dep module search path (colon-separated) |
| `OBT_DEP_PATH` | Dependency definitions search path |
| `OBT_BUILDS` | Build artifacts directory |
| `OBT_ROOT` | OBT root data directory |
| `ORKID_WORKSPACE_DIR` | orkid project root |
| `ORKID_GRAPHICS_API` | `VULKAN` or `OPENGL` |
| `ORKID_ASSET_MANIFEST_DIRS` | Colon-separated manifest dirs |

## Staging Directory

```
$OBT_STAGE/
  lib/           # Shared libraries
  bin/           # Executables
  include/       # Headers
  builds/        # Build artifacts per dep
  manifests/     # Dep build manifests
  pyvenv/        # Python venv for orkid runtime
  assetcache/    # Cached assets
```

## OBT Python API (key modules)

| Module | Purpose |
|--------|---------|
| `obt.dep` | Dependency management (Provider, Chain, DepNode) |
| `obt.pip` | pip install/uninstall wrapper |
| `obt.path` | Path utilities (stage(), builds(), bin(), libs()) |
| `obt.command` | Command execution with env/logging |
| `obt.host` | Host OS/arch detection (IsDarwin, IsLinux) |
| `obt.cmake` | CMake integration |
| `obt.git` | Git operations |
| `obt.manifest` | Build manifest files |
| `obt.deco` | Terminal color/decoration |

## How to Answer

1. For adding pip packages: edit `ork.build/modules/dep/pydefaults.py`, add to pip list
2. For new deps: create `modules/dep/<name>.py` with Provider subclass
3. For build issues: check `ork.build.py` args and CMake output
4. For new source files: just add to the right directory — GLOB_RECURSE picks them up
5. For pyext: check the module's `pyext/CMakeLists.txt`
6. For env vars: check `init_env.py` and `obt.versions.py`
7. Never suggest manual CMakeLists.txt edits for adding source files
8. To mirror live dep module changes: edit in the ork.build repo, then MANUALLY COPY to
   the installed location per Repository & Installation above — never `pip install -e .`
