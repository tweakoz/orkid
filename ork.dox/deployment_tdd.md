# Deployment System - Technical Design Document

## Overview

Orkid provides a phased, script-driven deployment system for creating relocatable macOS application bundles. The system takes a development staging directory (containing compiled binaries, Python environments, shared libraries, and assets) and produces a self-contained deployment that can run on any macOS machine without requiring the original build environment.

Extension projects (games, tools, etc.) integrate by providing a thin top-level deploy script and a `deployment_manifest.py` declaring their project-specific files.

## Architecture

### Two Configuration Sources

The system uses two distinct configuration mechanisms:

1. **`deploy_config`** — A Python dict passed directly to `run_deploy()` by the top-level deploy script. Controls Phase 7 (app bundle generation): which `.app` bundles to create, their icons, bundle IDs, and launch commands.

2. **`deployment_manifest.py`** — A per-project Python module in `<project>/obt.project/`. Declares which files, directories, and libraries from that project's source tree should be included in the deployment. Consumed by Phase 4 (environment variables) and Phase 5 (project file copy).

These are intentionally separate: the manifest describes *what content* a project contributes, while deploy_config describes *how the deployment presents itself* to the user.

### Deployed Directory Layout

```
<target>/
  .staging/                     # All runtime infrastructure (hidden)
    bin/                        # Executables
    lib/                        # Shared libraries (including internalized homebrew dylibs)
    pyvenv/                     # Staging Python environment
    share/                      # Shared data files
    obt_venv/                   # OBT framework Python venv
    projects/                   # Deployed project files
      <project_name>/
        obt.project/
        ...
    obt_config/
      env.common.sh             # Machine-specific environment variables
    assetcache -> ~/.obt-global/assetcache  # Symlink created at launch time
    dblockcache/                # Empty, created at deploy time
    obt-launch-env              # Entry point bootstrap script
    .deploy_path                # Relocation detection marker
    .is_deploy                  # Deploy mode flag
    deploy.log                  # Full output log from deployment
  App1.app/                     # Visible app bundles (macOS .app)
  App2.app/
```

## Phases

### Phase 1: Deep Copy + Internalize Homebrew

**Function:** `phase1_copy(staging_dir, target_dir, force=False)`

Copies runtime-essential directories from the staging area and internalizes the homebrew dylib closure.

**Steps:**
1. Walk staging with `MachoDependencyWalker` to discover the full homebrew dylib dependency closure
2. Copy `RUNTIME_DIRS` (`bin`, `lib`, `pyvenv`, `share`) preserving symlinks and permissions
3. Create an empty `dblockcache/` directory (assetcache is symlinked at launch time — see Phase 6)
4. Copy all discovered homebrew dylibs into `target/lib/`
5. Ensure `libpython` is present in `lib/` (resolving symlinks)
6. Fix hardcoded staging paths in text files (configs, scripts, pkg-config)

**Safety:** Refuses to overwrite an existing target unless `--force` is specified.

### Phase 2: Mach-O Relocation

**Function:** `phase2_relocate(target_dir, old_staging_dir, homebrew_dir)`

Rewrites all Mach-O load commands for `@rpath`-based relocation using `MachoRelocator`.

**Steps:**
1. Rewrite install names, rpaths, and dependency paths across all binaries
2. Create symlink farms for short rpath reach (solves CPython `lib-dynload` padding constraints)
3. Re-sign all modified binaries with ad-hoc signatures

### Phase 3: Verification

**Function:** `phase3_verify(target_dir)`

Validates that all Mach-O references resolve correctly using `MachoVerifier`. Reports pass/fail/warn counts. Non-critical failures (e.g., optional frameworks) do not halt the deploy.

### Phase 4: OBT Venv

**Function:** `phase4_obt_venv(target_dir, obt_venv_dir, homebrew_dir, deploy_manifests)`

Bundles the OBT framework's Python virtual environment (separate from the staging `pyvenv`).

**Steps:**
1. Deep copy the OBT venv
2. Replace the homebrew Python symlink with the real binary
3. Internalize the Python framework dylib and stdlib
4. Create `Python.app` stub structure (required by framework-built Python)
5. Relocate all Mach-O binaries within the venv
6. Fix text references (pyvenv.cfg, shebangs)
7. Generate `env.common.sh` from all project manifests' `environment` declarations

### Phase 5: Project Runtime Files

**Function:** `phase5_projects(target_dir, project_dirs)`

Copies project-specific runtime files as declared by each project's `deployment_manifest.py`.

**Manifest keys consumed:**
- `dirs` — Required directories to copy
- `optional_dirs` — Directories to copy if present (no error if missing)
- `files` — Individual files to copy
- `deploy_libs` — Dylibs to copy into `lib/` and relocate

After copying and text fixups, Phase 5 calls the manifest module's `deploy_fixup(infra_dir, proj_root, proj_target)` function if one is defined. This allows projects to run arbitrary Python code to copy additional files, patch deployed scripts, or perform other project-specific post-processing without modifying the core deployment phases.

