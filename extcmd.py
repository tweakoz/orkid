#!/usr/bin/env python3

import os, pathlib, sys

thisdir = pathlib.Path(os.path.dirname(os.path.abspath(__file__)))

cmd =  [thisdir/"ork.build"/"bin"/"init_env.py"]
cmd += ["--launch",thisdir/".stage"]
cmd += ["--command"]
cmd += ["\"%s\""%" ".join(sys.argv[1:])]

cmds = [str(s) for s in cmd]

os.system(" ".join(cmds))
