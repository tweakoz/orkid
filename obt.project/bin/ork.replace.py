#!/usr/bin/env python3

import sys, os
from obt import command 

assert(len(sys.argv)==3)
args = sys.argv[1:]
assert(len(args)==2)

cmd_list = [
  "obt.dep.replacetext.py",
  "--dep",
  "orkid",
  "--find",
  args[0],
  "--replace",
  args[1],
]

command.run(cmd_list,do_log=True)
