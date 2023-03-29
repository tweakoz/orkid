#!/usr/bin/env python3

import sys
import os, argparse
import ork.host
import ork.dep
from ork.path import Path
from ork.command import Command, run
from ork import buildtrace
import ork._globals as _glob


parser = argparse.ArgumentParser(description='orkid deploy to folder')

parser.add_argument("--destdir")

_args = vars(parser.parse_args())

this_path = os.path.realpath(__file__)
this_dir = os.path.dirname(this_path)
this_dir = os.path.dirname(this_dir)
this_dir = os.path.dirname(this_dir)

stage_dir = Path(os.path.abspath(str(ork.path.stage())))

build_dest = Path(_args["destdir"])
