#!/usr/bin/env python3

################################################################################
# ECS (Entity/Component/System) minimal sample 
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, random, argparse
from _controller import MYCONTROLLER
from orkengine import lev2, ecs

parser = argparse.ArgumentParser(description='minimal ECS PythonSystem sample')
parser.add_argument('-i','--interactive', action='store_true', help='interactive mode')

args = parser.parse_args()

################################################################################

class PYSYS_MINIMAL(object):

  ##############################################

  def __init__(self):
    super().__init__()
    self.interactive = args.interactive
    if self.interactive:
      self.ezapp = ecs.createApp(self,ssaa=0,left=10,top=42,width=1710,height=564,fullscreen=False)
    else:
      self.ezapp = ecs.createApp(self,ssaa=0,fullscreen=False)

    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)

  ##############################################

  def onGpuInit(self,ctx):
    self.controller = MYCONTROLLER(self)

  ##############################################

  def onUpdateExit(self):
    print( "onUpdateExit")
    self.controller.onUpdateExit()

  ##############################################

  def onGpuExit(self,ctx):
    print( "onGpuExit")
    self.controller.onGpuExit(ctx)

###############################################################################

PYSYS_MINIMAL().ezapp.mainThreadLoop()
