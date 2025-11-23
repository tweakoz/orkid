#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, random, argparse, sys, os
from obt import path

################################################################################

thisdir = path.directoryOfInvokingModule()

sys.path.append(str(thisdir/".."/".."/"ork.lev2"/"examples"/"python")) # add parent dir to path

print("WTF")
print(sys.argv)
################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-g", '--showgrid', action="store_true", help='show grid' )
parser.add_argument('--showskeleton', action="store_true", help='show skeleton' )
parser.add_argument("-f", '--forceregen', action="store_true", help='force asset regeneration' )
parser.add_argument("-m", "--model", type=str, required=False, default="data://tests/pbr1/pbr1", help='asset to load')
parser.add_argument("-i", "--lightintensity", type=float, default=1.0, help='light intensity')
parser.add_argument("-s", "--specularintensity", type=float, default=1.0, help='specular intensity')
parser.add_argument("-a", "--ambientintensity", type=float, default=0.0, help='diffuse intensity')
parser.add_argument("-d", "--diffuseintensity", type=float, default=1.0, help='diffuse intensity')
parser.add_argument("-D", "--camdist", type=float, default=0.0, help='camera distance')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')
parser.add_argument("-o", "--overrideshader", type=str, default="", help='override shader')
parser.add_argument("-c", "--overridecolor", type=str, default="", help='override color (vec3)')
parser.add_argument("-z", "--disablezeroareapolycheck", action="store_true", help='disable zero area poly check')
parser.add_argument("-x", "--encrypt", action="store_true", help='encrpyt model')
parser.add_argument("-t", "--ssaa", type=int, default=0, help='ssaa')
parser.add_argument("-u", "--ssao", type=int, default=0, help='SSAO samples')
parser.add_argument("-L", "--lightmap", type=str, default="", help='set active lightmap')
parser.add_argument('-r', '--rendermodel', type=str, default='forward', help='rendering model (deferred,forward)')
parser.add_argument('-S', '--stateDebugger', type=bool, default=False, help='Graphics state debugger')

################################################################################

args = vars(parser.parse_args())
showgrid = args["showgrid"]
modelpath = args["model"]
lightintens = args["lightintensity"]
specuintens = args["specularintensity"]
diffuintens = args["diffuseintensity"]
ambiuintens = args["ambientintensity"]
camdist = args["camdist"]
envmap = args["envmap"]
oshader = args["overrideshader"]
ocolor = args["overridecolor"]
ssaa = args["ssaa"]
ssao = args["ssao"]
lightmap = args["lightmap"]
rendermodel = args["rendermodel"]
statedebug = args["stateDebugger"]

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

def trace_imports(frame, event, arg):
    if event == "import":
        module_name = arg
        print(f"Importing module: {module_name}")
    return trace_imports

sys.settrace(trace_imports) 
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

################################################################################

#assert(False)

class SceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=ssaa)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(0,0.5,3))
    self.modelinsts=[]
    self.ssaamode = False
    if ssao>0:
      self.ssaamode = True
    self.curbrdfi = 0
    self.cursati = 0
    self.curgami = 0
    self.brdfset = [("GGX",tokens.GGX),("VELVET",tokens.GGXVELVET),("GGXRIM",tokens.GGXRIM),("BLINN",tokens.BLINN),("PHONG",tokens.PHONG)]
    self.satset = [0.0,0.1,0.2,0.5,0.75,1.0]
    self.gamset = [0.8,1.0,1.2,1.4,1.6,1.8,2.0,2.4]
    
  ##############################################

  def onGpuInit(self,ctx):

    sceneparams = VarMap() 
    sceneparams.preset = "ForwardPBR"
    sceneparams.SkyboxIntensity = float(lightintens)
    sceneparams.SpecularIntensity = float(specuintens)
    sceneparams.DiffuseIntensity = float(diffuintens)
    sceneparams.AmbientLight = vec3(ambiuintens)
    sceneparams.DepthFogDistance = float(1e5)
    sceneparams.SkyboxTexPathStr = "ork_envmaps|tozenv_nebula"

    if envmap != "":
      sceneparams.SkyboxTexPathStr = envmap

    #rendermodel = "DeferredPBR"
    global rendermodel
    if rendermodel == "deferred":
      rendermodel = "DeferredPBR"
    elif rendermodel == "forward":
      rendermodel="ForwardPBR"

    sceneparams.preset = rendermodel

    ###################################
    # post fx node
    ###################################
    postNode = PostFxNodeHSVG()
    postNode.hue = 0.0
    postNode.saturation = 1.0
    postNode.value = 1.0
    postNode.gamma = 2.2
    postNode.gpuInit(ctx,8,8);
    #postNode.addToSceneVars(sceneparams,"PostFxChain")
    self.post_node = postNode

    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.fwd_layers = [self.layer_fwd,self.layer_donly]
    self.pbr_common = self.scene.pbr_common
    self.pbr_common.useFloatColorBuffer = True

    ######################

    self.model = XgmModel(modelpath)
    self.sgnode = self.model.createNode("node",self.layer_fwd)
    self.pbr_common = self.scene.pbr_common
    self.model.debugRenderingModel = tokens.ALL if statedebug else tokens.NONE
    self.model.debugPassID = tokens.ALL # PROBE MAIN
    self.model.debugSubPassID = tokens.ALL # tokens.FORWARD_PBR

    ######################
    # override shader ?
    ######################

    if lightmap != "":
      self.modelinst = self.sgnode.user.pyext_retain_modelinst
      for m in self.model.meshes:
        for s in m.submeshes:
          mtl = s.material
          mtl.setActiveLightMapA(lightmap,vec3(1))

    elif oshader != "":
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
      if rendermodel == "ForwardPBR":
        self.grid_data.shader_suffix = "_V3"
      self.grid_node = self.layer_fwd.createGridNode("grid",self.grid_data)
      self.grid_node.sortkey = 1

    self.scene.lightingmanager.gpuInit(ctx)

  ##############################################

  def onUiEvent(self,uievent):
    res = ui.HandlerResult()
    if uievent.code == tokens.KEY_DOWN.hashed:
      ######################
      if uievent.keycode == ord("A"):
        if self.ssaamode == True:
          self.ssaamode = False
        else:
          self.ssaamode = True
        print("SSAO MODE",self.ssaamode)
        return res
      ######################
      if uievent.keycode == ord("B"):
        brdfi = self.curbrdfi+1
        if brdfi >= len(self.brdfset):
          brdfi = 0
        self.curbrdfi = brdfi
        brdf = self.brdfset[self.curbrdfi]
        print("BRDF",brdf[0])
        self.pbr_common.setBRDF(brdf[1])
      ######################
      if uievent.keycode == ord("S"):
        sati = self.cursati+1
        if sati >= len(self.satset):
          sati = 0
        self.cursati = sati
        sat = self.satset[self.cursati]
        self.post_node.saturation = sat
      ######################
      if uievent.keycode == ord("G"):
        gami = self.curgami+1
        if gami >= len(self.gamset):
          gami = 0
        self.curgami = gami
        gam = self.gamset[self.curgami]
        self.post_node.gamma = gam
      ######################
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    else:
      handled = ui.HandlerResult()
    return res

  ################################################

  def onUpdate(self,updinfo):

    if self.ssaamode:
      self.pbr_common.ssaoNumSamples = ssao 
    else:
      self.pbr_common.ssaoNumSamples = 0 
    self.scene.updateScene(self.cameralut) 

###############################################################################

print("XXXX")
SceneGraphApp().ezapp.mainThreadLoop()
