#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal, numpy
from obt import path

from orkengine.core import vec3, vec4, quat, mtx4
from orkengine.core import dfrustum, dvec4, fmtx4_to_dmtx4 
from orkengine.core import lev2_pyexdir, Transform
from orkengine.core import CrcStringProxy, thisdir, VarMap
from orkengine import lev2

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createParticleData
from lev2utils.scenegraph import createSceneGraph
this_dir = path.directoryOfInvokingModule()

tokens = CrcStringProxy()

SSAO_NUM_SAMPLES = 32

################################################################################
parser = argparse.ArgumentParser(description='scenegraph skinning example')
args = vars(parser.parse_args())
################################################################################

class HandPoser(object):
  def __init__(self,app,hand_name):
    self.app = app
    self.hand_name = hand_name
    self.jnt_hand = self.app.model.skeleton.jointIndex("mixamorig.%sHand"%(hand_name))
    self.jnt_forearm = self.app.model.skeleton.jointIndex("mixamorig.%sForeArm"%(hand_name))
    self.jnts_thumbs = [ self.app.model.skeleton.jointIndex("mixamorig.%sHandThumb%d"%(hand_name,i+1)) for i in range(4)]
    self.jnts_index = [ self.app.model.skeleton.jointIndex("mixamorig.%sHandIndex%d"%(hand_name,i+1)) for i in range(4)] 
    self.ikchain = lev2.IkChain(self.app.model.skeleton)
    self.ikchain.bindToJointNamed("mixamorig.%sArm"%(hand_name))
    self.ikchain.bindToJointNamed("mixamorig.%sForeArm"%(hand_name))
    self.ikchain.prepare()
    self.ikchain.compute(self.app.localpose,vec3(0,0,0))
    self.ikchain.C1 = .079 
    self.ikchain.C2 = .029 
    self.fixup_indices = [self.jnt_hand] + self.jnts_thumbs + self.jnts_index

  def update(self,offset):
    localpose = self.app.localpose
    concatmatrices = localpose.concatMatrices
    copy_of_locals = [ localpose.localMatrices[i] for i in range(self.app.model.skeleton.numJoints) ]
    copy_of_concats = [ concatmatrices[i] for i in range(self.app.model.skeleton.numJoints) ]

    num_fixups = len(self.fixup_indices)
    #rels = [ copy_of_concats[j] for j in self.fixup_indices ]

    #
    self.mtx_base = concatmatrices[self.jnt_forearm]
    self.mtx_end = concatmatrices[self.jnt_hand]
    self.mtx_endI = self.mtx_end.inverse
    self.extend_length = (self.mtx_base.translation-self.mtx_end.translation).length
    target = self.mtx_end.translation+offset
    self.ikchain.compute(localpose,target)

    if True:

      ####################################
      # reconnect hand to end of forearm
      ####################################

      fixup_base = concatmatrices[self.jnt_forearm]
      fixup_old = concatmatrices[self.jnt_hand].translation
      fixup_idx = concatmatrices[self.jnt_hand].translation
      fixup_new = vec3(0,self.extend_length,0).transform(fixup_base)
      fixup_delta = fixup_new-fixup_old;
    
      xf_delta = mtx4()
      xf_delta.setColumn(3,vec4(fixup_delta,1));

      for i in range(num_fixups):
        j = self.fixup_indices[i]
        O = concatmatrices[j]
        concatmatrices[j] = xf_delta * O # this moves the hand correctly

      ####################################
      # correct rotation of hand
      ####################################

      dir_forarm_to_hand = (concatmatrices[self.jnt_hand].translation
                         - concatmatrices[self.jnt_forearm].translation).normalized

      dir_hand_to_index = (concatmatrices[self.jnts_index[0]].translation 
                        - concatmatrices[self.jnt_hand].translation).normalized

      dir_cross = dir_forarm_to_hand.cross(dir_hand_to_index).normalized
      angle = dir_forarm_to_hand.angle(dir_hand_to_index)

      Q = quat()
      Q.fromAxisAngle(vec4(dir_cross,-angle))
      MQ = Q.toMatrix()

      h = concatmatrices[self.jnt_hand]
      a = mtx4()
      a.setColumn(3,h.getColumn(3))
      ai = a.inverse

      MQ = a*MQ*ai

      for i in range(num_fixups):
        j = self.fixup_indices[i]
        O = concatmatrices[j]
        concatmatrices[j] = MQ * O

      ####################################
      # modify rotation of hand
      ####################################

      mtxRX = mtx4.rotMatrix(vec3(1,0,0),math.sin(self.app.time*2)*0.15)
      mtxRY = mtx4.rotMatrix(vec3(0,1,0),math.sin(self.app.time*1.7)*0.15)
      mtxRZ = mtx4.rotMatrix(vec3(0,0,1),math.sin(self.app.time*3.7)*0.05)
      M = (mtxRX*mtxRY*mtxRZ)
      #M = mtxRY

      a = concatmatrices[self.jnt_hand]
      ai = a.inverse

      MQ = a*M*ai

      for i in range(num_fixups):
        j = self.fixup_indices[i]
        O = concatmatrices[j]
        concatmatrices[j] = MQ * O

