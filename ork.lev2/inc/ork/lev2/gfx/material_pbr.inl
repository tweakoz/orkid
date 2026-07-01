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

struct PbrMatrixBlockApplicator : public MaterialInstApplicator {
  MaterialInstItemMatrixBlock* _matrixblock = nullptr;
  const PBRMaterial* _pbrmaterial           = nullptr;
  void ApplyToTarget(Context* pTARG) final;
  static PbrMatrixBlockApplicator* getApplicator();
};

///////////////////////////////////////////////////////////////////////////////

class PBRMaterial final : public GfxMaterial {

  DeclareConcreteX(PBRMaterial, GfxMaterial);

  static fxpipeline_ptr_t _createFxPipeline(const FxPipelinePermutation& permu, const PBRMaterial* mtl);
  fxpipeline_ptr_t _createFxPipelineFWD(const FxPipelinePermutation& permu) const;
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

  static constexpr float roughnessLevels = 16.0f;

  ////////////////////////////////////////////

  static material_ptr_t _xgmReader( chunkfile::XgmMaterialReaderContext& ctx );
  static void _xgmWriter( chunkfile::XgmMaterialWriterContext& ctx );

  ////////////////////////////////////////////

  pbrmaterial_ptr_t clone() const;
  void addBasicStateLambda(fxpipeline_ptr_t pipe);
  void addLightingLambda(fxpipeline_ptr_t pipe);
  void addBasicStateLambda();
  void addLightingLambda();

  ////////////////////////////////////////////

  static FxShaderStorageBuffer* lightingDataBuffer(Context* targ);
  static FxUniformBuffer* boneDataBuffer(Context* targ);

  static texture_ptr_t brdfIntegrationMap(Context* targ,std::string type);

  ////////////////////////////////////////////

  void begin(const RenderContextFrameData& RCFD);
  void end(const RenderContextFrameData& RCFD);

  ////////////////////////////////////////////

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
  bool instancedMatricesOnly() const override { return _instanceMatricesOnly; }
  ////////////////////////////////////////////
  //void setupCamera(const RenderContextFrameData& RCFD);
  ////////////////////////////////////////////
  fxshaderasset_constptr_t _asset_shader;
  freestyle_mtl_ptr_t _as_freestyle;
  textureassetptr_t _asset_texcolor;
  textureassetptr_t _asset_texambocc;
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
  fxparam_constptr_t _paramIV            = nullptr;  // inv_v
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
  fxparam_constptr_t _paramMVIT          = nullptr;  // model-view inverse-transpose (mat4)
  fxparam_constptr_t _paramMVITROT       = nullptr;  // its 3x3 normal matrix (object->view)
  fxparam_constptr_t _paramDppZBias      = nullptr;
  fxparam_constptr_t _paramMapDepth      = nullptr;
  fxparam_constptr_t _paramMapLinearDepth      = nullptr;

  fxparam_constptr_t _paramMapCNMREA      = nullptr;

  fxparam_constptr_t _parInvViewSize     = nullptr;
  fxparam_constptr_t _parMetallicFactor  = nullptr;
  fxparam_constptr_t _parRoughnessFactor = nullptr;
  fxparam_constptr_t _parRoughnessPower  = nullptr;
  fxparam_constptr_t _parAlphaCutoff     = nullptr;
  fxparam_constptr_t _parModColor        = nullptr;
  // Per-material multiplicative tint applied to the textured albedo only.
  // Distinct from _parModColor (which is a post-light-output tint context
  // state). Conflating the two — what we used to do — caused albedo to leak
  // into the env-IBL specular path (see fwdnode_pipeline.cpp comment).
  fxparam_constptr_t _parModAlbedo       = nullptr;
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

  fxparam_constptr_t _parMapSpecularEnv      = nullptr;
  fxparam_constptr_t _parMapSpecularRufLevels= nullptr;
  fxparam_constptr_t _parMapDiffuseEnv       = nullptr;
  fxparam_constptr_t _parMapBrdfIntegration  = nullptr;
  fxparam_constptr_t _parEnvironmentMipBias  = nullptr;
  fxparam_constptr_t _parEnvironmentMipScale = nullptr;
  fxparam_constptr_t _parSpecularMipBias  = nullptr;
  fxparam_constptr_t _parDepthFogDistance = nullptr;
  fxparam_constptr_t _parDepthFogPower = nullptr;

