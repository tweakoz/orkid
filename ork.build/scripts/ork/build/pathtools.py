import os, sys
import fnmatch
import shutil

###############################################################################

def posixpath(path):
  return '/'.join(os.path.normpath(path).split(os.sep))

###############################################################################

def recursive_glob_get_dirs(path):
  d=[]
  try:
    for i in os.listdir(path):
      if os.path.isdir(path+i):
        d.append(os.path.basename(i))
  except:pass
  return d

###############################################################################

def recursive_patglob(path,pattern):
  l=[]
  if path[-1]!='/':
    path=path+'/'
  for i in recursive_glob_get_dirs(path):
    #print path+i
    l=l+recursive_patglob(path+i,pattern)
  try:
    dirlist = os.listdir(path)
    for i in dirlist:
      ii=i
      i=path+i
      #print i
      if os.path.isfile(i):
        if fnmatch.fnmatch(i,pattern):
          l.append(i)
  except:
      pass
  
  return l
    
###############################################################################

def recursive_glob(path):
  l=[]
  if path[-1]!='/':
    path=path+'/'
  for i in recursive_glob_get_dirs(path):
    #print path+i
    l=l+recursive_glob(path+i)
  try:
    dirlist = os.listdir(path)
    for i in dirlist:
      ii=i
      i=path+i
      if os.path.isfile(i):
        #print i
        #if fnmatch.fnmatch(ii,pattern):
        #print "Matched %s" % (i)
        l.append(i)
  except:
    pass  

  return l

###############################################################################

class path:

  def __init__(self,str_rep=""):
    self.string_rep = str_rep
    self.proc()

  def proc(self):
    tmp = self.string_rep
    tmp = os.path.abspath(tmp)
    self.string_rep = tmp

  def __div__(self,rhs):
    rhs_type = type(rhs)
    assert(rhs_type in (str,type(self)))
    rhs_str = str(rhs)
    tmp = os.path.join(self.string_rep,rhs_str)
    result = path(tmp)
    return result

  def exists(self):
    return os.path.exists(str(self))

  def rglob(self):
    return recursive_glob(str(self))

  def chdir(self):
    return os.chdir(str(self))

  def rmdir(self):
    return os.chdir(str(self))

  def rmtree(self):
    if self.exists():
        shutil.rmtree(str(self))

  def mkdir(self,clean=False):
    if clean and self.exists():
        self.rmtree()
    os.mkdir(str(self))

  def __str__(self):
    #print ( "PATHSTR<%s>\n" % self.string_rep )
    return self.string_rep

  def __repr__(self,rhs):
    return self.__str__(rhs)

  def __truediv__(self,rhs):
    return self.__div__(rhs)

###############################################################################

def SconsSymLink(target, source,env):
  dst = os.path.abspath(str(source[0]))
  src = os.path.abspath(str(target[0]))
  os.symlink(dst,src)
