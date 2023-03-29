#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, random, numpy, ork.path
from orkengine.core import *
from orkengine.lev2 import *
from _boilerplate import *

################################################################################

class UiGedTestApp(BasicUiCamSgApp):

  def __init__(self):
    super().__init__()

    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFixedFPS, 60)
    self.ezapp.topWidget.enableUiDraw()

    self.test_object = dataflow.DgModuleData.createShared()
    print(self.test_object.clazz.name)
    self.objmodel = ui.ObjModel()
    self.objmodel.attach(self.test_object,True)
    assert(False)
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    self.context = ctx
    pass

  def onGpuIter(self):
    pass


###############################################################################

app = UiGedTestApp()

app.ezapp.mainThreadLoop(on_iter=lambda: app.onGpuIter())
