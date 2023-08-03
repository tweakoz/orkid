#!/usr/bin/env python

from obt import command, path

items = [
    "ui", "ui2", "compositor"
]

for item in items:
  cmdlist = ["ork.shadlang.exe",
             "--in",
             "shaders://%s.glfx"% str(item),
             "--dotout",
             path.temp()/("%s.dot" % str(item))]
  
  command.run(cmdlist,do_log=True)

