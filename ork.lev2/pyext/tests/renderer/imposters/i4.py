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
this_dir.addToSysPath()

import _boilerplate as boilerplate

from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData, createImposter
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie

################################################################################
IMP_DIM = 768
################################################################################

class ImposterApp(boilerplate.ImposterBaseApp):

  def __init__(self,is_stereo=False,extapp=None,envmap="cold",statedebug=False):
    super().__init__(is_stereo=is_stereo,extapp=extapp,envmap=envmap,statedebug=statedebug)

  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
      
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
                               shaderpath=this_dir/"i4.glfx",
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
    imp_pass.pipeline.bindParam(imp_mtl.param("time"), lambda: self.time*2.3)
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

    imposter.installStandardBlit()

    self.imposter = imposter

    # debug shader state ?      
    #imposter.impdata.imp_pass.debug_shaderstate = True
    #imposter.impdata.blit_pass.debug_shaderstate = True

  ##############################################

  def onUpdate(self,updinfo):
    super().onUpdate(updinfo)

  ################################################

  def onGpuUpdate(self,ctx):
    self.imposter.onGpuUpdate(ctx)
    findex = self.imposter.frame_index
    x = math.sin(findex*0.005)
    z = -math.cos(findex*0.005)
    pos = vec3(x,0,z)
    #self.imposter.sgnode.worldTransform.translation = pos
    #self.imposter.impdata.enable_Lanczos_blit = ((int(findex)%800)<400)
    #print(self.imposter.impdata.enable_Lanczos_blit)
###############################################################################

if __name__ == "__main__":
  boilerplate.run(ImposterApp)
