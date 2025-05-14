#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
from orkengine.core import vec3, vec4, quat, mtx4
from orkengine.core import dfrustum, dvec4, fmtx4_to_dmtx4 
from orkengine.core import lev2_pyexdir, Transform
from orkengine.core import CrcStringProxy, thisdir, VarMap
from orkengine import lev2

################################################################################

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie

sys.path.append(str(thisdir()/".."/"particles"))
from _ptc_harness import *

################################################################################
parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('--stereo', action='store_true', help='stereo mode')
################################################################################
args = vars(parser.parse_args())
################################################################################
stereo = args["stereo"]
mono = not stereo
################################################################################
tokens = CrcStringProxy()

class LIGHTING_APP(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self,ssaa=0,msaa=0, fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.materials = set()

    if stereo:
      self.cameralut = lev2.CameraDataLut()
      self.vrcamera = lev2.CameraData()
      self.cameralut.addCamera("vrcam",self.vrcamera)
    else:
      setupUiCamera(app=self,eye=vec3(0,12,15))

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    if stereo:
      self.vrdev = orkidvr.novr_device()
      self.vrdev.camera = "vrcam"

    sceneparams = VarMap() 

    
    sceneparams.SkyboxIntensity = float(1)
    sceneparams.SpecularIntensity = float(1)
    sceneparams.DiffuseIntensity = float(1)
    sceneparams.AmbientLight = vec3(0.07)
    sceneparams.DepthFogDistance = float(10000)
    sceneparams.supersample = 1

    if mono:
      sceneparams.preset = "ForwardPBR"
    else:
      sceneparams.preset = "FWDPBRVR"

    ###################################
    postNode1 = lev2.PostFxNodeHSVG()
    postNode1.gpuInit(ctx,8,8);
    postNode1.addToSceneVars(sceneparams,"PostFxChain")
    postNode1.saturation = 0.75
    postNode1.gamma = 1.2
    self.post_node1 = postNode1
    ###################################

    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_dprobe = self.scene.createLayer("depth_probe")
    self.layer_probe = self.scene.createLayer("probe")
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.layer_all = self.scene.createLayer("All")
    self.fwd_layers = [self.layer_fwd,self.layer_donly]
    self.pbr_common = self.scene.pbr_common
    self.pbr_common.useFloatColorBuffer = True

    DEPTH_LAYERS = [self.layer_donly,self.layer_dprobe]
    FINAL_LAYERS = [self.layer_fwd,self.layer_donly]
    COLOR_LAYERS = [self.layer_fwd,self.layer_probe]
    ALL_LAYERS = [self.layer_fwd,self.layer_probe,self.layer_donly]

    ###################################

    if False:
      createDefaultSpriteSystem(app=self)
      self.particlenode.worldTransform.translation = vec3(0,4,10)
      self.particlenode.worldTransform.scale = 1/10.0
      self.layer_probe.addDrawableNode(self.particlenode)
      self.SPRI.material.colorIntensity = 0.3

    ###################################

    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    normal = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")
    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    model.debugRenderingModel = tokens.NONE # tokens.FORWARD_PBR
    for mesh in model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.assignImages(
          ctx,
          color = white,
          normal = normal,
          mtlruf = white,
          doConform=True
        )
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
    self.grid_data.texturepath = "src://effect_textures/white_64.dds"
    self.grid_drawable = self.grid_data.createDrawable()
    self.grid_node = self.scene.createDrawableNodeOnLayers(COLOR_LAYERS,"grid-node",self.grid_drawable)
    #self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    ###################################

    lmgr = self.scene.lightingmanager
    color_cookies = lmgr.spot_cookies_color
    depth_cookies = lmgr.spot_cookies_depth
    color_cookies.needsIrradianceCache = True
    color_cookies.resize(1024,1024,5,tokens.RGB8,True)
    depth_cookies.resize(1024,1024,5,tokens.Z32F,True)

    cookie_paths = [
      "src://effect_textures/L0D.png",
      "src://effect_textures/knob2.png",
      "src://effect_textures/ptc4.png",
      "src://effect_textures/ptc3.png",
      "src://effect_textures/adama.png",
    ]
    ccooks = [color_cookies.load(path) for path in cookie_paths]
    dcooks = [depth_cookies.slice(i) for i in range(5)]
    colors = [vec3(0,100,2000), 
              vec3(500,0,0), 
              vec3(0,100,0), 
              vec3(-100,-100,-100), 
              vec3(100,100,100)]
    indices = [i for i in range(5)]
    frqs = [0.17, 0.37, -0.27, 0.07, -0.08]
    fovbases = [25, 35, 20, 25, 25]
    fovamps = [25, 35, 25, 25, 25]
    voffsets = [16, 16, 16, 16, 16]
    vscales = [8, 8, 14, 8, 8]
    radii = [16, 16, 16, 16, 10]
    if True:
      shadow_size = 2048
      shadow_bias = 1e-3
      kwargs = {
        "app":self,
        "model":model,
        "bias":shadow_bias,
        "dim":shadow_size,
        "layers":COLOR_LAYERS,
      }
      self.spotlights = [] 
      for i in range(5):
        s = MySpotLight( **kwargs, 
                         index=indices[i],
                         frq=frqs[i],
                         color=colors[i],
                         cookie=ccooks[i],
                         depth_cookie=dcooks[i],
                         fovbase=fovbases[i],
                         fovamp=fovamps[i],
                         voffset=voffsets[i],
                         vscale=vscales[i],
                         radius=radii[i])
        self.spotlights.append(s)
   ##############################################

    self.probe = lev2.LightProbe()
    self.probe.type = tokens.REFLECTION
    self.probe.imageDim = 1024
    self.probe.worldMatrix = mtx4.transMatrix(0,4,0)
    self.probe.name = "probe1"
    self.probe_node = self.layer_all.createLightProbeNode("probe",self.probe)

  ##############################################

  def onUiEvent(self,uievent):
    handled = False
    if mono:
      handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return lev2.ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):
    self.lighttime = updinfo.absolutetime
    if stereo:
      self.vrdev.FOV = 90
      self.vrdev.IPD = 0.065
      self.vrdev.near = 0.1
      self.vrdev.far = 1e5
      xf = Transform()
      xf.lookAt(vec3(0,5,-10),vec3(0,5,0),vec3(0,1,0))
      mtx_hmd = xf.composed
      self.vrdev.setPoseMatrix("hmd",mtx_hmd)
      
    
    
    self.scene.updateScene(self.cameralut) 
    

  ################################################

  def onGpuUpdate(self,ctx):
    def genpos(node,frq,offset,radius=5,yscale=2):
      phase = offset+self.lighttime*frq
      x = math.sin(phase)*radius
      z = math.cos(phase)*radius
      y = 4 + math.cos(phase*2.7)*1
      node.modelnode.worldTransform.translation = vec3(x,y,z)
    
    self.probe.invalidate()
    frq = 0.1
    genpos(self.node_px,frq,0)
    genpos(self.node_nx,frq,math.pi/2)
    genpos(self.node_pz,frq,math.pi)
    genpos(self.node_nz,frq,3*math.pi/2)
    genpos(self.node_ctr,frq*2.1,0,radius=0,yscale=2)
    self.probe.worldMatrix = mtx4.transMatrix(0,2,0)*self.node_ctr.modelnode.worldTransform.composed
    #self.probe.worldMatrix = self.node_ctr.modelnode.worldTransform.composed


    if hasattr(self,'spotlights'):
      for s in self.spotlights:
        s.update(self.lighttime)

###############################################################################

LIGHTING_APP().ezapp.mainThreadLoop()