  fxparam_constptr_t _parLightColorCookies   = nullptr;
  fxparam_constptr_t _parLightDepthCookies   = nullptr;

  fxparam_constptr_t _parProbeReflection   = nullptr;
  fxparam_constptr_t _parProbeRadiance   = nullptr;
  fxparam_constptr_t _parHasReflectionProbe = nullptr;

  // PBR2 Phase 1 — 8 glTF KHR-extension lobe feature flags + factors.
  // Default off (has_*=0, factor=0); shader libblocks short-circuit when
  // flag is 0 so disabled lobes incur no shader cost. Each fxparam_constptr_t
  // is resolved from the shader at gpuInit() — P1.5 will populate them and
  // bind per-draw. Until P1.3 lands the matching shader uniforms these stay
  // nullptr (fxi->parameter lookup misses are non-fatal).
  fxparam_constptr_t _parHasTransmission           = nullptr;
  fxparam_constptr_t _parTransmissionFactor        = nullptr;
  fxparam_constptr_t _parHasTransmissionRoughness  = nullptr;
  fxparam_constptr_t _parTransmissionRoughness     = nullptr;
  fxparam_constptr_t _parHasIor                    = nullptr;
  fxparam_constptr_t _parIor                       = nullptr;
  fxparam_constptr_t _parHasVolume                 = nullptr;
  fxparam_constptr_t _parVolumeThicknessFactor     = nullptr;
  fxparam_constptr_t _parHasDiffuseTransmission    = nullptr;
  fxparam_constptr_t _parDiffuseTransmissionFactor = nullptr;
  fxparam_constptr_t _parHasSpecular               = nullptr;
  fxparam_constptr_t _parSpecularFactor            = nullptr;
  fxparam_constptr_t _parHasClearcoat              = nullptr;
  fxparam_constptr_t _parClearcoatFactor           = nullptr;
  fxparam_constptr_t _parHasSheen                  = nullptr;
  fxparam_constptr_t _parSheenFactor               = nullptr;
  fxparam_constptr_t _parHasIridescence            = nullptr;
  fxparam_constptr_t _parIridescenceFactor         = nullptr;

  // PBR2 Phase 2 — vec3 color + secondary scalar uniforms for the lobes.
  // sheen_color defaults (0,0,0) — per glTF spec sheen is OFF when color
  // is zero. specular/attenuation/diffuse_transmission default (1,1,1)
  // — modulate against a (future) white default texture.
  fxparam_constptr_t _parSheenColor                = nullptr;
  fxparam_constptr_t _parSpecularColor             = nullptr;
  fxparam_constptr_t _parAttenuationColor          = nullptr;
  fxparam_constptr_t _parDiffuseTransmissionColor  = nullptr;
  fxparam_constptr_t _parClearcoatRoughness        = nullptr;
  fxparam_constptr_t _parSheenRoughness            = nullptr;
  fxparam_constptr_t _parAttenuationDistance       = nullptr;

  // PBR2 Phase 3 (P3.D) — subsurface UBO uniforms (ublk_pbr_subsurface).
  fxparam_constptr_t _parHasSubsurface             = nullptr;
  fxparam_constptr_t _parSubsurfaceColor           = nullptr;
  fxparam_constptr_t _parSubsurfaceRadius          = nullptr;
  fxparam_constptr_t _parSubsurfaceFactor          = nullptr;
  // PBR2 Phase 2 — probe-capture short-circuit for refractive lobes.
  // Bound per-draw from fwdnode_pipeline.cpp via the RCFD
  // "renderingPROBE" user-property. Zero on the primary forward pass,
  // one when capturing into a reflection probe cubemap.
  fxparam_constptr_t _parRenderingProbe            = nullptr;

  fxparam_constptr_t _parUnTexPointLightsCount  = nullptr;
  fxparam_constptr_t _parTexSpotLightsCount   = nullptr;

  fxparamstorageblock_constptr_t _parForwardLightBlock   = nullptr;

  ///////////////////////////////////////////
  // instancing (via SSBO)
  fxparamstorageblock_constptr_t _parInstanceBlock = nullptr;
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

