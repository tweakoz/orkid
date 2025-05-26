#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
from orkengine.core import vec3, vec4, quat, mtx4, dfrustum, dvec4, fmtx4_to_dmtx4, CrcStringProxy
from orkengine.core import lev2_pyexdir, Transform, thisdir
from orkengine import lev2

################################################################################

lev2_pyexdir.addToSysPath()
this_dir = thisdir()

from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData, createImposter
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie

tokens = CrcStringProxy()
IMP_DIM = 768
################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('-S', '--stateDebugger', action="store_true", help='Graphics state debugger')
################################################################################
args = vars(parser.parse_args())
statedebug = args["stateDebugger"]
################################################################################

class StereoApp1(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self,ssaa=0,msaa=0)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(0,12,15))
    self.is_stereo = False
    self.time = 0.0
    
    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    params_dict = {
      "SkyboxTexPathStr": "cold",
      "SkyboxIntensity": float(0.5),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.1),
      "DepthFogDistance": float(10000),
      "supersample": "1",
    }

    #createSceneGraph(app=self,rendermodel="DeferredPBR",params_dict=params_dict)
    createSceneGraph(app=self,rendermodel="ForwardPBR",params_dict=params_dict)

    self.pbr_common = self.scene.pbr_common
    self.pbr_common.useDepthPrepass = True

    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.layer1
    self.fwd_layers = [self.layer_fwd,self.layer_donly]

    ###################################

    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    model.debugRenderingModel = tokens.ALL if statedebug else tokens.NONE
    model.debugPassID = tokens.PRIMARY if statedebug else tokens.NONE
    model.debugSubPassID = tokens.ALL if statedebug else tokens.NONE
    self.drawable_model = model.createDrawable()
    self.modelnode = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 1
    self.modelnode.worldTransform.translation = vec3(0,2,0)

    ###################################

    self.grid_data = createGridData(extent=1000.0)
    self.grid_data.shader_suffix = "_V4"
    self.grid_data.modcolor = vec3(1.0)
    self.grid_data.intensityA = 1.0
    self.grid_data.intensityB = 0.97
    self.grid_data.intensityC = 0
    self.grid_data.intensityD = 0
    self.grid_data.lineWidth = 0.025
    self.grid_node = self.layer1.createDrawableNodeFromData("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ##############################################
  # create imposter
  ##############################################
    if True:
      imposter = createImposter( context=ctx,
                                 radius=1.0,
                                 filtertype=tokens.BILINEAR,
                                 filterradius=3.0, 
                                 detail=3,
                                 shaderpath=this_dir/"i4.glfx",
                                 shadertek="tek_imp",
                                 layer=self.layer_fwd,
                                 DIM = IMP_DIM,
                                 is_stereo=self.is_stereo )
      
      imp_mtl = imposter.imp_mtl
      imp_pass = imposter.impdata.imp_pass
      imp_pass.pipeline.bindParam(imp_mtl.param("m"),  tokens.RCFD_M )
      imp_pass.pipeline.bindParam(imp_mtl.param("vp"),  tokens.RCFD_Camera_VP_Mono )
      imp_pass.pipeline.bindParam(imp_mtl.param("inv_vp"), tokens.RCFD_Camera_IVP_Mono )
      imp_pass.pipeline.bindParam(imp_mtl.param("raydir"), tokens.RCFD_Camera_ZNORMAL_Mono )
      imp_pass.pipeline.bindParam(imp_mtl.param("ViewportSize"), tokens.FBI_RTG_DIM )
      imp_pass.pipeline.bindParam(imp_mtl.param("InvViewportSize"), tokens.FBI_RTG_INVDIM )
      imp_pass.pipeline.bindParam(imp_mtl.param("time"), lambda: self.time*3.0)
      imp_pass.pipeline.bindParam(imp_mtl.param("reflectionPROBE"), tokens.RCFD_PBR_BLACK_CUBEMAP )
      imp_pass.pipeline.bindParam(imp_mtl.param("MapBrdfIntegration"), tokens.RCFD_PBR_BRDF_INTEGRATION_GGX )
      imp_pass.pipeline.bindParam(imp_mtl.param("SSAOMap"), tokens.RCFD_PBR_WHITE_2DMAP )
      imp_pass.pipeline.bindParam(imp_mtl.param("MapDiffuseEnv"), tokens.RCFD_PBR_DIFFUSE_ENV )
      imp_pass.pipeline.bindParam(imp_mtl.param("MapSpecularEnv"), tokens.RCFD_PBR_SPECULAR_ENV )
      imp_pass.pipeline.bindParam(imp_mtl.param("LightMapColors"), tokens.RCFD_PBR_LIGHTMAP_COLORS )
      imp_pass.pipeline.bindParam(imp_mtl.param("LightMapArray"), tokens.RCFD_PBR_BLACK_LIGHTMAP_ARRAY )
      imp_pass.pipeline.bindParam(imp_mtl.param("EyePostion"), tokens.RCFD_EYE_POSITION )
      imp_pass.pipeline.bindParam(imp_mtl.param("AmbientLevel"), vec3(0) )
      imp_pass.pipeline.bindParam(imp_mtl.param("SkyboxLevel"), 1.0 )
      imp_pass.pipeline.bindParam(imp_mtl.param("DiffuseLevel"), 1.0 )
      imp_pass.pipeline.bindParam(imp_mtl.param("SpecularLevel"), 1.0 )
      imp_pass.pipeline.bindParam(imp_mtl.param("RoughnessLevels"), 16.0 )
      imp_pass.pipeline.bindUniBlock(imp_mtl.uniblk("ublk_frg_fwd_lighting"), tokens.LMGR_LIGHTING_UBO )
      imp_pass.pipeline.bindParam(imp_mtl.param("point_light_count"), tokens.LMGR_ACTIVE_UNTEXTURED_POINTLIGHT_COUNT )
      imp_pass.pipeline.bindParam(imp_mtl.param("spot_light_count"), tokens.LMGR_ACTIVE_TEXTURED_SPOTLIGHT_COUNT )
      imp_pass.pipeline.bindParam(imp_mtl.param("light_cookie_colors"), tokens.LMGR_ACTIVE_TEXTURED_SPOTLIGHT_COLOR_COOKIES )
      imp_pass.pipeline.bindParam(imp_mtl.param("light_cookie_depths"), tokens.LMGR_ACTIVE_TEXTURED_SPOTLIGHT_DEPTH_COOKIES )

      imposter.installStandardBlit()

      self.imposter = imposter

    # debug shader state ?      
    #imposter.impdata.imp_pass.debug_shaderstate = True
    #imposter.impdata.blit_pass.debug_shaderstate = True

    ###################################

    lmgr = self.scene.lightingmanager
    color_cookies = lmgr.spot_cookies_color
    depth_cookies = lmgr.spot_cookies_depth
    color_cookies.needsIrradianceCache = True
    color_cookies.resize(1024,1024,5,tokens.RGB8,True)
    depth_cookies.resize(1024,1024,5,tokens.Z32F,True)

    cookie1 = color_cookies.load("src://effect_textures/L0D.png")
    cookie2 = color_cookies.load("lev2://textures/transponder24.png")
    cookie3 = color_cookies.load("src://effect_textures/knob2.png")
    cookie4 = color_cookies.load("src://effect_textures/knob2.png")
    depth1 = depth_cookies.slice(0)
    depth2 = depth_cookies.slice(1)
    depth3 = depth_cookies.slice(2)
    depth4 = depth_cookies.slice(3)
    shadow_size = 2048
    shadow_bias = 1e-4
    intens_scale = 0.5
    speed_scale = 0.5
    self.spotlight1 = MySpotLight(index=0,app=self,model=model,frq=0.17*speed_scale,color=vec3(0,5500,0)*intens_scale,cookie=cookie1,depth_cookie=depth1,fovbase=60.0,fovamp=20.0,voffset=15,vscale=13,bias=shadow_bias,dim=shadow_size,radius=1.2)
    self.spotlight2 = MySpotLight(index=1,app=self,model=model,frq=0.37*speed_scale,color=vec3(5000,0,0)*intens_scale,cookie=cookie2,depth_cookie=depth2,fovbase=60.0,fovamp=20.0,voffset=15,vscale=13,bias=shadow_bias,dim=shadow_size,radius=1.5)
    self.spotlight3 = MySpotLight(index=2,app=self,model=model,frq=0.57*speed_scale,color=vec3(800)*intens_scale,cookie=cookie3,depth_cookie=depth3,fovbase=60.0,fovamp=20.0,voffset=15,vscale=13,bias=shadow_bias,dim=shadow_size,radius=2.0)
    self.spotlight4 = MySpotLight(index=3,app=self,model=model,frq=0.97*speed_scale,color=vec3(0,0,600)*intens_scale,cookie=cookie4,depth_cookie=depth4,fovbase=70.0,fovamp=20.0,voffset=3,vscale=2,bias=shadow_bias,dim=shadow_size,radius=7)

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return lev2.ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):
    self.lighttime = updinfo.absolutetime
    self.time = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) 

  def onGpuUpdate(self,ctx):
    if hasattr(self,"imposter"):
      self.imposter.onGpuUpdate(ctx)
      z = math.sin(self.imposter.frame_index*0.01)*2.0
      self.imposter.sgnode.worldTransform.translation = vec3(0,0.1,z)
    self.spotlight1.update(self.lighttime)
    self.spotlight2.update(self.lighttime)
    self.spotlight3.update(self.lighttime)
    self.spotlight4.update(self.lighttime)
    if hasattr(self,"sgnode_frustum"):
      self.layer1.removeDrawableNode(self.sgnode_frustum )

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
