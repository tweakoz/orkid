#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, random, numpy, argparse
from obt import path
from orkengine.core import *
from orkengine.lev2 import *
l2exdir = (lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.primitives import createParticleData
from lev2utils.scenegraph import createSceneGraph

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')
args = vars(parser.parse_args())
################################################################################

class SkinningApp(object):

  def __init__(self):
    super().__init__()

    self.materials = set()

    self.ezapp = OrkEzApp.create(self, left=100, top=100, width=900, height=900)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    lg_group = self.ezapp.topLayoutGroup
    self.griditems = lg_group.makeGrid( width = 2,
                                        height = 1,
                                        margin = 1,
                                        uiclass = ui.SceneGraphViewport,
                                        args = ["SGVP",vec4(1,0,1,1)] )

    ################################################
    # set vertical proportional layout guide 
    ################################################

    vguides = lg_group.vertical_guides
    vguides[1].proportion = 0.5
    vguides[2].proportion = 0.5

    ################################################
    # replace left viewport with particle editor
    ################################################

    #self.objmodel = ui.ObjModel()
    #self.ged_item = lg_group.makeChild( uiclass = ui.GedSurface,
    #                                    args = ["GEDSURF",self.objmodel] )
    #lg_group.replaceChild(self.griditems[0].layout, self.ged_item)
    #self.ged_surf = self.ged_item.widget

    ################################################
    # camera / event handler
    ################################################

    setupUiCamera( app=self, eye = vec3(0,0,30), constrainZ=True, up=vec3(0,1,0))
    self.griditems[1].widget.evhandler = lambda x: self.onSceneGraphUiEvent(x)

  ################################################
  # scenegraph viewport UI event handler
  ################################################

  def onSceneGraphUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom( self.uicam.cameradata )
      #print(self.uicam.cameradata.eye )
    return ui.HandlerResult()

  ##############################################

  def onGpuInit(self,ctx):

    self.frame_index = 0

    ###################################
    # create scenegraph
    ###################################

    sg_params = VarMap()
    sg_params.SkyboxIntensity = 3.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel = vec3(.125)
    sg_params.DepthFogDistance = 10000.0
    sg_params.preset = "DeferredPBR"

    ###################################
    # create animation data
    ###################################

    self.model = XgmModel("data://tests/chartest/char_mesh")
    self.anim = XgmAnim("data://tests/chartest/char_testanim1")

    self.anim_inst = XgmAnimInst(self.anim)
    self.anim_inst.mask.enableAll()
    self.anim_inst.use_temporal_lerp = True
    self.anim_inst.bindToSkeleton(self.model.skeleton)

    ##################
    # create model / sg node
    ##################

    self.scenegraph = scenegraph.Scene(sg_params)
    self.layer = self.scenegraph.createLayer("std_deferred")
    self.sgnode = self.model.createNode("modelnode",self.layer)
    self.modelinst = self.sgnode.user.pyext_retain_modelinst
    self.modelinst.enableSkinning()
    self.modelinst.enableAllMeshes()
    self.localpose = self.modelinst.localpose
    self.worldpose = self.modelinst.worldpose

    for i in range(0,1):
      self.griditems[i].widget.scenegraph = self.scenegraph
      self.griditems[i].widget.forkDB()

  ################################################

  def onGpuPreFrame(self,context):
    pass
  def onGpuPostFrame(self,context):
    pass

  ################################################

  def onGpuUpdate(self,context):
    self.localpose.bindPose()
    self.anim_inst.currentFrame = self.frame_index
    self.anim_inst.weight = 1.0
    self.anim_inst.applyToPose(self.localpose)
    self.localpose.blendPoses()
    self.localpose.concatenate()
    self.worldpose.fromLocalPose(self.localpose,mtx4())
    self.frame_index += 0.1

  ################################################

  def onUpdate(self,updinfo):
    self.scenegraph.updateScene(self.cameralut) # update and enqueue all scenenodes
    for g in self.griditems:
      g.widget.setDirty()

  ##############################################

  def onUiEvent(self,uievent):
    return ui.HandlerResult()

###############################################################################

SkinningApp().ezapp.mainThreadLoop()
