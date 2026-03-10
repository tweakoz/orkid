# Deployment System - Usage Guide

## Quick Start

### Full deployment

```bash
my_project.deploy.macos.relocatable.py --target ~/Desktop/MyDeploy
```

### Re-run a single phase (on existing deployment)

```bash
my_project.deploy.macos.relocatable.py --target ~/Desktop/MyDeploy --phase 7
```

### Force overwrite

```bash
my_project.deploy.macos.relocatable.py --target ~/Desktop/MyDeploy --force
```

## Writing a Top-Level Deploy Script

Each product creates a thin wrapper in its `obt.project/bin/` directory:

```python
#!/usr/bin/env python3

import os
from ork.deploy_phases import run_deploy

_HERE = os.path.dirname(os.path.abspath(__file__))
_ICONS = os.path.join(_HERE, "..", "icons")

DEPLOY_CONFIG = {
    "folder_icon": os.path.join(_ICONS, "my_folder_icon.png"),
    "apps": [
        {
            "name": "MyApp",
            "command": ["my_launcher.py"],
            "mode": "gui",
            "bundle_id": "com.example.myapp",
            "icon": os.path.join(_ICONS, "my_app_icon.png"),
        },
        {
            "name": "LaunchShell",
            "command": [],
            "mode": "terminal",
            "bundle_id": "com.example.myapp.shell",
        },
    ],
}

if __name__ == "__main__":
    run_deploy(DEPLOY_CONFIG)
```

Make the script executable (`chmod +x`).

### App Spec Fields

| Field | Required | Description |
|-------|----------|-------------|
| `name` | yes | Name of the `.app` bundle (e.g., `"MyApp"` creates `MyApp.app`) |
| `command` | yes | Command to run. Empty list `[]` for terminal mode (opens shell). |
| `mode` | yes | `"gui"` (suppress terminal) or `"terminal"` (open Terminal.app) |
| `bundle_id` | yes | macOS bundle identifier (e.g., `"com.example.myapp"`) |
| `icon` | no | Path to PNG icon. Converted to `.icns` automatically. |

### Folder Icon

Set via either:
- `"folder_icon": "/absolute/path/to/icon.png"` — direct path
- `"folder_icon_search": ["relative/path/in/projects/icon.png"]` — searched in deployed projects directory

## Writing a deployment_manifest.py

Each project that contributes files to a deployment places this in `<project>/obt.project/deployment_manifest.py`:

```python
manifest = {
    # Directories to copy from project root into the deployment
    "dirs": [
        "obt.project",          # Always include this
        "data",
        "shaders",
    ],

    # Directories to copy if they exist (no error if missing)
    "optional_dirs": [
        "optional_data",
    ],

    # Individual files to copy
    "files": [
        "config.cfg",
        "some/nested/file.py",
    ],

    # Vendor dylibs to install into .staging/lib/ and relocate
    "deploy_libs": [
        "obt.project/modules/_sdk/mac/somevendor/lib/libfoo.dylib",
    ],

    # Environment variables to capture in env.common.sh
    # Values are read from the build host at deploy time
    "environment": [
        "MY_API_KEY",
        "MY_SERVICE_URL",
    ],
}
```

If no `deployment_manifest.py` exists, the default is to copy only `obt.project/`.

### deploy_fixup() Hook

The manifest module can optionally define a `deploy_fixup` function for project-specific post-processing:

```python
def deploy_fixup(infra_dir, proj_root, proj_target):
    """Run after project files are copied into the deployment."""
    # Example: copy model files from an external dependency
    import importlib.metadata, json, urllib.parse
    dist = importlib.metadata.distribution("my_package")
    url_file = Path(dist._path) / "direct_url.json"
    if url_file.exists():
        url_json = json.loads(url_file.read_text())
        src = Path(urllib.parse.urlparse(url_json["url"]).path)
        # copy assets into <staging>/share/
        ...

    # Example: patch a deployed script to use deployment paths
    script = proj_target / "scripts/my_utils.py"
    text = script.read_text()
    text = text.replace('old_hardcoded_path', 'new_deploy_path')
    script.write_text(text)
```

This runs during Phase 5, after file copies and text reference fixups. It receives:
- `infra_dir` — the `.staging/` directory
- `proj_root` — original source project directory
- `proj_target` — deployed copy under `.staging/projects/<name>/`

### What Each Key Does

- **`dirs`** — Copied recursively with `cp -a` (preserves symlinks, permissions)
- **`optional_dirs`** — Same as `dirs` but silently skipped if the source directory doesn't exist
- **`files`** — Individual files copied with directory structure preserved
- **`deploy_libs`** — Dylibs copied into `.staging/lib/`, then Mach-O relocated and re-signed
- **`environment`** — Variable names whose current values are written to `env.common.sh`. The launch environment sources this file, making these values available at runtime.

## CLI Reference

```
usage: deploy_script.py [-h] --target TARGET [--staging STAGING]
                        [--phase {1,2,3,4,5,5.5,6,7,8,all}]
                        [--force] [--homebrew HOMEBREW]
                        [--obt-venv OBT_VENV] [--project PROJECT]
                        [--walk-only] [--verify-only]

Options:
  --target TARGET       Deployment output directory (required)
  --staging STAGING     Source staging directory (default: $OBT_STAGE)
  --phase PHASE         Run a single phase instead of all (default: all)
  --force               Remove existing target before Phase 1 copy
  --homebrew PATH       Homebrew prefix (default: /opt/homebrew)
  --obt-venv PATH       OBT framework venv path (default: $VIRTUAL_ENV)
  --project DIR         Project to include (repeatable; default: $OBT_PROJECT_DIRS)
  --walk-only           Print dependency manifest without deploying
  --verify-only         Verify an existing deployment's Mach-O references
```

## Running Individual Phases

Phases can be re-run independently against an existing deployment. This is useful during development or when debugging a specific phase.

```bash
# Re-run only Mach-O relocation
deploy_script.py --target ~/Desktop/MyDeploy --phase 2

# Re-run only verification
deploy_script.py --target ~/Desktop/MyDeploy --phase 3

# Re-generate app bundles after changing deploy_config
deploy_script.py --target ~/Desktop/MyDeploy --phase 7
```

**Phase dependencies:**
- Phase 1 must run first (creates the target directory)
- Phases 2-8 expect Phase 1's output to exist
- Phase 4 requires `--obt-venv`
- Phase 5 requires `--project`

## Deploy Log

Every deployment produces `.staging/deploy.log` containing the full output from all phases. This is useful for diagnosing issues on target machines where the deploy was run.

## Post-Deploy: Asset Fetching

Asset content is not packaged in the deployment. On first launch, the `obt-launch-env` script creates a symlink from `<staging>/assetcache` to `~/.obt-global/assetcache` (creating the global directory if needed). This means assets fetched once are shared across all deployments on that machine. Fetch assets on the target machine using the asset catalog system.

## Troubleshooting

### "Target already exists" error
Use `--force` to overwrite, or choose a different `--target` path.

### Verification failures
Phase 3 reports unresolvable Mach-O references. Common causes:
- Vendor SDK dylibs not listed in `deploy_libs`
- Framework dependencies that aren't internalized
- Most failures are non-critical; the deploy continues with a warning.

### Missing environment variables
If a variable listed in `environment` is not set on the build host at deploy time, it will be absent from `env.common.sh`. Set it before deploying, or edit `env.common.sh` on the target machine.
