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
################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
################################################################################

class UiGedTestApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,height=640,width=1280)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    lg_group = self.ezapp.topLayoutGroup
    root_layout = lg_group.layout

    self.objmodel = ui.ObjModel()

    self.geditem = lg_group.makeChild( uiclass = ui.GedSurface,
                                       args = ["box",self.objmodel] )

    self.ged_surf = self.geditem.widget
    self.ged_layout = self.geditem.layout
    # create object model and ged surface

    self.ged_layout.top.anchorTo(root_layout.top)
    self.ged_layout.left.anchorTo(root_layout.left)
    self.ged_layout.bottom.anchorTo(root_layout.bottom)
    self.ged_layout.right.anchorTo(root_layout.right)
    #lg_group.layoutAndAddChild(self.gedsurface)
    #lg_group.layout.fill(self.ged_layout)
    #self.ged_layout.fill(lg_group.layout)

    # create a test object to edit and attach it to the object model
    self.test_object = dataflow.DgModuleData.createShared()
    self.objmodel.attach(self.test_object,True)
    print(self.test_object.clazz.name)

    root_layout.dump()
    #assert(False)
  ##############################################

  def onGpuInit(self,ctx):
    #super().onGpuInit(ctx)
    self.context = ctx
    pass

  def onGpuIter(self):
    pass


###############################################################################

app = UiGedTestApp()

app.ezapp.mainThreadLoop(on_iter=lambda: app.onGpuIter())
