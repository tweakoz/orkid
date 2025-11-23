#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

from orkengine import core
from orkengine import lev2

################################################################
# List available DRM display modes
################################################################

if __name__ == "__main__":
    lev2.printDrmMonitors()
