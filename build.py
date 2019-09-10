#!/usr/bin/env python3

import os
import ork.host
from ork.path import Path
from ork.command import Command

print(ork.host.SYSTEM)

build_dest = ork.path.stage()/"orkid"
build_dest.mkdir(parents=True,exist_ok=True)
build_dest.chdir()

prj_root = Path(os.environ["ORKID_WORKSPACE_DIR"])
cmd = ["cmake",prj_root]

Command(cmd).exec()
Command(["make"]).exec()
