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
    self.ezapp = OrkEzApp.create(self,left=420, top=100, height=960,width=480)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    lg_group = self.ezapp.topLayoutGroup
    root_layout = lg_group.layout

    # create object model and ged surface
    self.objmodel = ui.ObjModel()
    self.geditem = lg_group.makeChild( uiclass = ui.GedSurface,
                                       args = ["box",self.objmodel] )
    self.ged_surf = self.geditem.widget
    self.ged_layout = self.geditem.layout

    # lay it out
    self.ged_layout.top.anchorTo(root_layout.top)
    self.ged_layout.left.anchorTo(root_layout.left)
    self.ged_layout.bottom.anchorTo(root_layout.bottom)
    self.ged_layout.right.anchorTo(root_layout.right)

    # create a test object to edit and attach it to the object model
    self.test_object = GedTestObjectConfiguration()
    t1 = self.test_object.createTestObject("test1")
    t2 = self.test_object.createTestObject("test2")
    c1 = t2.createCurve("curve1")
    c2 = t2.createCurve("curve2")
    c3 = t2.createCurve("curve3")
    g3 = t2.createGradient("gradA")
    self.objmodel.attach(self.test_object,True)
    print(self.test_object.clazz.name)

    #root_layout.dump()
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
