#!/usr/bin/python
import os, sys, string

#################################################################################

def find(word):
 def _find(path):
  with open(path, "rb") as fp:
   for n, line in enumerate(fp):
    if word in line:
     yield n+1, line
 return _find

#################################################################################

class result:
 def __init__(self,path,lineno,text):
  self.path = path
  self.lineno = lineno
  self.text = text

#################################################################################

search_extensions  = ".c .cpp .cc "
search_extensions += ".h .hpp .inl "
search_extensions += ".m .mm .qml "
search_extensions += ".py .sconstruct .dae .lua"

search_extension_list = string.split(search_extensions)

def search_at_root(word, root):
 finder = find(word)
 results = list()
 for root, dirs, files in os.walk(root):
  for f in files:
   path = os.path.join(root, f)
   #print path
   spl = os.path.splitext(path)
   ext = spl[1]
   not_obj = (spl[0].find("/obj/")==-1) and (spl[0].find("/pluginobj/")==-1)
   #print spl[0], fobj

   if not_obj:
    is_in_list = ext in search_extension_list
    if is_in_list:
     for line_number, line in finder(path):
      line = line.replace("\n","")
      res = result(path,line_number,line)
      results.append(res)
 return results  

#################################################################################

pathset =  "ork.build ork.core ork.lev2 ork.ent ork.tool ork.fcollada305 tweakout ork.lua ork.luabind ork.data"
pathspl = string.split(pathset)

#################################################################################

pathlist = ""
for p in pathspl:
  pathlist += "%s " % (p)

pathlist = string.split(pathlist)

#################################################################################

def search(word):
  for path in pathlist:
   results = search_at_root(word,path)
   have_results = len(results)!=0
   if have_results:
    print "/////////////////////////////////////////////////////////////"
    print "// path : %s" % path
    print "/////////"
    for item in results:
     print "%s : line %d : '%s'" % (item.path, item.lineno, item.text) 

#################################################################################

if __name__ == "__main__":
 if not len(sys.argv) == 2:
  print("usage: word")
  sys.exit(1)
 word = sys.argv[1]
 search(word)
