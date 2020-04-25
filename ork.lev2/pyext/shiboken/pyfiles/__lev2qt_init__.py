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
qt5py = dep.instance("qt5forpython")
lib1 = cdll.LoadLibrary(qt5py.library_file())
lib2 = cdll.LoadLibrary(qt5py.library_file2())
lib3 = cdll.LoadLibrary(qt5py.pyside_library_file())
##################################
from ._lev2qt import *
