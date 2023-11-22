#!/usr/bin/env python3
from obt.path import Path, directoryOfInvokingModule
from obt import command
import os, sys
import argparse
from ork import blender

parser = argparse.ArgumentParser(description="export orkid character mesh (via blender)")
parser.add_argument("--inpfile", "-i", help="input blend file")
parser.add_argument("--outfile","-o",  help="output glb file")

if len(sys.argv) != 5:
  parser.print_help()
  sys.exit(-1)

args = parser.parse_args()
inp_file = Path(args.inpfile)
out_file = Path(args.outfile)
print(inp_file,out_file)

this_dir = directoryOfInvokingModule()
print(this_dir)
print(blender.executable)

os.environ["BLENDER_FILE_PATH"] = str(inp_file)
os.environ["EXPORT_PATH"] = str(out_file)

cmd_list = [
    blender.executable,
    "--background",
    "--python",
    this_dir/"_blender_impl_export_character.py",
]

command.run(cmd_list)