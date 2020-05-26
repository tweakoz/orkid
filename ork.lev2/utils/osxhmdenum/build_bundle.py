#!/usr/bin/env python3

# I tried building the bundles the cmake way.
#  Too complicated
#  a task far better suited for python

import os
from ork import osx, path, pathtools, command
this_dir = path.Path(os.path.dirname(os.path.abspath(__file__)))

infpath = this_dir/"Info.plist"
binpath = path.stage()/"orkid"/"ork.lev2"/"utils"/"osxhmdenum"/"ork.osxhmdenum"
osx.macho_dump(binpath)

##################################################################
bundle_dest = path.stage()/"apps"/"ork.osxhmdenum.app"
bundle_contents = bundle_dest/"Contents"
bundle_macos = bundle_contents/"MacOs"
bundle_bin = bundle_macos/"ork.osxhmdenum"
pathtools.mkdir(bundle_macos,clean=True,parents=True)
##################################################################
pathtools.copyfile(binpath,bundle_bin)
pathtools.copyfile(infpath,bundle_contents/"Info.plist")
##################################################################
def dylib_copy(dylib_name):
    src = path.stage()/"lib"/dylib_name
    dst = bundle_macos/dylib_name
    pathtools.copyfile(src,dst)
    return dst
##################################################################
def dylib_fixid(dylib_name):
    dst = bundle_macos/dylib_name
    command.run([ "install_name_tool",
                  "-id","@executable_path/"+dylib_name,
                  dst ],do_log=True)
##################################################################
def dylib_paths(dylib_name):
  dst = bundle_macos/dylib_name
  osx.macho_replace_loadpaths(dst, "@executable_path/../lib/", "@executable_path/")
  osx.macho_replace_loadpaths(dst, "@rpath/", "@executable_path/")
  osx.macho_replace_loadpaths(dst, path.libs(), "@executable_path")
##################################################################
libs_to_package = [
    "libassimp.5.dylib",
    "libBulletCollision.2.89.dylib",
    "libBulletDynamics.2.89.dylib",
    "libglfw.3.dylib",
    "libHalf-2_4.24.dylib",
    "libIex-2_4.24.dylib",
    "libIexMath-2_4.24.dylib",
    "libIlmImf-2_4.24.dylib",
    "libIlmThread-2_4.24.dylib",
    "libImath-2_4.24.dylib",
    "libIrrXML.dylib",
    "libispc_texcomp.so",
    "libLinearMath.2.89.dylib",
    "libOpenImageIO.2.1.dylib",
    "libOpenImageIO_Util.2.1.dylib",
    "libork_core.dylib",
    "libork_lev2.dylib",
    "libork_ecs.dylib",
    "libork_tuio.dylib",
    "libork_utpp.dylib",
    "libpython3.8d.dylib",
]
##################################################################
out_dylibs = []
for item in libs_to_package:
    out_dylibs += [dylib_copy(item)]
for item in libs_to_package:
    dylib_fixid(item)
for item in libs_to_package:
    dylib_paths(item)
##################################################################
for item in out_dylibs:
  osx.macho_replace_loadpaths(item, "@executable_path/../lib/", "@executable_path/")
for item in out_dylibs:
  osx.macho_replace_loadpaths(item, "@rpath/", "@executable_path/")
##################################################################
osx.macho_replace_loadpaths(bundle_bin, "@executable_path/../lib/", "@executable_path/")
osx.macho_replace_loadpaths(bundle_bin, "@rpath/", "@executable_path/")
osx.macho_replace_loadpaths(bundle_bin, path.libs(), "@executable_path")
#command.run(["install_name_tool","-delete_rpath",path.libs(),binpath])
