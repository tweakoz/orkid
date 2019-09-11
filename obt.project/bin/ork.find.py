#!/usr/bin/env python3
###############################################################################
# Orkid Build System
# Copyright 2010-2018, Michael T. Mayers
# email: michael@tweakoz.com
# The Orkid Build System is published under the GPL 2.0 license
# see http://www.gnu.org/licenses/gpl-2.0.html
###############################################################################

import os, sys, string
import ork.search

#################################################################################

if __name__ == "__main__":
 if not len(sys.argv) == 2:
  print("usage: word")
  sys.exit(1)
 word = sys.argv[1]
 ork.search.execute(word)