  ///////////////////////////////////////////
  // Lightmaps
  ///////////////////////////////////////////

  void setActiveLightMap(std::string name, fvec3 c );

  fxparam_constptr_t _parMapLightMapArray      = nullptr;

  fxparam_constptr_t _paramLightMapColors = nullptr; 
  
  texturearray_ptr_t _texLightMapArray;

  constexpr static size_t kMaxLightmaps = 8;
  std::unordered_map<std::string, image_ptr_t> _lightmap_image_assets;
  std::unordered_map<std::string, int> _lightmap_indices;
  std::vector<image_ptr_t> _image_lightmaps;

  void conformLightmaps();
  void assignLightmaps(Context* ctx);

  fvec3 _lightmapColors[kMaxLightmaps];

  ///////////////////////////////////////////

  pbr::commonstuff_ptr_t _commonOverride;

  texturearray_ptr_t _texArrayCNMREA;
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
  fxtechnique_constptr_t _tek_FWD_CV_NM_RI_IN_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CV_NM_RI_IN_MO_ALPHA = nullptr;
  fxtechnique_constptr_t _tek_FWD_CT_NM_RI_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CV_NM_RI_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CV_NM_RI_NI_MO_ALPHA = nullptr;
  fxtechnique_constptr_t _tek_FWD_CT_NM_RI_IN_ST = nullptr;
  fxtechnique_constptr_t _tek_FWD_CT_NM_RI_NI_ST = nullptr;
  
  fxtechnique_constptr_t _tek_FWD_CT_NM_SK_IN_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CT_NM_SK_NI_MO = nullptr;
  fxtechnique_constptr_t _tek_FWD_CT_NM_SK_IN_ST = nullptr;
  fxtechnique_constptr_t _tek_FWD_CT_NM_SK_NI_ST = nullptr;

  // SSBO-sourced vertex variant (compute-generated geometry; ptex3d FWD_SSBO_CUSTOM). Null unless
  // the (generated) shader declares it. Selected via permu._is_vertex_ssbo. See project_fwd_ssbo_custom.
  fxtechnique_constptr_t _tek_FWD_SSBO_CUSTOM = nullptr;
  fxtechnique_constptr_t _tek_FWD_SSBO_CUSTOM_INSTANCED = nullptr;   // SSBO geometry x per-instance matrix (gl_InstanceIndex)
  fxtechnique_constptr_t _tek_FWD_SSBO_CUSTOM_CAPTURE = nullptr;     // impostor bake: SSBO pull -> raw PBR to MRT (no lighting)
  // LOD impostor billboard: SSBO instance-matrix pull -> camera-facing quad -> surface() samples the baked
  // atlas -> the SAME forward PBR lighting (_forward_lightingZ). Selected via permu._is_impostor. The atlas
  // textures + grid/radius are set on the material after the bake (bindImpostorAtlas) and bound by the
  // forward pipeline's impostor branch. Null unless the (generated, impostor=True) shader declares it.
  fxtechnique_constptr_t _tek_FWD_SSBO_CUSTOM_IMPOSTOR = nullptr;
  fxparam_constptr_t _parImpAlbedo     = nullptr;
  fxparam_constptr_t _parImpNormal     = nullptr;
  fxparam_constptr_t _parImpMetalRough = nullptr;
  fxparam_constptr_t _parImpCenter     = nullptr;
  fxparam_constptr_t _parImpGrid       = nullptr;
  texture_ptr_t _impostorAtlasAlbedo;
  texture_ptr_t _impostorAtlasNormal;
  texture_ptr_t _impostorAtlasMetalRough;
  fvec4 _impostorCenter = fvec4(0, 0, 0, 1); // xyz = object-space bbox center, w = bound radius
  fvec4 _impostorGrid   = fvec4(8, 0, 0, 0); // x = hemi-oct grid N, y = max draw distance (fade-out / cull)
  void bindImpostorAtlas(texture_ptr_t alb, texture_ptr_t nrm, texture_ptr_t mr,
                         const fvec3& center, float radius, float gridN, float maxDist) {
    _impostorAtlasAlbedo = alb; _impostorAtlasNormal = nrm; _impostorAtlasMetalRough = mr;
    _impostorCenter = fvec4(center.x, center.y, center.z, radius);
    _impostorGrid   = fvec4(gridN, maxDist, 0, 0);
  }
  fxtechnique_constptr_t _tek_FWD_SSBO_CUSTOM_DEPTHPREPASS = nullptr;
  fxtechnique_constptr_t _tek_FWD_SSBO_CUSTOM_INSTANCED_DEPTHPREPASS = nullptr; // E.4