################################################################################

class SkinningApp(object):

  def __init__(self):
    super().__init__()

    self.materials = set()

    self.ezapp = lev2.OrkEzApp.create(self, left=100, top=100, width=960, height=480, ssaa=2)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    setupUiCamera( app=self, 
                   eye = vec3(0,0,30), 
                   constrainZ=True, 
                   up=vec3(0,1,0),
                   fov_deg = 110 )
    self.time = 0.0
    self.ssaamode = True

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
    sg_params.SpecularIntensity = 0.0
    sg_params.AmbientLevel = vec3(0)
    sg_params.DepthFogDistance = 10000.0
    sg_params.SkyboxTexPathStr = "src://envmaps/blender_forest.dds"
    #sg_params.SkyboxTexPathStr = "src://envmaps/blender_studio.dds"
    sg_params.preset = "DeferredPBR"
    sg_params.SSAONumSamples = 32
    sg_params.SSAONumSteps = 2
    sg_params.SSAOBias = 1e-3
    sg_params.SSAORadius = 25.0/1000
    sg_params.SSAOWeight = 0.75
    sg_params.SSAOPower = 0.75

    ###################################
    # post fx node
    ###################################

    postNode = lev2.PostFxNodeHSVG()
    postNode.hue = 0.0
    postNode.saturation = 0.8
    postNode.value = 1.0
    postNode.gamma = 1.3
    postNode.gpuInit(ctx,8,8);
    postNode.addToSceneVars(sg_params,"PostFxChain")
    self.post_node = postNode

    ###################################

    self.scenegraph = self.ezapp.createScene(sg_params)
    self.layer = self.scenegraph.createLayer("std_deferred")
    self.pbr_common = self.scenegraph.pbr_common
    self.pbr_common.useFloatColorBuffer = True


    ###################################
    # create model data
    ###################################

    self.model = lev2.XgmModel("data://tests/chartest/char_mesh")
    for mesh in self.model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.assignImages(
          ctx,
          doConform=True
        )
        copy.baseColor = vec4(1,.75,.75,1)
        copy.roughnessFactor = 0.0
        copy.metallicFactor = 1.0
        copy.shaderpath = str(this_dir/"skin_override_test.glfx")
        copy.gpuInit(ctx)
        submesh.material = copy

    ###################################
    # create animation data
    ###################################

    self.anim = lev2.XgmAnim("data://tests/chartest/char_testanim1")

    self.anim_inst = lev2.XgmAnimInst(self.anim)
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
    self.modelinst.drawSkeleton = True
    self.localpose = self.modelinst.localpose
    self.worldpose = self.modelinst.worldpose

    ##################

    self.lposer = HandPoser(self,"Left")
    self.rposer = HandPoser(self,"Right")

  ################################################

  def onGpuUpdate(self,context):

    self.localpose.bindPose()
    self.anim_inst.currentFrame = self.frame_index
    self.anim_inst.weight = 1.0
    self.anim_inst.applyToPose(self.localpose)
    self.localpose.blendPoses()
    self.localpose.concatenate()

    #################################
    ## synthetic motion
    #################################

    offsetL =  vec3(0+math.sin(self.time*1.3)*0.25, 
                    4+math.cos(self.time*2.4)*4.0, 
                    1-math.cos(self.time*3.8)*1.0)

    offsetR =  vec3(0+math.sin(self.time)*0.25, 
                    1+math.cos(self.time*2.1)*3.0, 
                    1-math.cos(self.time*3.1)*1.0)

    self.lposer.update(offsetL)
    self.rposer.update(offsetR)

    self.frame_index = self.time*30.0

  ################################################

  def onUpdate(self,updinfo):
    self.time += updinfo.deltatime
    self.scenegraph.updateScene(self.cameralut) # update and enqueue all scenenodes
    if self.ssaamode == True:
      self.pbr_common.ssaoNumSamples = SSAO_NUM_SAMPLES
    else:
      self.pbr_common.ssaoNumSamples = 0

  ##############################################

  def onUiEvent(self,uievent):

    res = lev2.ui.HandlerResult()
    handled = False

    if uievent.code == tokens.KEY_DOWN.hashed:
      if uievent.keycode == ord("A"):
        if self.ssaamode == True:
          self.ssaamode = False
        else:
          self.ssaamode = True
        print("SSAO MODE",self.ssaamode)
        return res       
 
    if not handled:
      handled = self.uicam.uiEventHandler(uievent)
      if handled:
        self.camera.copyFrom( self.uicam.cameradata )
    return res

###############################################################################

SkinningApp().ezapp.mainThreadLoop()
