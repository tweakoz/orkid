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
parser.add_argument('--vrmode', action="store_true", help='run in vr' )
parser.add_argument("-g", '--showgrid', action="store_true", help='show grid' )
parser.add_argument('--showskeleton', action="store_true", help='show skeleton' )
parser.add_argument("-f", '--forceregen', action="store_true", help='force asset regeneration' )
parser.add_argument('--forwardpbr', action="store_true", help='use forward pbr renderer' )
parser.add_argument("-m", "--model", type=str, required=False, default="data://tests/pbr1/pbr1", help='asset to load')
parser.add_argument("-i", "--lightintensity", type=float, default=1.0, help='light intensity')
parser.add_argument("-s", "--specularintensity", type=float, default=1.0, help='specular intensity')
parser.add_argument("-a", "--ambientintensity", type=float, default=0.0, help='diffuse intensity')
parser.add_argument("-d", "--diffuseintensity", type=float, default=1.0, help='diffuse intensity')
parser.add_argument("-r", "--camdist", type=float, default=0.0, help='camera distance')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')
parser.add_argument("-o", "--overrideshader", type=str, default="", help='override shader')
parser.add_argument("-c", "--overridecolor", type=str, default="", help='override color (vec3)')
parser.add_argument("-z", "--disablezeroareapolycheck", action="store_true", help='disable zero area poly check')
parser.add_argument("-x", "--encrypt", action="store_true", help='encrpyt model')
parser.add_argument("-t", "--ssaa", type=int, default=4, help='ssaa')

################################################################################

args = vars(parser.parse_args())
vrmode = (args["vrmode"]==True)
showgrid = args["showgrid"]
modelpath = args["model"]
lightintens = args["lightintensity"]
specuintens = args["specularintensity"]
diffuintens = args["diffuseintensity"]
ambiuintens = args["ambientintensity"]
camdist = args["camdist"]
fwdpbr = args["forwardpbr"]
envmap = args["envmap"]
oshader = args["overrideshader"]
ocolor = args["overridecolor"]
ssaa = args["ssaa"]

if args["forceregen"]:
  os.environ["ORKID_LEV2_FORCE_MODEL_REGEN"] = "1"

if args["showskeleton"]:
  os.environ["ORKID_LEV2_SHOW_SKELETON"] = "1"

if args["disablezeroareapolycheck"]:
  os.environ["ORKID_LEV2_MESHUTIL_DISABLE_ZEROAREACHECK"] = "1"

if args["encrypt"]:
  os.environ["ORKID_ASSET_ENCRYPT_MODE"] = "1"

#os.environ["ORKID_LOGFILE_meshutil.assimp"] = os.environ["OBT_STAGE"]+"/tempdir/assimp.log"

################################################################################

# make sure env vars are set before importing the engine...

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
    setupUiCamera(app=self,eye=vec3(0,0.5,3))
    self.modelinsts=[]

  ##############################################

  def onGpuInit(self,ctx):

    params_dict = {
      "SkyboxIntensity": float(lightintens),
      "AmbientLight": vec3(ambiuintens),
      "DiffuseIntensity": diffuintens,
      "SpecularIntensity": specuintens,
      "depthFogDistance": float(10000),
    }

    if envmap != "":
      params_dict["SkyboxTexPathStr"] = envmap

    if fwdpbr:
      if vrmode:
        rendermodel = "FWDPBRVR"
      else:
        rendermodel = "ForwardPBR"
    else:
      if vrmode:
        rendermodel = "PBRVR"
      else:
        rendermodel = "DeferredPBR"

    #rendermodel = "PICKTEST"

    createSceneGraph( app=self,
                      params_dict=params_dict,
                      rendermodel=rendermodel )
    
    self.scene.enablePickHud()
    
    self.model = XgmModel(modelpath)
    self.sgnode = self.model.createNode("node",self.layer1)

    self.ball_model = XgmModel("data://tests/pbr_calib")
    self.ball_node = self.ball_model.createNode("ball-node",self.layer1)
    self.ball_node.worldTransform.scale = 0.01
    self.ball_node.pickable = False

    ######################
    # override shader ?
    ######################

    if oshader != "":
      self.modelinst = self.sgnode.user.pyext_retain_modelinst
      mesh = self.model.meshes[0]
      orig_submesh = mesh.submeshes[0]


      subinst = self.modelinst.submeshinsts[0]
      mtl_cloned = orig_submesh.material.clone()
      mtl_cloned.baseColor = vec4(1,1,1,1)

      if ocolor != "":
        ocolor_eval = eval(ocolor)
        mtl_cloned.baseColor = vec4(ocolor_eval,1)


      mtl_cloned.metallicFactor = 0
      mtl_cloned.roughnessFactor = 1
      mtl_cloned.texColor = Texture.load("src://effect_textures/white.dds")
      mtl_cloned.texNormal = Texture.load("src://effect_textures/default_normal.dds")
      mtl_cloned.texMtlRuf = Texture.load("src://effect_textures/white.dds")

      if oshader=="topo":
        #meshutil_submesh = ???
        #self.barysub_isect = meshutil_submesh.barycentricUVs()
        #self.prim = meshutil.RigidPrimitive(self.barysub_isect,ctx)
        mtl_cloned.metallicFactor = 0
        mtl_cloned.roughnessFactor = 1
        mtl_cloned.shaderpath = "orkshader://deferred_ovr_topo.glfx"
      elif oshader=="mirror":
        mtl_cloned.metallicFactor = 1
        mtl_cloned.roughnessFactor = .01
      elif oshader=="shinyplastic":
        mtl_cloned.metallicFactor = 0
        mtl_cloned.roughnessFactor = 0
      elif oshader=="roughplastic":
        mtl_cloned.metallicFactor = 0
        mtl_cloned.roughnessFactor = 1
      elif oshader=="roughmetal":
        mtl_cloned.metallicFactor = 1
        mtl_cloned.roughnessFactor = .75

      mtl_cloned.gpuInit(ctx)
      subinst.overrideMaterial(mtl_cloned)

    ######################

    center = self.model.boundingCenter
    radius = self.model.boundingRadius*2.5

    print("center<%s> radius<%s>"%(center,radius))
    
    if camdist!=0.0:
      radius = camdist

    self.uicam.lookAt( center-vec3(0,0,radius), 
                       center, 
                       vec3(0,1,0) )

    #self.uicam.base_zmoveamt = radius*0.01 

    self.camera.copyFrom( self.uicam.cameradata )

    ###################################

    if showgrid:
      self.grid_data = createGridData()
      self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
      self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):
    
    if uievent.code == tokens.PUSH.hashed: 
      camdat = self.uicam.cameradata
      scoord = uievent.pos
      def pick_callback(pixel_fetch_context):
        #dstr = pixel_fetch_context.dump()
        #print(dstr)
        obj = pixel_fetch_context.value(0)
        pos = pixel_fetch_context.value(1)
        nrm = pixel_fetch_context.value(2)
        print("obj: %s"%obj)
        print("pos: %s"%pos)
        print("nrm: %s"%nrm)
        self.ball_node.worldTransform.translation = pos.xyz()
      self.scene.pickWithScreenCoord(camdat,scoord,pick_callback)
    
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

  ################################################

  def onUpdate(self,updinfo):

    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
