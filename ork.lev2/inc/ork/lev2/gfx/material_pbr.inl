////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

//#include <boost/filesystem.hpp>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/fx_pipeline.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/varmap.inl>

//using namespace boost::filesystem;

namespace ork::lev2 {
using namespace std::literals;

///////////////////////////////////////////////////////////////////////////////

class PBRMaterial;
using pbrmaterial_ptr_t      = std::shared_ptr<PBRMaterial>;
using pbrmaterial_constptr_t = std::shared_ptr<const PBRMaterial>;

struct PbrMatrixBlockApplicator : public MaterialInstApplicator {
  MaterialInstItemMatrixBlock* _matrixblock = nullptr;
  const PBRMaterial* _pbrmaterial           = nullptr;
  void ApplyToTarget(Context* pTARG) final;
  static PbrMatrixBlockApplicator* getApplicator();
};

///////////////////////////////////////////////////////////////////////////////

struct FilteredEnvMap {
  std::shared_ptr<RtGroup> _rtgroup;
  std::shared_ptr<RtBuffer> _rtbuffer;
  Texture* _texture = nullptr;
};
typedef std::shared_ptr<FilteredEnvMap> filtenvmapptr_t;
///////////////////////////////////////////////////////////////////////////////

class PBRMaterial final : public GfxMaterial {

  DeclareConcreteX(PBRMaterial, GfxMaterial);

  static fxpipeline_ptr_t _createFxPipeline(const FxPipelinePermutation& permu, const PBRMaterial* mtl);
  fxpipeline_ptr_t _createFxPipelineFWD(const FxPipelinePermutation& permu) const;
  fxpipeline_ptr_t _createFxPipelineDEF(const FxPipelinePermutation& permu) const;
  fxpipeline_ptr_t _createFxPipelineDPP(const FxPipelinePermutation& permu) const;
  fxpipeline_ptr_t _createFxPipelinePIK(const FxPipelinePermutation& permu) const;
  fxpipeline_ptr_t _createFxPipelineUNL(const FxPipelinePermutation& permu) const;
  fxpipeline_ptr_t _createFxPipelineSKY(const FxPipelinePermutation& permu) const;
  fxpipeline_ptr_t _createFxPipelineVTX(const FxPipelinePermutation& permu) const;

public:
  PBRMaterial(Context* targ);
  PBRMaterial();
  ~PBRMaterial();

  void setTextureBaseName(std::string basename) {
    _textureBaseName = basename;
  }

  ////////////////////////////////////////////

  pbrmaterial_ptr_t clone() const;
  void addBasicStateLambda(fxpipeline_ptr_t pipe);
  void addLightingLambda(fxpipeline_ptr_t pipe);
  void addBasicStateLambda();
  void addLightingLambda();

  ////////////////////////////////////////////

  static FxShaderParamBuffer* pointLightDataBuffer(Context* targ);
  static FxShaderParamBuffer* boneDataBuffer(Context* targ);

  static texture_ptr_t brdfIntegrationMap(Context* targ);
  static texture_ptr_t filterSpecularEnvMap(texture_ptr_t rawenvmap, Context* targ, bool equirectangular);
  static texture_ptr_t filterDiffuseEnvMap(texture_ptr_t rawenvmap, Context* targ, bool equirectangular);

  ////////////////////////////////////////////

  void begin(const RenderContextFrameData& RCFD);
  void end(const RenderContextFrameData& RCFD);

  ////////////////////////////////////////////

  bool BeginPass(Context* targ, int iPass = 0) final;
  void EndPass(Context* targ) final;
  int BeginBlock(Context* targ, const RenderContextInstData& RCID) final;
  void EndBlock(Context* targ) final;
  void gpuInit(Context* targ) final;
  void gpuUpdate(Context* context) final;
  void Update() final;
  void BindMaterialInstItem(MaterialInstItem* pitem) const override;
  void UnBindMaterialInstItem(MaterialInstItem* pitem) const override;
  void UpdateMVPMatrix(Context* pTARG) final;
  void UpdateMMatrix(Context* pTARG) final;

