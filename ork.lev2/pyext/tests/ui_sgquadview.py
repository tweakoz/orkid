#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import sys
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/".."/".."/"examples"/"python").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createGridData
from common.scenegraph import createSceneGraph

################################################################################

class UiSgQuadViewTestApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    lg_group = self.ezapp.topLayoutGroup
    griditems = lg_group.makeGrid( width = 2,
                                   height = 2,
                                   margin = 1,
                                   uiclass = ui.UiSceneGraphViewport,
                                   args = ["box",vec4(1,0,1,1)] )

  ##############################################

  def onGpuInit(self,ctx):
    self.cameralut = self.ezapp.vars.cameras
    self.dbufcontext = self.ezapp.vars.dbufcontext

    self.cameraA, self.uicamA = setupUiCameraX( cameralut=self.cameralut,
                                                camname="cameraA")

    self.cameraB, self.uicamB = setupUiCameraX( cameralut=self.cameralut,
                                                camname="cameraB")

    self.cameraC, self.uicamC = setupUiCameraX( cameralut=self.cameralut,
                                                camname="cameraC")

    self.cameraD, self.uicamD = setupUiCameraX( cameralut=self.cameralut,
                                                camname="cameraD")
    
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


    #griditems[0].scenegraph = sg
    #griditems[1].scenegraph = sg
    #griditems[2].scenegraph = sg
    #griditems[3].scenegraph = sg

  ################################################

  def onUpdate(self,updinfo):
    self.scenegraph.updateScene(self.cameralut) # update and enqueue all scenenodes
    pass

###############################################################################

UiSgQuadViewTestApp().ezapp.mainThreadLoop()
