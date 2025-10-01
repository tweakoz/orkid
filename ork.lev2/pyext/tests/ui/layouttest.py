#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, signal
from obt import path
from orkengine.core import vec2, vec3, vec4, mtx4, quat, VarMap
from orkengine import lev2

##############################################
# add lev2 python examples dir to python path
#  and import some utilities from there...
##############################################

l2exdir = (lev2.lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.primitives import createParticleData
from lev2utils.scenegraph import createSceneGraph
from lev2utils.primitives import createGridData

################################################################################

class LayoutTest(object):

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, 
                                      left=100, 
                                      top=100, 
                                      width=900, 
                                      height=900)

    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    #lg_group.margin = 4

    self.griditems = lg_group.makeGrid(
      width=2,
      height=2,
      margin = 4,
      uiclass = lev2.ui.Box,
      args = ["label",vec4(0.1,0.1,0.3,1)],
    )

    self.lg_group = lg_group
    lg_group.margin = 4

    ################################################
    # replace left viewport with particle editor
    ################################################

    if False:
      self.objmodel = lev2.ui.ObjModel()
      self.ged_item = lg_group.makeChild( uiclass = lev2.ui.GedSurface,
                                          args = ["GEDSURF",self.objmodel] )
      self.widget_ged = self.ged_item.widget
      self.test_object = lev2.GedTestObjectConfiguration()
      t2 = self.test_object.createTestObject("test2")
      c1 = t2.createCurve("curve1")
      c2 = t2.createCurve("curve2")
      c3 = t2.createCurve("curve3")
      ga = t2.createGradient("gradA")
      gb = t2.createGradient("gradB")
      gc = t2.createGradient("gradC")
      t3 = self.test_object.createTestObject("test3")
      t4 = self.test_object.createTestObject("test4")
      t5 = self.test_object.createTestObject("test5")
      self.objmodel.attach(self.test_object, True)
      lg_group.replaceChild(self.griditems[0].layout, self.ged_item)

    ################################################
    # camera / event handler
    ################################################

    setupUiCamera( app=self, 
                   eye = vec3(0,0,30), 
                   constrainZ=True, 
                   up=vec3(0,1,0))

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ################################################
  # scenegraph viewport UI event handler
  ################################################

  def onSceneGraphUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom( self.uicam.cameradata )
    return lev2.ui.HandlerResult()

  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create scenegraph
    ###################################

    if False:
      sg_params = VarMap()
      sg_params.SkyboxIntensity = 1.0
      sg_params.DiffuseIntensity = 1.0
      sg_params.SpecularIntensity = 1.0
      sg_params.AmbientLevel = vec3(.125)
      sg_params.SkyboxTexPathStr = "ork_envmaps|cold4k"
      sg_params.preset = "ForwardPBR"

      self.scenegraph = lev2.scenegraph.Scene(sg_params)
      self.layer_donly = self.scenegraph.createLayer("depth_prepass")
      self.layer_fwd = self.scenegraph.createLayer("std_forward")

      self.xz_grid_data = createGridData()
      self.xz_grid_data.shader_suffix = "_V4"
      self.xz_grid_data.modcolor = vec3(1.2)
      self.xz_grid_data.intensityA = 1.0*0.5
      self.xz_grid_data.intensityB = 0.97*0.5
      self.xz_grid_data.intensityC = 0
      self.xz_grid_data.intensityD = 0
      self.xz_grid_node = self.layer_fwd.createDrawableNodeFromData("grid",self.xz_grid_data)
      self.xz_grid_node.sortkey = 1

      item = self.lg_group.makeChild( uiclass = lev2.ui.SceneGraphViewport,
                                      args = ["YO"] )
      item.widget.scenegraph = self.scenegraph
      item.widget.forkDB()
      item.widget.evhandler = lambda x: self.onSceneGraphUiEvent(x)
      item.widget.ignoreEvents = False
      self.widget_scene = item.widget

      self.lg_group.replaceChild( self.griditems[1].layout,item)

   
  ################################################

  def onUpdate(self,updinfo):
    if hasattr(self,"widget_scene"):
      self.scenegraph.updateScene(self.cameralut) # update and enqueue all scenenodes
      self.widget_scene.setDirty()
    if hasattr(self,"widget_ged"):
      self.widget_ged.setDirty()

  ##############################################

  def onUiEvent(self,uievent):
    return lev2.ui.HandlerResult()

###############################################################################

LayoutTest().ezapp.mainThreadLoop()
