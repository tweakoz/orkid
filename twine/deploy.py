#!/usr/bin/env python3
import os, pathlib


this_dir = pathlib.PosixPath(os.path.dirname(os.path.realpath(__file__)))
os.chdir(str(this_dir/".."))
os.system("git clean -fdx")
os.system("rm -rf %s" % str(this_dir/"../dist"))

os.system("python3 -m build")
os.system("twine check dist/*.whl")
os.system("twine upload --verbose dist/*")
