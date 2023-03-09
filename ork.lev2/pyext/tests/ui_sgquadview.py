#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import sys, math, random
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/".."/".."/"examples"/"python").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createGridData, createCubePrim
from common.scenegraph import createSceneGraph

################################################################################

class panel:
  def __init__(self,cam,uicam):
    self.cur_eye = vec3(0,0,0)
    self.cur_tgt = vec3(0,0,0)
    self.dst_eye = vec3(0,0,0)
    self.dst_tgt = vec3(0,0,0)
    self.counter = 0
    self.camera = cam 
    self.uicam = uicam 

  def update(self):
    def genpos():
      r = vec3(0)
      r.x = random.uniform(-10,10)
      r.z = random.uniform(-10,10)
      r.y = random.uniform(  0,10)
      return r 
    
    if self.counter<=0:
      self.counter = int(random.uniform(1,100))
      self.dst_eye = genpos()
      self.dst_tgt = vec3(0,random.uniform(  0,2),0)

    self.cur_eye = self.cur_eye*0.999 + self.dst_eye*0.001
    self.cur_tgt = self.cur_tgt*0.999 + self.dst_tgt*0.001
    self.uicam.lookAt( self.cur_eye,
                        self.cur_tgt,
                        vec3(0,1,0))

    self.counter = self.counter-1

    self.uicam.updateMatrices()

    self.camera.copyFrom( self.uicam.cameradata )

################################################################################

class UiSgQuadViewTestApp(object):

  def __init__(self):
    super().__init__()

    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)

    # enable UI draw mode
    self.ezapp.topWidget.enableUiDraw()

    # make a grid of scenegraph viewports

    lg_group = self.ezapp.topLayoutGroup
    self.griditems = lg_group.makeGrid( width = 2,
                                        height = 2,
                                        margin = 1,
                                        uiclass = ui.UiSceneGraphViewport,
                                        args = ["box",vec4(1,0,1,1)] )

  ##############################################

  def onGpuInit(self,ctx):

    self.dbufcontext = self.ezapp.vars.dbufcontext
    self.cameralut = self.ezapp.vars.cameras

    # create scenegraph    

    sg_params = VarMap()
    sg_params.SkyboxIntensity = 1.0
    sg_params.preset = "DeferredPBR"
    sg_params.dbufcontext = self.dbufcontext

    self.scenegraph = scenegraph.Scene(sg_params)
    self.grid_data = createGridData()
    self.layer = self.scenegraph.createLayer("layer")
    self.grid_node = self.layer.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1
    self.rendernode = self.scenegraph.compositorrendernode
    cube_prim = createCubePrim(ctx=ctx,size=2.0)
    pipeline_cube = createPipeline( app = self, ctx = ctx, rendermodel="DeferredPBR" )
    self.cube_node = cube_prim.createNode("cube",self.layer,pipeline_cube)

    # assign shared scenegraph and creat cameras for all sg viewports

    def createPanel(camname, griditem):
      camera, uicam = setupUiCameraX( cameralut=self.cameralut,
                                        camname=camname)
      griditem.widget.cameraName = camname
      griditem.widget.scenegraph = self.scenegraph
      return panel(camera, uicam)

    self.panels = [
      createPanel("cameraA",self.griditems[0]),
      createPanel("cameraB",self.griditems[1]),
      createPanel("cameraC",self.griditems[2]),
      createPanel("cameraD",self.griditems[3]),
    ]
    
  ################################################

  def onUpdate(self,updinfo):

    abstime = updinfo.absolutetime

    cube_y = 0.4+math.sin(abstime)*0.2
    self.cube_node.worldTransform.translation = vec3(0,cube_y,0) 
    self.cube_node.worldTransform.orientation = quat(vec3(0,1,0),abstime*90*constants.DTOR) 
    self.cube_node.worldTransform.scale = 0.1

    for p in self.panels:
      p.update()

    self.scenegraph.updateScene(self.cameralut) 

    for g in self.griditems:
      g.widget.setDirty()
    
###############################################################################

UiSgQuadViewTestApp().ezapp.mainThreadLoop()
