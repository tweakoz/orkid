#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
import numpy as np
from orkengine.core import vec3, vec4, quat, mtx4, Sphere, Transform
from orkengine.core import CrcStringProxy, lev2_pyexdir, thisdir
from orkengine import lev2
from obt.path import Path       
tokens = CrcStringProxy()
        
################################################################################

lev2_pyexdir.addToSysPath()
this_dir = thisdir()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData, createImposter
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie

################################################################################
IMP_DIM = 512
################################################################################

class ImposterApp(object):

  def __init__(self,is_stereo=None,extapp=None,envmap="cold"):
    super().__init__()
    self.time = 0.0
    if extapp==None:
      self.ezapp = lev2.OrkEzApp.create(self,ssaa=0)
      self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    else:
      self.ezapp = extapp.ezapp

    self.extapp = extapp
    setupUiCamera(app=self,eye=vec3(0,1,1)*25,tgt=vec3(0,0,0))

    self.is_stereo = is_stereo
    self.RENDERMODEL = "FWDPBRVRDM" if is_stereo else "ForwardPBR"
    self.envmap = envmap

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    if self.is_stereo and (self.extapp==None):
      self.vrdev = lev2.orkidvr.novr_device()
      self.vrdev.camera = "vrcam"
      self.vrdev.width = 1280
      self.vrdev.height = 1280
      self.vrdev.FOVD = 90
      
    ###################################
    # create scenegraph
    ###################################

    if self.extapp==None:
      params_dict = {
        "SkyboxTexPathStr": self.envmap,
        "SkyboxIntensity": 1.0,
        "DiffuseIntensity": 1.0,
        "SpecularIntensity": 10.0,
        "AmbientLevel": vec3(0),
        "DepthFogDistance": 10000.0,
      }
      params_dict["preset"] = self.RENDERMODEL

      ##################
      # create model / sg node
      ##################

      createSceneGraph(app=self,params_dict=params_dict)
      self.lyr_donly = self.scene.createLayer("depth_prepass")
      self.lyr_fwd = self.layer1
      self.fwd_layers = [self.lyr_fwd,self.lyr_donly]
     ###################################

    self.grid_data = createGridData(extent=1000)

    self.grid_data.shader_suffix = "_V3"
    self.grid_data.modcolor = vec3(1,1.2,1.3)*2
    self.grid_data.majorTileDim = 1.0
    self.grid_node = self.lyr_fwd.createDrawableNodeFromData("grid",self.grid_data)
    self.grid_node.sortkey = 100

    self.ball_model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.cookie1 = MyCookie("src://effect_textures/knob2.png")

  ##############################################
  # create imposter
  ##############################################

    imposter = createImposter( context=ctx,
                               radius=1.0,
                               filtertype=tokens.BILINEAR,
                               filterradius=3.0, 
                               detail=3,
                               shaderpath=this_dir/"i2.glfx",
                               shadertek="tek_imp",
                               layer=self.lyr_fwd,
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
    imp_pass.pipeline.bindParam(imp_mtl.param("time"), lambda: self.time*0.1)
    imp_pass.pipeline.bindParam(imp_mtl.param("reflectionPROBE"), tokens.RCFD_PBR_BLACK_CUBEMAP )
    imp_pass.pipeline.bindParam(imp_mtl.param("MapBrdfIntegration"), tokens.RCFD_PBR_BRDF_INTEGRATION_GGX )
    imp_pass.pipeline.bindParam(imp_mtl.param("SSAOMap"), tokens.RCFD_PBR_WHITE_2DMAP )
    imp_pass.pipeline.bindParam(imp_mtl.param("MapDiffuseEnv"), tokens.RCFD_PBR_DIFFUSE_ENV )
    imp_pass.pipeline.bindParam(imp_mtl.param("MapSpecularEnv"), tokens.RCFD_PBR_SPECULAR_ENV )
    imp_pass.pipeline.bindParam(imp_mtl.param("LightMapColors"), tokens.RCFD_PBR_LIGHTMAP_COLORS )
    imp_pass.pipeline.bindParam(imp_mtl.param("EyePostion"), tokens.RCFD_EYE_POSITION )
    imp_pass.pipeline.bindParam(imp_mtl.param("AmbientLevel"), vec3(0) )
    imp_pass.pipeline.bindParam(imp_mtl.param("SkyboxLevel"), 1.0 )
    imp_pass.pipeline.bindParam(imp_mtl.param("DiffuseLevel"), 1.0 )
    imp_pass.pipeline.bindParam(imp_mtl.param("SpecularLevel"), 1.0 )
    imp_pass.pipeline.bindParam(imp_mtl.param("RoughnessLevels"), 16.0 )

    #####################
    # user pass
    #####################

    if False:

      # warped(2D) feedback pass
      imposter.installFeedbackBlit(shaderpath=this_dir/"i1.glfx",
                                   shadertek="tek_upass" )
      
      upass = imposter.feedback_pass
      umtl  = imposter.feedback_mtl
      upass.pipeline.bindParam(umtl.param("mvp"), mtx4())
      upass.pipeline.bindParam(umtl.param("time"), lambda: self.time)
      upass.pipeline.bindParam(umtl.param("fbtex"), lambda: imposter.fb_tex )
      upass.pipeline.bindParam(umtl.param("rtgtex"), lambda: imposter.rtg_imp.texture(0))
      upass.pipeline.bindParam(umtl.param("depthtex"), lambda: imposter.rtg_imp.depth_buffer.texture)

    else:  

      imposter.installStandardBlit()

    self.imposter = imposter

    # debug shader state ?      
    #imposter.impdata.imp_pass.debug_shaderstate = True
    #imposter.impdata.blit_pass.debug_shaderstate = True

  ##############################################

  def onUiEvent(self,uievent):
    res = lev2.ui.HandlerResult()
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return res

  ################################################

  def onUpdate(self,updinfo):
    abstime = updinfo.absolutetime
    self.time = abstime
    #########################
    if (self.extapp==None):
      if self.is_stereo:
        x = math.sin(abstime*0.1)
        z = -math.cos(abstime*0.1)
        xf_hmd = mtx4.lookAt( vec3(x,0.5,z)*3.0,  # eye
                              vec3(0,0,0),        # tgt
                              vec3(0,1,0))        # up
        self.vrdev.setPoseMatrix("hmd",xf_hmd)
      self.scene.updateScene(self.cameralut) 
    #########################

  ################################################

  def onGpuUpdate(self,ctx):
    self.imposter.onGpuUpdate(ctx)
    findex = self.imposter.frame_index
    y = math.sin(findex*0.005)
    pos = vec3(0,y,0)
    #self.imposter.sgnode.worldTransform.translation = pos
    #self.imposter.impdata.enable_Lanczos_blit = ((int(findex)%800)<400)
    #print(self.imposter.impdata.enable_Lanczos_blit)
###############################################################################

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='scenegraph example')
  parser.add_argument("--stereo", action="store_true", help='enable stereo rendering')
  parser.add_argument("-e", "--envmap", type=str, default="cold", help='environment map')
  ################################################################################
  args = vars(parser.parse_args())
  is_stereo = args["stereo"]
  envmap = args["envmap"]
  ImposterApp(is_stereo=is_stereo,envmap=envmap).ezapp.mainThreadLoop()