  // matrices-only instancing: when set, the instanced pipeline uses FWD_CT_NM_IM_NI_MO and
  // _parInstanceBlock resolves to the dynamic storage_inst_mtx block. Set before gpuInit.
  bool _instanceMatricesOnly = false;
  fxtechnique_constptr_t _tek_FWD_CT_NM_IM_NI_MO = nullptr;

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
  fvec4 _baseColor = fvec4(1, 1, 1, 1);

  // PBR2 Phase 1 — 8-lobe feature flags + factors (per-material).
  // All default OFF — Phase 1 lands the scaffolding; shader libblocks
  // and per-draw bindings come in P1.4 / P1.5. Factor defaults match
  // glTF KHR extension spec defaults (intent-preserving when enabled).
  bool  _hasTransmission           = false;
  float _transmissionFactor        = 0.0f;     // KHR_materials_transmission
  // P3.D — optional separate transmission roughness (beyond glTF spec).
  // When _hasTransmissionRoughness=false, falls back to _roughnessFactor.
  bool  _hasTransmissionRoughness  = false;
  float _transmissionRoughness     = 0.0f;
  bool  _hasIor                    = false;
  float _ior                       = 1.5f;     // KHR_materials_ior (dielectric default)
  bool  _hasVolume                 = false;
  float _volumeThicknessFactor     = 0.0f;     // KHR_materials_volume
  bool  _hasDiffuseTransmission    = false;
  float _diffuseTransmissionFactor = 0.0f;     // KHR_materials_diffuse_transmission
  bool  _hasSpecular               = false;
  float _specularFactor            = 1.0f;     // KHR_materials_specular (1.0 = full when enabled)
  bool  _hasClearcoat              = false;
  float _clearcoatFactor           = 0.0f;     // KHR_materials_clearcoat
  bool  _hasSheen                  = false;
  float _sheenFactor               = 0.0f;     // KHR_materials_sheen
  bool  _hasIridescence            = false;
  float _iridescenceFactor         = 0.0f;     // KHR_materials_iridescence

  // PBR2 Phase 2 — lobe color + secondary scalar parameters.
  // glTF spec defaults: sheen_color = (0,0,0) [sheen OFF when zero];
  // specular/attenuation/diffuse_transmission color = (1,1,1).
  fvec3 _sheenColor               = fvec3(0, 0, 0);
  fvec3 _specularColor            = fvec3(1, 1, 1);
  fvec3 _attenuationColor         = fvec3(1, 1, 1);
  fvec3 _diffuseTransmissionColor = fvec3(1, 1, 1);
  float _clearcoatRoughness       = 0.0f;
  float _sheenRoughness           = 0.0f;
  float _attenuationDistance      = 1.0f;   // +Inf in spec; finite here

  // PBR2 Phase 3 (P3.D) — KHR_materials_subsurface (in-flight ext).
  // Screen-space separable subsurface scattering. radius is per-channel
  // (mm); skin baseline = (1.4, 0.5, 0.3).
  bool  _hasSubsurface     = false;
  fvec3 _subsurfaceColor   = fvec3(1, 1, 1);
  fvec3 _subsurfaceRadius  = fvec3(1, 1, 1);
  float _subsurfaceFactor  = 0.0f;

  bool _stereoVtex = false;
  bool _doubleSided = false;
  bool _alphaBlend = false;
  float _alphaCutoff = 0.0f;
  int _alphaMode = 0; // 0=OPAQUE, 1=MASK, 2=BLEND

  varmap::varmap_ptr_t _vars;
  xgmmodelassetmaterialmodifiers_ptr_t _modifiers;

};

FxPipeline::statelambda_t createBasicStateLambda(const PBRMaterial* mtl);
FxPipeline::statelambda_t createLightingLambda(const PBRMaterial* mtl);

pbrmaterial_ptr_t default3DMaterial(Context* ctx);

} // namespace ork::lev2
