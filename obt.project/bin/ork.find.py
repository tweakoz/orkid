#!/usr/bin/env python3

import sys, os
from obt import command 

assert(len(sys.argv)==2)
args = sys.argv[1:]

cmd_list = [
  "obt.dep.findtext.py",
  "--dep",
  "orkid",
] + args

#cmd_str = " ".join(cmd_list)
#os.system(cmd_str)
command.run(cmd_list,do_log=True)
