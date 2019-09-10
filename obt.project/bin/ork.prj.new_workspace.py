#!/usr/bin/python

import os
import sys

this_path = os.path.realpath(__file__)
this_dir = os.path.dirname(this_path)
this_dir = os.path.dirname(this_dir)

print "this_dir<%s>" % this_dir

nargs = len(sys.argv)
if 1 == nargs:
	print "usage: new_workspace.py wsname"

wspath = sys.argv[1]
wsname = os.path.basename(wspath)
wsdir = os.path.dirname(wspath)

print "wspath<%s> dirname<%s> wsname<%s>" % (wspath,wsdir,wsname)

cd = os.getcwd()
if os.path.exists(wspath):
	print "sorry cannot create workspace, folder<%s> already exists" % wspath
	sys.exit(0)

os.system("mkdir -p %s"%wspath)
os.chdir(wspath)

os.system( "cp -r %s ./" % this_dir )
