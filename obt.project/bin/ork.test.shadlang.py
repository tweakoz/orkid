#!/usr/bin/env python

from obt import command, path

items = [
    #"ui",
    #"ui2",
    #"basic",
    #"particle",
    #"compositor", 
    #"noise",
    "deferred"
]

OK = True
for item in items:
  if OK:
    cmdlist = ["ork.shadlang.exe",
               "--in",
               "shaders://%s.glfx"% str(item),
               "--dot",
               path.temp()/("%s.dot" % str(item)),
               "--glfx",
               path.temp()/("%s.glfx" % str(item))]
  
    OK = (command.run(cmdlist,do_log=True)==0)
    if not OK:
      print("FAILED: %s" % str(item))
      break
