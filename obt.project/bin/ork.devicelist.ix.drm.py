#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

from obt import host
import obt.deco

deco = obt.deco.Deco()

################################################################
# List available DRM display modes (Linux only)
################################################################

if __name__ == "__main__":
    if not host.IsLinux:
        print(deco.red("DRM display modes not supported on this platform (Linux only)"))
    else:
        from orkengine import core
        from orkengine import lev2
        print(deco.yellow("Available DRM Display Modes") + "\n")
        lev2.printDrmMonitors()
