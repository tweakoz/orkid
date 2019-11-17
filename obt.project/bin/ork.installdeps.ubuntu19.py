#!/usr/bin/env python3

import os

deplist =  ["qtbase5-private-dev","libopenimageio-dev","libboost-dev"]
deplist += ["libglfw3-dev","libflac++-dev","cmake","scons","git"]
deplist += ["rapidjson-dev","graphviz","doxygen","clang","libtiff-dev"]
deplist += ["libboost-filesystem-dev","libboost-system-dev","libboost-thread-dev"]
deplist += ["libqt5x11extras5-dev", "portaudio19-dev", "pybind11-dev"]
deplist += ["libpng-dev","clang-format","python-dev", "libnvtt-bin"]

for item in deplist:
    os.system("sudo apt install %s" % item)
