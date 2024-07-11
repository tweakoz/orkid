#!/usr/bin/env python3

################################################################################
# ECS (Entity/Component/System) minimal sample 
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, random
from _controller import MYCONTROLLER
from orkengine import lev2, ecs

################################################################################

class PYSYS_MINIMAL(object):

  ##############################################

  def __init__(self):
    super().__init__()
    self.ezapp = ecs.createApp(self,ssaa=0,fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)

  ##############################################

  def onGpuInit(self,ctx):
    self.controller = MYCONTROLLER(self.ezapp)

  ##############################################

  def onGpuExit(self,ctx):
    self.controller.terminate()

###############################################################################

PYSYS_MINIMAL().ezapp.mainThreadLoop()
