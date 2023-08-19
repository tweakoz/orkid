#!/usr/bin/env python3
###############################################################################
# Orkid Build System
# Copyright 2010-2018, Michael T. Mayers
# email: michael@tweakoz.com
# The Orkid Build System is published under the GPL 2.0 license
# see http://www.gnu.org/licenses/gpl-2.0.html
###############################################################################

import os, sys, string, argparse
import obt.search
import obt.path
import obt.dep

parser = argparse.ArgumentParser(description='build all box products')
parser.add_argument('keywords', metavar='K', type=str, nargs='+', help='search keywords')

_args = vars(parser.parse_args())

#################################################################################

depnode = obt.dep.DepNode.FIND("orkid")
depinst = depnode.instance
path_list = depinst.find_paths()
words = _args["keywords"]
obt.search.execute_at(words,path_list)