Also writes `projects/manifest.json` recording what was deployed.

### Phase 5.5: Dependency Module Fixups

**Function:** `phase5_5_dep_fixups(target_dir)`

Iterates all OBT dependency modules and calls `deployment_fixup(target_dir)` on any module that provides it. This allows per-dependency custom post-processing.

### Phase 6: Launch Script + Shebang Fixup

**Function:** `phase6_launch_script(target_dir)`

Creates the self-locating entry point and performs final setup.

**Steps:**
1. Generate `obt-launch-env` — a bash script that bootstraps the deployment environment, clears host variables, and detects relocation. Also creates a symlink from `$DEPLOY_ROOT/assetcache` to `~/.obt-global/assetcache` on first launch (ensuring the global cache directory exists), so asset data is shared across deployments and persists per-user.
2. Internalize host binaries (`pkg-config`, `rsvg-convert`) with their dylib closures
3. Set up MoltenVK ICD configuration for Vulkan support
4. Fix Python shebangs in `obt_venv/bin/` to use `#!/usr/bin/env python3`
5. Write `.deploy_path` marker for relocation detection
6. Write `.is_deploy` mode flag

The launch script implements runtime relocation detection: if the deployment has been moved since last launch, it rewrites `.cfg` files, shebangs, `pkg-config` files, and CMake configs to match the new location.

### Phase 7: App Bundle Generation

**Function:** `phase7_app_bundles(infra_dir, visible_dir, deploy_config)`

Generates macOS `.app` bundles from the `deploy_config` dict.

**Steps:**
1. Convert folder icon PNG to `.icns` format
2. Generate `.app` bundles for each entry in `deploy_config["apps"]`
3. Apply folder icon to the visible deployment directory

**App modes:**
- `gui` — Launches via the OBT environment, suppresses terminal
- `terminal` — Opens Terminal.app with the OBT environment sourced

### Phase 8: Archive

**Function:** `phase8_archive(target_dir)`

Creates a compressed `.dmg` disk image.

**Steps:**
1. Create a sparse HFS+ image with 20% headroom
2. Mount and copy with `ditto` (preserves resource forks, xattrs, folder icons)
3. Convert to compressed read-only UDZO format

## Output Logging

All `print()` output during deployment is captured via `_TeeWriter`, a stream multiplexer that writes to both stdout and a temporary log file. The log is moved into `.staging/deploy.log` upon completion, ensuring all phases (including Phase 1) are captured.

## deployment_manifest.py Schema

Each extension project may provide `<project>/obt.project/deployment_manifest.py`:

```python
manifest = {
    "dirs": [                    # Directories to copy (required)
        "obt.project",
    ],
    "optional_dirs": [           # Directories to copy if present
        "data",
    ],
    "files": [                   # Individual files to copy
        "config.cfg",
    ],
    "deploy_libs": [             # Dylibs to install into lib/
        "obt.project/modules/_sdk/mac/somelib/lib/libfoo.dylib",
    ],
    "environment": [             # Env vars to include in env.common.sh
        "MY_API_KEY",
    ],
}
```

If no manifest exists, the default is `{"dirs": ["obt.project"], "optional_dirs": []}`.

### deploy_fixup() Hook

The manifest module may also define a `deploy_fixup(infra_dir, proj_root, proj_target)` function. This runs after Phase 5 copies the project files and fixes text references. It receives:

- `infra_dir` — the `.staging/` directory (for copying data into `share/`, `lib/`, etc.)
- `proj_root` — the original source project directory
- `proj_target` — the deployed copy under `.staging/projects/<name>/`

Use cases include copying model/data files from external repositories, patching hardcoded paths in deployed scripts, or any project-specific post-processing. The fixup can use pip install metadata (`importlib.metadata`) to dynamically locate dependencies and their assets.

## Top-Level Deploy Script Pattern

Extension projects provide a thin script that defines `deploy_config` and calls `run_deploy()`:

```python
#!/usr/bin/env python3

from ork.deploy_phases import run_deploy

DEPLOY_CONFIG = {
    "folder_icon_search": ["my_project/icons/folder.png"],
    "apps": [
        {
            "name": "MyApp",
            "command": ["my_app_launcher.py"],
            "mode": "gui",
            "bundle_id": "com.example.myapp",
            "icon": "/path/to/icon.png",  # optional
        },
    ],
}

if __name__ == "__main__":
    run_deploy(DEPLOY_CONFIG)
```

## CLI Arguments

```
--target TARGET       (required) Deployment output directory
--staging PATH        Source staging directory (default: $OBT_STAGE)
--phase PHASE         Run specific phase: 1|2|3|4|5|5.5|6|7|8|all (default: all)
--force               Remove existing target before copying
--homebrew PATH       Homebrew directory (default: /opt/homebrew)
--obt-venv PATH       OBT framework venv (default: $VIRTUAL_ENV)
--project DIR         Project directory to include (repeatable; default: $OBT_PROJECT_DIRS)
--walk-only           Only run dependency walker and print manifest
--verify-only         Only verify an existing deployment
```
