#!/usr/bin/env python

import os, sys,string

args = string.join(sys.argv[1:])
cmd = "$TOZ_STAGE/bundle/OrkidTool.app/Contents/MacOS/ork.tool.test.osx.release -filter dae:xgm " + args
print cmd
os.system( cmd )
