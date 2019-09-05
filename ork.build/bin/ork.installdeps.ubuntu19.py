#!/usr/bin/env python3

import os

deplist =  ["qtbase5-private-dev","libopenimageio-dev","libboost-dev"]
deplist += ["libglfw3-dev","libflac++-dev","cmake","scons","git"]
deplist += ["rapidjson-dev","graphviz","doxygen","clang"]

for item in deplist:
    os.system("sudo apt install %s" % item)