  void forceEmissive();
  ////////////////////////////////////////////
  void conformImages();
  void assignImages(  lev2::Context* ctx,   //
                      image_ptr_t color,    //
                      image_ptr_t normal,   // 
                      image_ptr_t mtlruf,   // 
                      image_ptr_t emissive, // 
                      image_ptr_t ambocc,
                      bool do_conform = false);  
  ////////////////////////////////////////////
  fxpipelinecache_constptr_t _doFxPipelineCache(fxpipelinepermutation_set_constptr_t perms) const final;
  ////////////////////////////////////////////
  //void setupCamera(const RenderContextFrameData& RCFD);
  ////////////////////////////////////////////
  fxshaderasset_constptr_t _asset_shader;
  freestyle_mtl_ptr_t _as_freestyle;
  textureassetptr_t _asset_texcolor;
  textureassetptr_t _asset_texnormal;
  textureassetptr_t _asset_mtlruf;
  textureassetptr_t _asset_emissive;

  ////////////////////////////////////////////

  FxShader* _shader                      = nullptr;
  Context* _initialTarget                = nullptr;
  fxparam_constptr_t _paramM             = nullptr;
  fxparam_constptr_t _paramV             = nullptr;
  fxparam_constptr_t _paramP             = nullptr;
  fxparam_constptr_t _paramIP            = nullptr;
  fxparam_constptr_t _paramVP            = nullptr;
  fxparam_constptr_t _paramIVP           = nullptr;
  fxparam_constptr_t _paramVL            = nullptr;
  fxparam_constptr_t _paramVR            = nullptr;
  fxparam_constptr_t _paramVPL           = nullptr;
  fxparam_constptr_t _paramVPR           = nullptr;
  fxparam_constptr_t _paramIVPL          = nullptr;
  fxparam_constptr_t _paramIVPR          = nullptr;
  fxparam_constptr_t _paramMVP           = nullptr;
  fxparam_constptr_t _paramMVPL          = nullptr;
  fxparam_constptr_t _paramMVPR          = nullptr;
  fxparam_constptr_t _paramMV            = nullptr;
  fxparam_constptr_t _paramMROT          = nullptr;
  fxparam_constptr_t _paramMapDepth      = nullptr;
  fxparam_constptr_t _paramMapLinearDepth      = nullptr;

  fxparam_constptr_t _paramMapCNMREA      = nullptr;

  fxparam_constptr_t _parInvViewSize     = nullptr;
  fxparam_constptr_t _parMetallicFactor  = nullptr;
  fxparam_constptr_t _parRoughnessFactor = nullptr;
  fxparam_constptr_t _parRoughnessPower  = nullptr;
  fxparam_constptr_t _parModColor        = nullptr;
  fxparam_constptr_t _parPickID          = nullptr;
  fxparamblock_constptr_t _parBoneBlock  = nullptr;

  // fwd

  fxparam_constptr_t _paramEyePostion      = nullptr;
  fxparam_constptr_t _paramEyePostionL     = nullptr;
  fxparam_constptr_t _paramEyePostionR     = nullptr;
  fxparam_constptr_t _paramAmbientLevel    = nullptr;
  fxparam_constptr_t _paramDiffuseLevel    = nullptr;
  fxparam_constptr_t _paramSpecularLevel   = nullptr;
  fxparam_constptr_t _paramSkyboxLevel     = nullptr;

  fxparam_constptr_t _paramSSAOTexture    = nullptr;
  fxparam_constptr_t _paramSSAOWeight    = nullptr;
  fxparam_constptr_t _paramSSAOPower    = nullptr;
  fxparam_constptr_t _paramSSAOBias    = nullptr;
  fxparam_constptr_t _paramSSAORadius    = nullptr;
  fxparam_constptr_t _paramSSAONumSteps    = nullptr;
  fxparam_constptr_t _paramSSAONumSamples    = nullptr;
  fxparam_constptr_t _paramSSAOKernel    = nullptr;
  fxparam_constptr_t _paramSSAOScrNoise    = nullptr;

  fxparam_constptr_t _paramNearFar      = nullptr;

  fxparam_constptr_t _parBaseColor      = nullptr;

  fxparam_constptr_t _parMapLightMap         = nullptr;
  fxparam_constptr_t _parMapSpecularEnv      = nullptr;
  fxparam_constptr_t _parMapDiffuseEnv       = nullptr;
  fxparam_constptr_t _parMapBrdfIntegration  = nullptr;
  fxparam_constptr_t _parEnvironmentMipBias  = nullptr;
  fxparam_constptr_t _parEnvironmentMipScale = nullptr;
  fxparam_constptr_t _parSpecularMipBias  = nullptr;
  fxparam_constptr_t _parDepthFogDistance = nullptr;
  fxparam_constptr_t _parDepthFogPower = nullptr;




