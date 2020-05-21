#!/usr/bin/env python3

# I tried building the bundles the cmake way.
#  Too complicated
#  a task far better suited for python

from ork import osx, path

binpath = path.stage()/"orkid"/"ork.lev2"/"utils"/"osxhmdenum"/"ork.osxhmdenum.exe"
osx.macho_replace_loadpaths(binpath, "@rpath/", "@yopath/")
osx.macho_dump(binpath)