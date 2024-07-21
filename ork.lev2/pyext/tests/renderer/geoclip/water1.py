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
from orkengine.core import *
from orkengine.lev2 import *
lev2_pyexdir.addToSysPath()
from lev2utils.cameras import *
from lev2utils.scenegraph import createSceneGraph
from signal import signal, SIGINT

tokens = CrcStringProxy()

sys.path.append(str(thisdir()/".."/"particles"))
from _ptc_harness import *

################################################################################
parser = argparse.ArgumentParser(description='scenegraph particles example')

args = vars(parser.parse_args())

################################################################################

class WaterApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=4)
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
    sceneparams.SpecularIntensity = float(1.2)
    sceneparams.DiffuseIntensity = float(1.0)
    sceneparams.AmbientLight = vec3(0.1)
    sceneparams.DepthFogDistance = float(10000)
    sceneparams.DepthFogPower = float(2)
    sceneparams.SkyboxTexPathStr = "src://envmaps/tozenv_nebula.png"
    ###################################
    # post fx node
    ###################################

    postNode = PostFxNodeDecompBlur()
    postNode.threshold = 0.99
    postNode.blurwidth = 16.0
    postNode.blurfactor = 0.1
    postNode.amount = 0.2
    postNode.gpuInit(ctx,8,8);
    postNode.addToSceneVars(sceneparams,"PostFxChain")

    ###################################
    # create scene
    ###################################

    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.fwd_layers = [self.layer_fwd,self.layer_donly]

    ###################################
    # create particle drawable 
    ###################################

    self.ptc_systems = gen_psys_set( self.scene,
                                     self.layer_fwd,
                                     count = 8,
                                     frqbase=0.1,
                                     radbase = 4 )
    for ptc in self.ptc_systems:
      ptc.drawable_data.emitterIntensity = 700.0
      ptc.drawable_data.emitterRadius = 2
      ptc.EMITN.inputs.EmissionRate = random.uniform(40,80)
      ptc.EMITR.inputs.EmissionRate = random.uniform(40,80)
      ptc.particlenode.worldTransform.scale = 4
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

    gmtl = PBRMaterial() 
    gmtl.texColor = Texture.load("src://effect_textures/white.dds")
    gmtl.texNormal = Texture.load("src://effect_textures/default_normal.dds")
    gmtl.texMtlRuf = Texture.load("src://effect_textures/white.dds")
    gmtl.metallicFactor = 1
    gmtl.roughnessFactor = 1
    gmtl.doubleSided = True
    gmtl.shaderpath = str(thisdir()/"water1.glfx")
    gmtl.addLightingLambda()
    gmtl.gpuInit(ctx)
    gmtl.blending = tokens.ALPHA
    self.NOISETEX = Texture.load("src://effect_textures/voltex_pn2.dds")

    freestyle = gmtl.freestyle
    assert(freestyle)
    param_refract_map = freestyle.param("refract_map")
    param_voltexa = freestyle.param("MapVolTexA")
    param_time = freestyle.param("Time")
    param_color = freestyle.param("BaseColor")
    param_depthmap = freestyle.param("depth_map")
    param_bufinvdim = freestyle.param("bufinvdim")
    param_plightamp = freestyle.param("plightamp")
    param_m= freestyle.param("m")
    assert(param_time)
    
    def _gentime():
      return self.curtime

    self.refract_tex = Texture.load( "src://envmaps/tozenv_hellscape.png")

    gmtl.bindParam(param_refract_map,self.refract_tex)
    gmtl.bindParam(param_voltexa,self.NOISETEX)
    gmtl.bindParam(param_time,lambda: _gentime() )
    gmtl.bindParam(param_color,lambda: vec3(0.75,1.2,1) ) 
    gmtl.bindParam(param_depthmap,tokens.RCFD_DEPTH_MAP )
    gmtl.bindParam(param_bufinvdim,tokens.CPD_Rtg_InvDim )
    gmtl.bindParam(param_m,tokens.RCFD_M )
    gmtl.bindParam(param_plightamp,0.2 )
    
    #######################################
    # ground drawable
    #######################################

    gdata = GroundPlaneDrawableData()
    gdata.pbrmaterial = gmtl
    #gmtl.addBasicStateLambdaToPipeline(pipeline)
    #gdata.pipeline = pipeline
    gdata.extent = 10000.0
    self.gdata = gdata
    self.drawable_ground = gdata.createSGDrawable(self.scene)
    self.groundnode = self.scene.createDrawableNodeOnLayers([self.layer_fwd],"partgroundicle-node",self.drawable_ground)
    self.groundnode.worldTransform.translation = vec3(0,-5,0)

    #######################################
    # helmet mesh
    #######################################

    self.model = XgmModel("data://tests/misc_gltf_samples/DamagedHelmet.glb")
    self.drawable_model = self.model.createDrawable()
    self.modelnode = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 35
    self.modelnode.worldTransform.translation = vec3(0,28,0)


  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    self.curtime = updinfo.absolutetime
    update_psys_set(self.ptc_systems,updinfo.absolutetime,90.0)

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
