import glob, re, string, commands, sys, os
import shutil, fnmatch, platform

search_moduleset =  """ork.build ork.core ork.lev2 ork.ent
                       ork.tool ork.fcollada305 tweakout 
                       ork.lua ork.luabind ork.data 
                       ork.mayax """

search_extensions  = """.c .cpp .cc .mel
                        .h .hpp .inl 
                        .m .mm .qml 
                        .py .sconstruct .dae .lua """

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

###############################################################################

search_extension_list = string.split(search_extensions)

def search_at_root(word, root):
 finder = find(word)
 results = list()
 for root, dirs, files in os.walk(root):
  for f in files:
   path = os.path.join(root, f)
   spl = os.path.splitext(path)
   ext = spl[1]
   not_obj = (spl[0].find("/obj/")==-1) and (spl[0].find("/pluginobj/")==-1)
   if not_obj:
    is_in_list = ext in search_extension_list
    if is_in_list:
     for line_number, line in finder(path):
      line = line.replace("\n","")
      res = result(path,line_number,line)
      results.append(res)
 return results 


#################################################################################
def makeModuleList(as_str):
  pathspl = string.split(as_str)
  pathlist = ""
  for p in pathspl:
    pathlist += "%s " % (p)
  return string.split(pathlist)

search_modulelist = makeModuleList(search_moduleset)

#################################################################################

def visit(word,visitor,modulelist=search_modulelist):
  print "searching modules<%s>" % modulelist
  for module in modulelist:
   results = search_at_root(word,module)
   have_results = len(results)!=0
   if have_results:
     visitor.onPath(module)
     for item in results:
       visitor.onItem(item)
