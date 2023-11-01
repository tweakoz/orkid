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
from common.cameras import *
from common.primitives import createParticleData
from common.scenegraph import createSceneGraph
this_dir = path.directoryOfInvokingModule()

################################################################################
parser = argparse.ArgumentParser(description='scenegraph skinning example')
args = vars(parser.parse_args())
################################################################################

class SkinningApp(object):

  def __init__(self):
    super().__init__()

    self.materials = set()

    self.ezapp = OrkEzApp.create(self, left=100, top=100, width=960, height=480)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    setupUiCamera( app=self, eye = vec3(0,0,30), constrainZ=True, up=vec3(0,1,0))

  ##############################################

  def onGpuInit(self,ctx):

    self.frame_index = 0
    random.seed(12)

    ###################################
    # create scenegraph
    ###################################

    sg_params = VarMap()
    sg_params.SkyboxIntensity = 1.0
    sg_params.DiffuseIntensity = 1.0
    sg_params.SpecularIntensity = 1.0
    sg_params.AmbientLevel = vec3(1)
    sg_params.SkyboxTexPathStr = "src://envmaps/blender_forest.dds"
    sg_params.preset = "DeferredPBR"

    self.scenegraph = self.ezapp.createScene(sg_params)
    self.layer = self.scenegraph.createLayer("layer")

    ###################################
    # create model data
    ###################################

    tex_white = Texture.load("src://effect_textures/white.dds")
    tex_normal = Texture.load("src://effect_textures/default_normal.dds")

    self.model = XgmModel("data://tests/chartest/char_mesh")
    for mesh in self.model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.texColor = tex_white
        copy.texNormal = tex_normal
        copy.texMtlRuf = tex_white
        copy.baseColor = vec4(1,.7,.8,1)*1.4
        copy.roughnessFactor = 0.8
        copy.metallicFactor = 0.2
        copy.shaderpath = str(this_dir/"skin_override_test.glfx")
        copy.gpuInit(ctx)
        submesh.material = copy

    ###################################
    # create animation data
    ###################################

    self.anim = XgmAnim("data://tests/chartest/char_testanim1")

    self.anim_inst = XgmAnimInst(self.anim)
    self.anim_inst.mask.enableAll()
    self.anim_inst.use_temporal_lerp = True
    self.anim_inst.bindToSkeleton(self.model.skeleton)

    ##################
    # create model / sg node
    ##################

    self.sgnode = self.model.createNode("modelnode",self.layer)
    self.modelinst = self.sgnode.user.pyext_retain_modelinst
    self.modelinst.enableSkinning()
    self.modelinst.enableAllMeshes()
    self.localpose = self.modelinst.localpose
    self.worldpose = self.modelinst.worldpose

  ################################################

  def onGpuUpdate(self,context):
    self.localpose.bindPose()
    self.anim_inst.currentFrame = self.frame_index
    self.anim_inst.weight = 1.0
    self.anim_inst.applyToPose(self.localpose)
    self.localpose.blendPoses()
    self.localpose.concatenate()
    self.worldpose.fromLocalPose(self.localpose,mtx4())
    self.frame_index += 0.3

  ################################################

  def onUpdate(self,updinfo):
    self.scenegraph.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

###############################################################################

SkinningApp().ezapp.mainThreadLoop()
