from orkengine.core import vec3, vec4, CrcStringProxy, Sphere
from orkengine import lev2 

tokens = CrcStringProxy()

def createImposter( context = None,
                    radius     = 1.0, 
                    detail     = 1, 
                    shaderpath = None,
                    shadertek  = None,
                    layer = None,
                    DIM = 128,
                    is_stereo = False,
                    filtertype = None,
                    filterradius = 2.0,
                    use_pbr = False
                    ):

  assert(shaderpath!=None)
  assert(shadertek!=None)
  assert(layer!=None)
  assert(context!=None)
  assert(filtertype!=None)

  class ImposterObject(object):

    def __init__(self):
      super().__init__()
      imp_mtl = lev2.FreestyleMaterial()
      imp_mtl.gpuInit(context,str(shaderpath))
      imp_mtl.rasterstate.setBlendingMacro(tokens.OFF)
      imp_mtl.rasterstate.culltest = tokens.PASS_FRONT
      imp_mtl.rasterstate.depthtest = tokens.LEQUALS
      imp_permu = lev2.FxPipelinePermutation()
      imp_permu.technique = imp_mtl.shader.technique(shadertek)
      imp_pipeline = imp_mtl.fxcache.findPipeline(imp_permu)
      imp_pipeline.name = "imppasspipe"
      imp_pipeline.sharedMaterial = imp_mtl
      self.impdata = lev2.ImposterDrawableData()
      self.impdata.shape = Sphere(vec3(0), radius)
      self.impdata.detail = detail
      self.impdata.imp_pass.pipeline = imp_pipeline
      self.sgnode = layer.createDrawableNodeFromData("imp1",self.impdata)
      self.sgnode.worldTransform.scale = 1
      self.sgnode.worldTransform.translation = vec3(0,0,0)
      self.imp_mtl = imp_mtl
      self.is_stereo = is_stereo
      self.frame_index = 0

      self.impdata.filter_type = filtertype
      self.impdata.filter_radius = filterradius

      #####################
      # imposter rtgroup
      #####################

      imp_pass = self.impdata.imp_pass
      rtg_imps = [self.createRGBRTG("imprtg0")]
      if is_stereo:
        rtg_imps += [self.createRGBRTG("imprtg1")]
      self.rtg_imp = rtg_imps[0]
      imp_pass.rtgroup = self.rtg_imp
      imp_pass.pipeline = imp_pipeline
      self.rtg_imps = rtg_imps
    
      #####################

      imp_pass.pipeline.bindParam(imp_mtl.param("m"),  tokens.RCFD_M )
      imp_pass.pipeline.bindParam(imp_mtl.param("mrot"),  tokens.RCFD_Model_Rot )
      imp_pass.pipeline.bindParam(imp_mtl.param("v"),  tokens.RCFD_Camera_V_Mono )
      imp_pass.pipeline.bindParam(imp_mtl.param("mv"),  tokens.RCFD_Camera_MV_Mono )
      imp_pass.pipeline.bindParam(imp_mtl.param("p"),  tokens.RCFD_Camera_P_Mono )
      imp_pass.pipeline.bindParam(imp_mtl.param("vp"),  tokens.RCFD_Camera_VP_Mono )
      imp_pass.pipeline.bindParam(imp_mtl.param("mvp"),  tokens.RCFD_Camera_MVP_Mono )
      imp_pass.pipeline.bindParam(imp_mtl.param("inv_v"), tokens.RCFD_Camera_IV_Mono )
      imp_pass.pipeline.bindParam(imp_mtl.param("inv_p"), tokens.RCFD_Camera_IP_Mono )
      imp_pass.pipeline.bindParam(imp_mtl.param("inv_vp"), tokens.RCFD_Camera_IVP_Mono )
      imp_pass.pipeline.bindParam(imp_mtl.param("ViewportSize"), tokens.FBI_RTG_DIM )
      imp_pass.pipeline.bindParam(imp_mtl.param("InvViewportSize"), tokens.FBI_RTG_INVDIM )
      imp_pass.pipeline.bindParam(imp_mtl.param("raydir"), tokens.RCFD_Camera_ZNORMAL_Mono )

      #####################

      if use_pbr:
        imp_pass.pipeline.bindParam(imp_mtl.param("LightMapColors"), tokens.RCFD_PBR_LIGHTMAP_COLORS )
        imp_pass.pipeline.bindParam(imp_mtl.param("LightMapArray"), tokens.RCFD_PBR_BLACK_LIGHTMAP_ARRAY )
        imp_pass.pipeline.bindParam(imp_mtl.param("reflectionPROBE"), tokens.RCFD_PBR_BLACK_CUBEMAP )
        imp_pass.pipeline.bindParam(imp_mtl.param("MapBrdfIntegration"), tokens.RCFD_PBR_BRDF_INTEGRATION_GGX )
        imp_pass.pipeline.bindParam(imp_mtl.param("SSAOMap"), tokens.RCFD_PBR_WHITE_2DMAP )
        # THE AMBIENT is nine L2 coefficients now (W4-S9), not a map: the bake
        # has to bind them or an impostor is lit by whatever the block held.
        imp_pass.pipeline.bindParam(imp_mtl.param("EnvSH"), tokens.RCFD_PBR_ENV_SH )
        imp_pass.pipeline.bindParam(imp_mtl.param("EnvSHValid"), tokens.RCFD_PBR_ENV_SH_VALID )
        imp_pass.pipeline.bindParam(imp_mtl.param("MapSpecularEnv"), tokens.RCFD_PBR_SPECULAR_ENV )
        # outgoing specular set + blend weight (procedural refilter crossfade);
        # aliases the bind above whenever no fade is running.
        imp_pass.pipeline.bindParam(imp_mtl.param("MapSpecularEnvPrev"), tokens.RCFD_PBR_SPECULAR_ENV_PREV )
        imp_pass.pipeline.bindParam(imp_mtl.param("EnvBlendWeight"), tokens.RCFD_PBR_ENV_BLEND_WEIGHT )
        imp_pass.pipeline.bindParam(imp_mtl.param("EyePostion"), tokens.RCFD_EYE_POSITION )
        imp_pass.pipeline.bindParam(imp_mtl.param("AmbientLevel"), vec3(0) )
        imp_pass.pipeline.bindParam(imp_mtl.param("SkyboxLevel"), 1.0 )
        imp_pass.pipeline.bindParam(imp_mtl.param("DiffuseLevel"), 1.0 )
        imp_pass.pipeline.bindParam(imp_mtl.param("SpecularLevel"), 1.0 )
        imp_pass.pipeline.bindParam(imp_mtl.param("RoughnessLevels"), 16.0 )
        imp_pass.pipeline.bindStorage(imp_mtl.storage("storage_fwd_lighting"), tokens.LMGR_LIGHTING_STORAGE )
        imp_pass.pipeline.bindParam(imp_mtl.param("point_light_count"), tokens.LMGR_ACTIVE_UNTEXTURED_POINTLIGHT_COUNT )
        imp_pass.pipeline.bindParam(imp_mtl.param("spot_light_count"), tokens.LMGR_ACTIVE_TEXTURED_SPOTLIGHT_COUNT )
        imp_pass.pipeline.bindParam(imp_mtl.param("light_cookie_colors"), tokens.LMGR_ACTIVE_TEXTURED_SPOTLIGHT_COLOR_COOKIES )
        imp_pass.pipeline.bindParam(imp_mtl.param("light_cookie_depths"), tokens.LMGR_ACTIVE_TEXTURED_SPOTLIGHT_DEPTH_COOKIES )

    #######################################################

    def onGpuUpdate(self,ctx):
      self.frame_index += 1

    #######################################################
    
    def createRGBRTG(self,name):
      rtg = lev2.RtGroup(context,DIM,DIM)
      rtg.name = name
      rtg.createBuffer(tokens.RGBA32F,tokens.color)
      rtg.createDepthBuffer(tokens.Z32F,True)
      return rtg

    #######################################################
    # standard feedback (ping-ping) blit pass
    #  with user specified shader
    #######################################################

    def installFeedbackBlit(self,shaderpath=None,shadertek=None):

      upass = lev2.ImposterPassData()
      
      rtg_feedback = [self.createRGBRTG("FB0L"),self.createRGBRTG("FB1L")]
      if self.is_stereo:
        rtg_feedback += [self.createRGBRTG("FB0R"),self.createRGBRTG("FB1R")]

      self.fb_tex = rtg_feedback[0].texture(0)
            
      upass_mtl = lev2.FreestyleMaterial()
      upass_mtl.gpuInit(context,shaderpath)
      upass_mtl.rasterstate.setBlendingMacro(tokens.OFF)
      upass_mtl.rasterstate.culltest = tokens.OFF
      upass_mtl.rasterstate.depthtest = tokens.OFF
      upass_mtl.rasterstate.culltest = tokens.OFF
      upass_mtl.rasterstate.depthtest = tokens.OFF
      upass_mtl.rasterstate.writeMaskRGB = True
      upass_mtl.rasterstate.writeMaskA = True
      upass_mtl.rasterstate.writeMaskZ = True
      upass_permu = lev2.FxPipelinePermutation()
      upass_permu.technique = upass_mtl.shader.technique(shadertek)
      upass.pipeline = upass_mtl.fxcache.findPipeline(upass_permu)
      upass.pipeline.name = "upasspipe"
      upass.pipeline.sharedMaterial = upass_mtl
      self.impdata.user_passes = [upass]
      upass.enabled = True

      def _on_post_render():
        eye_index = context.topRCFD.userprops.eye_index # 0: left, 1: right
        eye_index = 0 if (eye_index==None) else eye_index
        self.rtg_imp = self.rtg_imps[eye_index]
        self.impdata.imp_pass.rtgroup = self.rtg_imp
        base = eye_index * 2
        if (self.frame_index % 2) == 0:
          upass.rtgroup = rtg_feedback[base+1] # upass renders to fb1
          self.impdata.blit_pass.userdata.color_rtg = rtg_feedback[base+1] # blit_pass reads fb1
          self.fb_tex = rtg_feedback[base+0].texture(0) # upass reads fb0
        else:
          upass.rtgroup = rtg_feedback[base+0] # upass renders to fb0
          self.impdata.blit_pass.userdata.color_rtg = rtg_feedback[base+0] # blit_pass reads fb0
          self.fb_tex = rtg_feedback[base+1].texture(0) # upass reads fb1

      upass.onPostRender(_on_post_render)
      _on_post_render() # first time init
      
      self.feedback_pass = upass
      self.feedback_mtl = upass_mtl

    #######################################################

    def installStandardBlit(self):
      def _on_post_render():
        eye_index = context.topRCFD.userprops.eye_index # 0: left, 1: right
        eye_index = 0 if (eye_index==None) else eye_index
        self.rtg_imp = self.rtg_imps[eye_index]
        self.impdata.imp_pass.rtgroup = self.rtg_imp
        self.impdata.blit_pass.userdata.color_rtg = self.rtg_imp 
        self.impdata.blit_pass.userdata.depth_rtg = self.rtg_imp 
      self.impdata.blit_pass.onPostRender(_on_post_render)
      _on_post_render() # first time init

    #######################################################

  impobject = ImposterObject()
  return impobject