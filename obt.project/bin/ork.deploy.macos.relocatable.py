#!/usr/bin/env python3
###############################################################################
# ork.deploy.macos.relocatable.py
#
# Orkid-specific relocatable macOS deployment.
# Thin wrapper — all phase logic lives in ork.deploy_phases.
###############################################################################

from ork.deploy_phases import run_deploy

DEPLOY_CONFIG = {
  "folder_icon_search": ["orkid/ork.data/misc/OrkidLogo.png"],
  "apps": [
    {
      "name": "LaunchShell",
      "command": [],
      "mode": "terminal",
      "bundle_id": "com.tweakoz.obt.launchshell",
    },
    {
      "name": "OrkTestRunner",
      "command": ["ork.app.testrunner.py"],
      "mode": "gui",
      "bundle_id": "com.tweakoz.orkid.testrunner",
    },
  ],
}

if __name__ == "__main__":
  run_deploy(DEPLOY_CONFIG)
