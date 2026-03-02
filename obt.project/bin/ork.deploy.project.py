#!/usr/bin/env python3
"""Deploy manifest for orkid.

Outputs JSON describing what files/dirs from the source tree
should be included in a relocatable deployment.

The init_env.py references (relative to project root):
  - obt.project/ (bin, scripts, modules, template)
  - ork.data/ (asset manifests, test data, grammars, sounds, etc.)
  - ork.core/, ork.lev2/, ork.ecs/, ork.eda/, ork.ftxui/ (for OBT_SEARCH_PATH)
  - ork.dox/ (for OBT_SEARCH_PATH)

ork.data/ is large (~3GB) but needed for asset manifests and runtime data.
Source dirs (ork.core/ etc.) are only needed for OBT_SEARCH_PATH to find
scripts/shaders — the compiled binaries are already in staging.
"""

import json

manifest = {
    "dirs": [
        "obt.project",
        "ork.core/pyext/tests",
        "ork.lev2/examples/python",
        "ork.lev2/pyext/tests",
        "ork.ecs/examples/python",
    ],
    "optional_dirs": [
        "ork.data",
    ],
    "files": [
        "orkid.cmake",
    ],
    "apps": [
        {
            "name": "OrkTestRunner",
            "command": ["ork.app.testrunner.py"],
            "mode": "gui",
            "bundle_id": "com.tweakoz.orkid.testrunner",
        },
    ],
}

print(json.dumps(manifest))
