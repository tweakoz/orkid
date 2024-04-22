#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

lev2_pyexdir.addToSysPath()
from common.cameras import *
from common.shaders import *
from common.misc import *
from common.primitives import createGridData
from common.scenegraph import createSceneGraph
from common.lighting import MySpotLight, MyCookie

sys.path.append(str(thisdir()/".."/"particles"))
from _ptc_harness import *

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
################################################################################
args = vars(parser.parse_args())
################################################################################
tokens = CrcStringProxy()

class LIGHTING_APP(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=1,msaa=1)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(0,12,15))

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    params_dict = {
      "SkyboxIntensity": float(1),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.07),
      "DepthFogDistance": float(10000),
      "supersample": "1",
    }

    #createSceneGraph(app=self,rendermodel="DeferredPBR",params_dict=params_dict)
    createSceneGraph(app=self,rendermodel="ForwardPBR",params_dict=params_dict)
    self.render_node = self.scene.compositorrendernode
    self.pbr_common = self.render_node.pbr_common
    self.pbr_common.useFloatColorBuffer = True
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_dprobe = self.scene.createLayer("depth_probe")
    self.layer_probe = self.scene.createLayer("probe")
    self.layer_fwd = self.layer1
    DEPTH_LAYERS = [self.layer_donly,self.layer_dprobe]
    FINAL_LAYERS = [self.layer_fwd,self.layer_donly]
    COLOR_LAYERS = [self.layer_fwd,self.layer_probe]
    ALL_LAYERS = [self.layer_fwd,self.layer_probe,self.layer_donly]

    ###################################

    #createDefaultStreakSystem(app=self)
    #self.particlenode.worldTransform.translation = vec3(0,1,0)
    #self.particlenode.worldTransform.scale = 0.3
    #self.layer_probe.addDrawableNode(self.particlenode)
    
    ###################################

    model = XgmModel("data://tests/pbr_calib.glb")
    for mesh in model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.texColor = Texture.load("src://effect_textures/white.dds")
        copy.texNormal = Texture.load("src://effect_textures/default_normal.dds")
        copy.texMtlRuf = Texture.load("src://effect_textures/white.dds")
        submesh.material = copy

    class Node:
      def __init__(self,app,pos,color,layers,mtl=0,ruf=1):
        self.drawable_model = model.createDrawable()
        self.modelnode = app.scene.createDrawableNodeOnLayers(layers,"model-node",self.drawable_model)
        self.modelnode.worldTransform.scale = 1
        self.modelnode.worldTransform.translation = pos
        subinst = self.drawable_model.modelinst.submeshinsts[0]
        mtl_cloned = subinst.material.clone()
        mtl_cloned.metallicFactor = mtl
        mtl_cloned.roughnessFactor = ruf
        mtl_cloned.baseColor = vec4(color,1)
        subinst.overrideMaterial(mtl_cloned)


    self.node_px = Node(self,vec3(5,2,0),vec3(2,0,0),ALL_LAYERS)
    self.node_nx = Node(self,vec3(-5,2,0),vec3(0,2,0),ALL_LAYERS)
    self.node_pz = Node(self,vec3(0,2,5),vec3(0,0,2),ALL_LAYERS)
    self.node_nz = Node(self,vec3(0,2,-5),vec3(2),ALL_LAYERS)

    self.node_ctr = Node(self,vec3(0,2,0),vec3(1),DEPTH_LAYERS+[self.layer_fwd],mtl=1,ruf=0)

    ###################################

    self.grid_data = createGridData()
    self.grid_data.shader_suffix = "_V4"
    self.grid_data.modcolor = vec3(1)
    self.grid_data.intensityA = 1
    self.grid_data.intensityB = .95
    self.grid_data.intensityC = 0
    self.grid_data.intensityD = 0
    self.grid_data.lineWidth = 0.05
    self.grid_data.texturepath = "src://effect_textures/white.dds"
    self.grid_drawable = self.grid_data.createDrawable()
    self.grid_node = self.scene.createDrawableNodeOnLayers(COLOR_LAYERS,"grid-node",self.grid_drawable)
    #self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    ###################################

    cookie1 = MyCookie("src://effect_textures/L0D.png")
    cookie2 = MyCookie("src://effect_textures/knob2.dds")
    
    shadow_size = 2048
    shadow_bias = 1e-4
    if True:
      self.spotlight1 = MySpotLight( index=0,
                                     app=self,
                                     model=model,
                                     frq=0.37,
                                     color=vec3(0,700,1500)*2,
                                     cookie=cookie1,
                                     fovbase=50.0,
                                     fovamp=25.0,
                                     voffset=16,
                                     vscale=8,
                                     bias=shadow_bias,
                                     dim=shadow_size,
                                     radius=8,
                                     layers = COLOR_LAYERS)
      self.spotlight2 = MySpotLight( index=0,
                                     app=self,
                                     model=model,
                                     frq=0.47,
                                     color=vec3(500,0,0),
                                     cookie=cookie2,
                                     fovbase=30.0,
                                     fovamp=35.0,
                                     voffset=12,
                                     vscale=8,
                                     bias=shadow_bias,
                                     dim=shadow_size,
                                     radius=4,
                                     layers = COLOR_LAYERS)
    ##############################################

    self.probe = LightProbe()
    self.probe.type = tokens.REFLECTION
    self.probe.imageDim = 1024
    self.probe.worldMatrix = mtx4.transMatrix(0,4,0)
    self.probe.name = "probe1"
    self.probe_node = self.layer1.createLightProbeNode("probe",self.probe)

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):
    self.lighttime = updinfo.absolutetime
    
    
    
    self.scene.updateScene(self.cameralut) 
    

  def onGpuUpdate(self,ctx):
    def genpos(node,frq,offset,radius=5,yscale=2):
      phase = offset+self.lighttime*frq
      x = math.sin(phase)*radius
      z = math.cos(phase)*radius
      y = 4 + math.cos(phase*2.7)*1
      node.modelnode.worldTransform.translation = vec3(x,y,z)
    
    self.probe.invalidate()
    #genpos(self.node_px,0.3,0)
    #genpos(self.node_nx,0.3,math.pi/2)
    #genpos(self.node_pz,0.3,math.pi)
    #genpos(self.node_nz,0.3,3*math.pi/2)
    #genpos(self.node_ctr,0.7,0,radius=0,yscale=2)
    #self.probe.worldMatrix = mtx4.transMatrix(0,2,0)*self.node_ctr.modelnode.worldTransform.composed


    if hasattr(self,'spotlight1'):
      self.spotlight1.update(self.lighttime)
      self.spotlight2.update(self.lighttime)

###############################################################################

LIGHTING_APP().ezapp.mainThreadLoop()
