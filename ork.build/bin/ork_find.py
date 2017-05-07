#!/usr/bin/python
import os, sys, string
import ork.build.search as search

#################################################################################
class visitor:
  def __init__(self):
    pass
  def onPath(self,pth):
    print "/////////////////////////////////////////////////////////////"
    print "// path : %s" % pth
    print "/////////"
  def onItem(self,item):
    print "%-*s : line %-*d : %s" % (40, item.path, 5, item.lineno, item.text) 
#################################################################################

if __name__ == "__main__":
  #########################
  if not len(sys.argv) >= 2:
    print("usage: word [module]")
    sys.exit(1)
  #########################
  pathlist = search.search_pathlist
  if len(sys.argv) == 3:
    pathlist = search.makePathList(sys.argv[2])
  #########################
  wsdir = os.environ["ORKDOTBUILD_WORKSPACE_DIR"]
  os.chdir(wsdir)
  find_word = sys.argv[1]
  print("searching for word<%s>" % find_word)
  search.visit(find_word,visitor(),pathlist)
