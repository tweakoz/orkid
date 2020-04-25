from ork import env, dep
from ctypes import *
##################################
# get the shiboken library from our special spot
#  todo - figure out why it cant already..
#  things we know:
#   its not in LD_LIBRARY_PATH (by design)
#   it is dynamically linked into _lev2.so
#  a guess:
#   a stock python3 invocation needs it in LD_LIBRARY_PATH
##################################
shiboken_path = dep.instance("qt5forpython").library_file()
lib1 = cdll.LoadLibrary(shiboken_path)
##################################
from ._lev2 import *
