#!/usr/bin/env python3
import os, pathlib

this_dir = pathlib.PosixPath(os.path.dirname(os.path.realpath(__file__)))
os.chdir(str(this_dir/".."))
os.system("pip3 install --force-reinstall --editable .")
