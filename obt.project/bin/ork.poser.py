#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, random, argparse, sys, os, time
from obt import path

#time.sleep(10)
################################################################################

thisdir = path.directoryOfInvokingModule()

sys.path.append(str(thisdir/".."/".."/"ork.lev2"/"examples"/"python")) # add parent dir to path

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-g", '--showgrid', action="store_true", help='show grid' )
parser.add_argument("-f", '--forceregen', action="store_true", help='force asset regeneration' )
parser.add_argument("-m", "--model", type=str, required=False, default="data://tests/pbr1/pbr1", help='asset to load')
parser.add_argument("-i", "--lightintensity", type=float, default=1.0, help='light intensity')
parser.add_argument("-r", "--camdist", type=float, default=0.0, help='camera distance')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')
parser.add_argument("-t", "--ssaa", type=int, default=4, help='ssaa')

################################################################################

args = vars(parser.parse_args())
showgrid = args["showgrid"]
modelpath = args["model"]
lightintens = args["lightintensity"]
camdist = args["camdist"]
envmap = args["envmap"]
ssaa = args["ssaa"]

################################################################################
# make sure env vars are set before importing the engine...
################################################################################

if args["forceregen"]:
  os.environ["ORKID_LEV2_FORCE_MODEL_REGEN"] = "1"

os.environ["ORKID_LEV2_SHOW_SKELETON"] = "1"

################################################################################

from orkengine.core import *
from orkengine.lev2 import *
from common.cameras import *
from common.shaders import *
from common.primitives import createGridData
from common.scenegraph import createSceneGraph
tokens = CrcStringProxy()

################################################################################

class SceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=ssaa)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    self.editor = Editor()
    setupUiCamera(app=self,eye=vec3(0,0.5,1))

  ##############################################

  def onGpuInit(self,ctx):

    params_dict = {
      "SkyboxIntensity": float(lightintens),
      "AmbientLight": vec3(0.05),
      "DiffuseIntensity": 1,
      "SpecularIntensity": 1,
      "depthFogDistance": float(10000),
    }

    if envmap != "":
      params_dict["SkyboxTexPathStr"] = envmap

    rendermodel = "DeferredPBR"

    #rendermodel = "PICKTEST"

    createSceneGraph( app=self,
                      params_dict=params_dict,
                      rendermodel=rendermodel )
    
    self.scene.enablePickHud()
    
    self.model = XgmModel(modelpath)
    self.skeleton = self.model.skeleton
    self.sgnode = self.model.createNode("node",self.layer1)
    self.modelinst = self.sgnode.user.pyext_retain_modelinst
    self.modelinst.enableSkinning()
    self.modelinst.enableAllMeshes()
    self.localpose = self.modelinst.localpose
    self.worldpose = self.modelinst.worldpose
    self.animinst = XgmAnimInst()
    self.animinst.weight = 1.0
    self.animinst.mask.enableAll()
    self.animinst.use_temporal_lerp = True
    self.animinst.bindToSkeleton(self.skeleton)

    self.ball_model = XgmModel("data://tests/pbr_calib")
    self.ball_node = self.ball_model.createNode("ball-node",self.layer1)
    self.ball_node.worldTransform.scale = 0.01
    self.ball_node.pickable = False

    ######################

    center = self.model.boundingCenter
    radius = self.model.boundingRadius*1.5

    print("center<%s> radius<%s>"%(center,radius))
    
    if camdist!=0.0:
      radius = camdist

    self.uicam.lookAt( center-vec3(0,0,radius), 
                       center, 
                       vec3(0,1,0) )

    self.camera.copyFrom( self.uicam.cameradata )

    ###################################

    if showgrid:
      self.grid_data = createGridData()
      self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
      self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):
    
    if uievent.alt:
      if uievent.code == tokens.MOVE.hashed:
        camdat = self.uicam.cameradata
        scoord = uievent.pos
        def pick_callback(pixel_fetch_context):
          obj = pixel_fetch_context.value(0)
          pos = pixel_fetch_context.value(1)
          nrm = pixel_fetch_context.value(2)
          uv  = pixel_fetch_context.value(3)
          if obj is not None:
            sel_bone = obj["y"]
            self.ball_node.worldTransform.translation = pos.xyz()
            if type(obj["x"]) == vec4:
              self.skeleton.selectJoint(sel_bone)
              dcmtx = DecompMatrix()
              self.localpose.bindPose()
              self.localpose.poseJoint( sel_bone, 1.0, dcmtx )
              self.localpose.blendPoses()
              self.localpose.concatenate()
            else:
              self.skeleton.selectJoint(-1)
              self.localpose.bindPose()
              self.localpose.blendPoses()
              self.localpose.concatenate()
        self.scene.pickWithScreenCoord(camdat,scoord,pick_callback)
    else:
      handled = self.uicam.uiEventHandler(uievent)
      if handled:
        self.camera.copyFrom( self.uicam.cameradata )
          
    

  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
