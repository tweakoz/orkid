#!/usr/bin/env python3

from obt import command, path
from obt import env
import os

nsdir = path.Path("/opt")/"nvidia"/"nsight-graphics-for-linux"/"nsight-graphics-for-linux-2023.3.0.0"/"host"/"linux-desktop-nomad-x64"

env.set("NSIGHT_DIR",nsdir)
env.append("LD_LIBRARY_PATH",nsdir)
env.append("PATH",nsdir)

command.run(["ngfx.bin","--help"])
#