  //fxparam_constptr_t _parLightCookies   = nullptr;
  fxparam_constptr_t _parLightCookie0   = nullptr;
  fxparam_constptr_t _parLightCookie1   = nullptr;
  fxparam_constptr_t _parLightCookie2   = nullptr;
  fxparam_constptr_t _parLightCookie3   = nullptr;
  fxparam_constptr_t _parLightCookie4   = nullptr;
  fxparam_constptr_t _parLightCookie5   = nullptr;
  fxparam_constptr_t _parLightCookie6   = nullptr;
  fxparam_constptr_t _parLightCookie7   = nullptr;

  fxparam_constptr_t _parProbeReflection   = nullptr;
  fxparam_constptr_t _parProbeIrradiance   = nullptr;

  fxparam_constptr_t _parUnTexPointLightsCount  = nullptr;
  fxparam_constptr_t _parTexSpotLightsCount   = nullptr;

  fxparamblock_constptr_t _parUnTexPointLightsData   = nullptr;

  ///////////////////////////////////////////
  // instancing (via texture)
  fxparam_constptr_t _paramInstanceMatrixMap = nullptr; // 1k*1k texture containing instance matrices
  fxparam_constptr_t _paramInstanceIdMap     = nullptr; // 1k*1k texture containing instance pickids
  fxparam_constptr_t _paramInstanceColorMap  = nullptr; // 1k*1k texture containing instance colors
  const FxShaderParamBlock* _paramInstanceBlock = nullptr;
  ///////////////////////////////////////////
  image_ptr_t _image_color;
  image_ptr_t _image_normal;
  image_ptr_t _image_mtlruf;
  image_ptr_t _image_emissive;
  image_ptr_t _image_ambocc;
  ///////////////////////////////////////////
  texture_ptr_t _texColor;
  texture_ptr_t _texNormal;
  texture_ptr_t _texMtlRuf;
  texture_ptr_t _texEmissive;
  texture_ptr_t _texAmbOcc;
  texture_ptr_t _texLightMap;
  texture_ptr_t _texBlack;
  texture_ptr_t _texCubeBlack;
  texture_ptr_t _texArrayCNMREA;
  std::string _textureBaseName;
  std::string _shader_suffix;
  ///////////////////////////////////////////

  // PIK: Picking
  // FWD: Forward
  // GBU: Deferred (gbuffer pass)
  // RI: Rigid
  // SK: Skinned
  // CM: ModColor
  // CT: Textured
  // CV: Vertex Color
  // CF: Font
  // NM: NormalMapped
  // NV: VertexNormals
  // DB: DebugVisualizer
  // NI: Non-Instanced
  // IN: Instanced
  // MO: Mono
  // ST: Stereo

  //////////////////
  // pick/special techniques
  //////////////////

  fxtechnique_constptr_t _tek_GBU_DB_NM_NI_MO = nullptr;

  fxtechnique_constptr_t _tek_GBU_CF_IN_MO = nullptr;
  fxtechnique_constptr_t _tek_GBU_CF_NI_MO = nullptr;

  fxtechnique_constptr_t _tek_PIK_RI_IN = nullptr;
  fxtechnique_constptr_t _tek_PIK_RI_NI = nullptr;
  fxtechnique_constptr_t _tek_PIK_SK_NI = nullptr;

  //////////////////////
  // forward techniques
  //////////////////////

  fxtechnique_constptr_t _tek_FWD_UNLIT_NI_MO = nullptr;

  fxtechnique_constptr_t _tek_FWD_SKYBOX_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_SKYBOX_ST = nullptr;
  fxtechnique_constptr_t _tek_FWD_DEPTHPREPASS_IN_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_DEPTHPREPASS_IN_ST = nullptr;

  fxtechnique_constptr_t _tek_FWD_DEPTHPREPASS_RI_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_DEPTHPREPASS_SK_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_DEPTHPREPASS_RI_IN_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_DEPTHPREPASS_SK_IN_MO = nullptr;

