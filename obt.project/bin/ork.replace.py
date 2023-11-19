#!/usr/bin/env python3
###############################################################################
# Orkid Build System
# Copyright 2010-2018, Michael T. Mayers
# email: michael@tweakoz.com
# The Orkid Build System is published under the GPL 2.0 license
# see http://www.gnu.org/licenses/gpl-2.0.html
###############################################################################

import os, sys, string
import obt.search as search
import obt.path

#################################################################################
class visitor:
  def __init__(self,find,repl):
    self.by_file = dict()
    self.find = find
    self.repl = repl
  def __del__(self):
    self.process()
#######################
  def onPath(self,pth):
    print("/////////////////////////////////////////////////////////////")
    print("// path : %s" % pth)
    print("/////////")
#######################
  def onItem(self,item):
    if item.path not in self.by_file:
      self.by_file[item.path] = list()
    self.by_file[item.path].append(item)
#######################
  def process(self):
    for filename in self.by_file.keys():
      items = self.by_file[filename]
      if os.path.isfile(filename):
        lines = list()
        with open(filename,"r") as file:
          lines = file.readlines()
          for item in items:
            lineno = item.lineno
            line = lines[lineno-1]
            line = line.replace(self.find,self.repl)
            lines[lineno-1] = line
            print(line.replace("\n",""))
        with open(filename,"w") as file:
          file.writelines(lines)
      #print("%-*s : line %-*d : %s" % (40, item.path, 5, item.lineno, item.text)
      #pass
    pass
#################################################################################

if __name__ == "__main__":
  #########################
  if not len(sys.argv) >= 2:
    print("usage: find_word repl_word")
    sys.exit(1)
  #########################
  obt.path.project_root().chdir()
  path_list = []
  ork_dir = obt.path.Path(os.environ["ORKID_WORKSPACE_DIR"])
  path_list += [ork_dir/"ork.core"]
  path_list += [ork_dir/"ork.lev2"]
  path_list += [ork_dir/"ork.ecs"]
  path_list += [ork_dir/"ork.houdini"]
  path_list += [ork_dir/"ork.data"/"platform_lev2"/"shaders"]
  #########################
  find_word = sys.argv[1]
  repl_word = sys.argv[2]
  #########################
  search.visit(find_word, visitor(find_word,repl_word),path_list=path_list)
