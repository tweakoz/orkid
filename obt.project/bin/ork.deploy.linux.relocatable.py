#!/usr/bin/env python3
###############################################################################
# ork.deploy.linux.relocatable.py
#
# Orkid-specific relocatable Linux (x86_64) deployment.
# Thin wrapper — all phase logic lives in ork.deploy_phases, which dispatches
# the relocation backend on host (ELF/patchelf on Linux, Mach-O on macOS).
#
# Phases 1-6 produce a self-contained, $ORIGIN-relocatable staging tree.
# Phases 7 (.app) / 8 (.dmg) are macOS-only and are skipped on Linux — the
# Linux packaging (tarball / AppImage / Flatpak) is produced by a separate
# driver (see ork.deploy.ix.flatpak.py).
###############################################################################

from ork.deploy_phases import run_deploy

DEPLOY_CONFIG = {
  "folder_icon_search": ["orkid/ork.data/misc/OrkidLogo.png"],
  # Retained for parity with the macOS config and for future .desktop / AppImage
  # / Flatpak entry-point generation. Phases 7/8 are skipped on Linux, so these
  # are informational for now.
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
