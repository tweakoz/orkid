#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, argparse
import _boilerplate as boilerplate
from orkengine.core import vec3, mtx4, CrcStringProxy, thisdir
from orkengine import lev2
from ork.app.std_scenegraph import StandardSceneGraphComponent, StdSpotLight
from ork.app.std_imposter import createImposter
from ork.app.std_grid import createGridData

################################################################################

tokens = CrcStringProxy()
        
################################################################################
IMP_DIM = 128
################################################################################

class ImposterApp(boilerplate.ImposterBaseApp):

  def __init__(self,envmap="cold"):
    super().__init__()
    self.time = 0.0

    grid_data = createGridData(extent=1000.0)
    grid_data.shader_suffix = "_V3"
    grid_data.modcolor = vec3(1,1.2,1.3)*2

    super().__init__(ssaa=1,
                     grid_data=grid_data,
                     sg_params = {
                        "SkyboxTexPathStr": envmap,
                        "SkyboxIntensity": 0.5,
                        "SpecularIntensity": 1.0,
                        "DiffuseIntensity": 1.0,
                        "AmbientLight": vec3(0.0),
                        "DepthFogDistance": float(1e5),
                        "use_float_color_buffer": True
                     })

  ##############################################

  def _onGpuInit(self,ctx):
      
    SGC = self.findComponentsByClass(StandardSceneGraphComponent)[0]
    SG = SGC.scenegraph      

    self.ball_model = lev2.XgmModel("data://tests/pbr_calib.glb")

  ##############################################
  # create imposter
  ##############################################

    imposter = createImposter( context=ctx,
                               radius=1.0,
                               filtertype=tokens.BILINEAR,
                               filterradius=3.0, 
                               detail=2,
                               shaderpath=thisdir()/"i1.fxv2",
                               shadertek="tek_imp1",
                               layer=SGC.layer_fwd,
                               DIM = IMP_DIM )
    
    imp_mtl = imposter.imp_mtl
    imp_pass = imposter.impdata.imp_pass
    imp_pass.pipeline.bindParam(imp_mtl.param("mvp"),  mtx4())
    imp_pass.pipeline.bindParam(imp_mtl.param("time"), lambda: self.time)

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

    SG.lightingmanager.gpuInit(ctx)
    
  ################################################

  def _onUpdate(self,updinfo):
    self.time = updinfo.absolutetime
      #self.scene.updateScene(self.cameralut) 
    #########################

  ################################################

  def _onGpuUpdate(self,ctx):
    self.imposter.onGpuUpdate(ctx)
    findex = self.imposter.frame_index
    y = math.sin(findex*0.005)
    pos = vec3(0,y,0)
    #self.imposter.sgnode.worldTransform.translation = pos
    #self.imposter.impdata.enable_Lanczos_blit = ((int(findex)%800)<400)
    #print(self.imposter.impdata.enable_Lanczos_blit)
###############################################################################

if __name__ == "__main__":
  boilerplate.run(ImposterApp)
