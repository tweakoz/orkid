#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, random, numpy, argparse
from obt import path
from pathlib import Path
from orkengine.core import vec3, vec4
from orkengine.core import CrcStringProxy, VarMap
from orkengine.core import lev2_pyexdir, thisdir
from orkengine import lev2
lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie
from signal import signal, SIGINT

tokens = CrcStringProxy()
ui = lev2.ui
sys.path.append(str(thisdir()/".."/"particles"))
from _ptc_harness import *

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')

args = vars(parser.parse_args())

################################################################################

class WaterApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=0,fullscreen=False)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.curtime = 0.0

    setupUiCamera( app=self, #
                   near = 0.1, #
                   far = 10000, #
                   eye = vec3(0,100,150), #
                   constrainZ=True, #
                   up=vec3(0,1,0))

  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create scenegraph
    ###################################
    sceneparams = VarMap() 
    sceneparams.preset = "ForwardPBR"
    sceneparams.SkyboxIntensity = float(1.0)
    sceneparams.SpecularIntensity = float(1.0)
    sceneparams.DiffuseIntensity = float(1.0)
    sceneparams.AmbientLight = vec3(0.1)
    sceneparams.DepthFogDistance = float(10000)
    sceneparams.DepthFogPower = float(2)
    sceneparams.SkyboxTexPathStr = "src://envmaps/tozenv_nebula.png"
    ###################################
    # post fx node
    ###################################

    postNode = lev2.PostFxNodeDecompBlur()
    postNode.threshold = 0.99
    postNode.blurwidth = 3.0
    postNode.blurfactor = 0.1
    postNode.amount = 0.2
    postNode.gpuInit(ctx,8,8);
    postNode.addToSceneVars(sceneparams,"PostFxChain")

    ###################################
    # create scene
    ###################################

    self.scene = self.ezapp.createScene(sceneparams)
    self.pbr_common = self.scene.pbr_common
    self.pbr_common.useFloatColorBuffer = True
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.fwd_layers = [self.layer_fwd,self.layer_donly]

    lite_model = lev2.XgmModel("data://tests/pbr_calib.glb")
    cookie = MyCookie("src://effect_textures/knob2.png")
    shadow_size = 2048
    shadow_bias = -1e-4
    self.spotlight1 = MySpotLight( index=0,
                                  app=self,
                                  model=lite_model,
                                  frq=0.177,
                                  color=vec3(1,1,.7)*9.5e5,
                                  cookie=cookie,
                                  fovbase=70.0,
                                  fovamp=20.0,
                                  voffset=1500,
                                  vscale=1300,
                                  bias=shadow_bias,
                                  dim=shadow_size,
                                  radius=700,
                                  range=4000)

    ###################################
    # create particle drawable 
    ###################################

    self.ptc_systems = gen_psys_set( self.scene,
                                     self.layer_fwd,
                                     count = 8,
                                     frqbase=0.1,
                                     radbase = 20 )
    for ptc in self.ptc_systems:
      ptc.drawable_data.emitterIntensity = 700.0
      ptc.drawable_data.emitterRadius = 2
      ptc.EMITN.inputs.EmissionRate = random.uniform(40,80)
      ptc.EMITR.inputs.EmissionRate = random.uniform(40,80)
      ptc.particlenode.worldTransform.scale = 4
      ptc.SPRI.material.colorIntensity = 4
      ptc.frq = random.uniform(-0.5,0.5)
      x = random.uniform(-0.5,0.5)
      y = random.uniform(-0.5,0.5)
      z = random.uniform(-0.5,0.5)
      ptc.GRAV.inputs.Center = vec3(x,y,z)
      x = random.uniform(-0.5,0.5)
      y = random.uniform(-0.5,0.5)
      z = random.uniform(-0.5,0.5)
      ptc.EMITN.inputs.Offset = vec3(x,y,z)
      x = random.uniform(-0.5,0.5)
      y = random.uniform(-0.5,0.5)
      z = random.uniform(-0.5,0.5)
      ptc.EMITR.inputs.Offset = vec3(x,y,z)
    #######################################
    # ground material (water)
    #######################################

    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    normal = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")
    gmtl = lev2.PBRMaterial() 
    gmtl.assignImages(
      ctx,
      color = white,
      normal = normal,
      mtlruf = white,
      doConform=True
    )
    gmtl.metallicFactor = 1
    gmtl.roughnessFactor = 1
    gmtl.doubleSided = True
    gmtl.shaderpath = str(thisdir()/"geoclipmesh_water.glfx")
    gmtl.addLightingLambda()
    gmtl.gpuInit(ctx)
    gmtl.blending = tokens.ALPHA
    self.NOISETEX = lev2.Texture.load("src://effect_textures/voltex_pn2.dds")
    self.NOISETEX2 = lev2.Texture.load("src://effect_textures/NoiseKern.dds")

    freestyle = gmtl.freestyle
    assert(freestyle)
    #param_refract_map = freestyle.param("refract_map")
    #param_voltexa = freestyle.param("MapVolTexA")
    param_time = freestyle.param("Time")
    param_color = freestyle.param("BaseColor")
    param_depthmap = freestyle.param("depth_map")
    param_bufinvdim = freestyle.param("bufinvdim")
    param_plightamp = freestyle.param("plightamp")
    #param_noizekernmap = freestyle.param("noizekernmap")
    param_m= freestyle.param("m")
    assert(param_time)
    
    def _gentime():
      return self.curtime

    self.refract_tex = Texture.load( "src://envmaps/tozenv_hellscape.png")

    #gmtl.bindParam(param_refract_map,self.refract_tex)
    #gmtl.bindParam(param_voltexa,self.NOISETEX)
    #gmtl.bindParam(param_noizekernmap,self.NOISETEX2)
    gmtl.bindParam(param_time,lambda: _gentime() )
    gmtl.bindParam(param_color,lambda: vec4(0.75,1.2,1,1) ) 
    gmtl.bindParam(param_depthmap,tokens.RCFD_DEPTH_MAP )
    gmtl.bindParam(param_bufinvdim,tokens.CPD_Rtg_InvDim )
    gmtl.bindParam(param_m,tokens.RCFD_M )
    gmtl.bindParam(param_plightamp,0.15 )

    #######################################
    # ground drawable
    #######################################

    gdata = lev2.GeoClipMapDrawable()
    gdata.pbrmaterial = gmtl
    gdata.numLevels = 16
    gdata.ringSize = 128
    gdata.baseQuadSize = 0.25
    #gmtl.addBasicStateLambdaToPipeline(pipeline)
    #gdata.pipeline = pipeline
    #gdata.extent = 10000.0
    self.gdata = gdata
    self.drawable_ground = gdata.createSGDrawable(self.scene)
    self.groundnode = self.scene.createDrawableNodeOnLayers([self.layer_fwd],"partgroundicle-node",self.drawable_ground)
    self.groundnode.worldTransform.translation = vec3(0,0,0)
    self.groundnode.worldTransform.scale = 2
    #self.groundnode.viewRelative = True

    #######################################
    # helmet mesh
    #######################################

    self.model = lev2.XgmModel("data://tests/misc_gltf_samples/DamagedHelmet.glb")
    self.drawable_model = self.model.createDrawable()
    self.modelnode = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 35
    self.modelnode.worldTransform.translation = vec3(0,28,0)


  def onGpuUpdate(self,ctx):
    self.spotlight1.update(self.lighttime)

  ################################################

  def _radial_gerstnerwave_fn(self, center, pos, timeval, frq, baseamp, rscale, falloff):
    radius = (pos.xz - center.xz).length
    amp    = clamp(math.pow(1 / radius, falloff), 0, 1)
    amp *= clamp(radius * rscale, 0, 1)
    dir = (pos - center).normalized
    # gersnter wave point moves in circe about pos along direction and up vectors
    phase        = radius * frq - timeval
    displacementA = dir * math.sin(phase)
    displacementB = vec3(0, -math.cos(phase), 0)
    displacement  = (displacementA + displacementB) * baseamp * amp
    return displacement

  ################################################

  def onUpdate(self,updinfo):
    self.lighttime = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    self.curtime = updinfo.absolutetime
    update_psys_set(self.ptc_systems,updinfo.absolutetime,90.0)
    mdl_y = 0 + 5*math.sin(self.curtime*1.3)
    
    orient = quat(vec3(1,0,0),0.5+math.sin(self.curtime*0.1)*0.4)
    
    speed   = self.curtime * 1.65
    baseamp = 25
    wave_pos = vec3(0,0,0.001)
    radius  = wave_pos.xz.length

    cdist   = 3000.0
    falloff = 0.09

    disp = self._radial_gerstnerwave_fn( vec3(-6, 0, -32) * cdist, # center
                                 wave_pos, # pos
                                 self.curtime, # timeval
                                 0.1, # frq
                                 0.1, # baseamp
                                 0.1, # rscale
                                 0.1 ) # falloff
    
    disp += self._radial_gerstnerwave_fn(
      vec3(6, 0, 5) * cdist, # center
      wave_pos,              # pos
      speed * 2.1,           # timeval
      0.0131,                # frq
      baseamp * 0.3,         # baseamp
      0.00013,               # rscale
      falloff)              # falloff

    disp += self._radial_gerstnerwave_fn(
      vec3(11, 0, -6) * cdist, # center
      wave_pos,                # pos
      speed * 0.6,             # timeval
      0.0131,                  # frq
      baseamp * 0.3,           # baseamp
      0.00013,                 # rscale
      falloff)                # falloff

    disp += self._radial_gerstnerwave_fn(
      vec3(-11, 0, 6) * cdist, # center
      wave_pos,                # pos
      speed * 0.3,             # timeval
      0.00111,                  # frq
      baseamp * 0.5,           # baseamp
      0.00013,                 # rscale
      falloff)                # falloff
    
    
    self.modelnode.worldTransform.translation = disp+vec3(0,mdl_y,0)
    self.modelnode.worldTransform.orientation = orient

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

###############################################################################

def sig_handler(signal_received, frame):
  print('SIGINT or CTRL-C detected. Exiting gracefully')
  sys.exit(0)

###############################################################################

signal(SIGINT, sig_handler)

WaterApp().ezapp.mainThreadLoop()