  fxtechnique_constptr_t _tek_FWD_DEPTHPREPASS_RI_NI_ST = nullptr;
  fxtechnique_constptr_t _tek_FWD_DEPTHPREPASS_SK_NI_ST = nullptr;
  fxtechnique_constptr_t _tek_FWD_DEPTHPREPASS_RI_IN_ST = nullptr;
  fxtechnique_constptr_t _tek_FWD_DEPTHPREPASS_SK_IN_ST = nullptr;

  // modcolor

  fxtechnique_constptr_t _tek_FWD_CM_NM_RI_IN_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CM_NM_RI_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CM_NM_RI_IN_ST = nullptr;
  fxtechnique_constptr_t _tek_FWD_CM_NM_RI_NI_ST = nullptr;
  
  fxtechnique_constptr_t _tek_FWD_CM_NM_SK_IN_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CM_NM_SK_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CM_NM_SK_IN_ST = nullptr;
  fxtechnique_constptr_t _tek_FWD_CM_NM_SK_NI_ST = nullptr;

  // texcolor

  fxtechnique_constptr_t _tek_FWD_CT_NM_RI_IN_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CT_NM_RI_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CV_NM_RI_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CT_NM_RI_IN_ST = nullptr;
  fxtechnique_constptr_t _tek_FWD_CT_NM_RI_NI_ST = nullptr;
  
  fxtechnique_constptr_t _tek_FWD_CT_NM_SK_IN_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CT_NM_SK_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CT_NM_SK_IN_ST = nullptr;
  fxtechnique_constptr_t _tek_FWD_CT_NM_SK_NI_ST = nullptr;

  // vtxcolor

  fxtechnique_constptr_t _tek_FWD_CV_EMI_RI_NI_MO = nullptr;

  //////////////////////
  // deferred (gbuffer) techniques
  //////////////////////

  // modcolor

  fxtechnique_constptr_t _tek_GBU_CM_NM_RI_IN_MO = nullptr;
  fxtechnique_constptr_t _tek_GBU_CM_NM_RI_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_GBU_CM_NM_RI_IN_ST = nullptr;
  fxtechnique_constptr_t _tek_GBU_CM_NM_RI_NI_ST = nullptr;

  fxtechnique_constptr_t _tek_GBU_CM_NM_SK_IN_MO = nullptr;
  fxtechnique_constptr_t _tek_GBU_CM_NM_SK_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_GBU_CM_NM_SK_IN_ST = nullptr;
  fxtechnique_constptr_t _tek_GBU_CM_NM_SK_NI_ST = nullptr;

  // texcolor

  fxtechnique_constptr_t _tek_GBU_CT_NV_RI_NI_MO = nullptr;

  fxtechnique_constptr_t _tek_GBU_CT_NM_RI_IN_MO = nullptr;
  fxtechnique_constptr_t _tek_GBU_CT_NM_RI_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_GBU_CT_NM_RI_IN_ST = nullptr;
  fxtechnique_constptr_t _tek_GBU_CT_NM_RI_NI_ST = nullptr;

  fxtechnique_constptr_t _tek_GBU_CT_NM_SK_IN_MO = nullptr;
  fxtechnique_constptr_t _tek_GBU_CT_NM_SK_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_GBU_CT_NM_SK_IN_ST = nullptr;
  fxtechnique_constptr_t _tek_GBU_CT_NM_SK_NI_ST = nullptr;

  // vtxcolor

  fxtechnique_constptr_t _tek_GBU_CV_EMI_RI_NI_MO = nullptr;

  ////////////////////////////////////

  std::string _colorMapName;
  std::string _normalMapName;
  std::string _mtlRufMapName;
  std::string _amboccMapName;
  std::string _emissiveMapName;
  file::Path _shaderpath;

  float _metallicFactor  = 0.0f;
  float _roughnessFactor = 1.0f;
  fvec4 _baseColor;

  bool _stereoVtex = false;
  bool _doubleSided = false;
  
  varmap::varmap_ptr_t _vars;
};

FxPipeline::statelambda_t createBasicStateLambda(const PBRMaterial* mtl);
FxPipeline::statelambda_t createLightingLambda(const PBRMaterial* mtl);
FxPipeline::statelambda_t createForwardLightingLambda(const PBRMaterial* mtl);

pbrmaterial_ptr_t default3DMaterial(Context* ctx);

} // namespace ork::lev2
