#!/usr/bin/env python3
import os, sys, pathlib

if "OBT_STAGE" in os.environ:
  print( "ERROR: do not run setup.py in a staging environment")
  sys.exit(-1)

this_dir = pathlib.PosixPath(os.path.dirname(os.path.realpath(__file__)))
os.chdir(str(this_dir/".."))
os.system("rm -rf %s" % str(this_dir/"../dist"))

os.system("python3 -m build")
os.system("twine check dist/*.whl")
os.system("pip3 install --force-reinstall dist/orkid-*.whl")
