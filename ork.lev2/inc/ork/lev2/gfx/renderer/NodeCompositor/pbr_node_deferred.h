////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/kernel/varmap.inl>
#include "pbr_common.h"

namespace ork::lev2::pbr::deferrednode {

class DeferredCompositingNode;

///////////////////////////////////////////////////////////////////////////////

struct AuxParamBinding{
  const FxShaderParam* _param = nullptr;
  svar64_t _var;
};

using auxparambinding_ptr_t = std::shared_ptr<AuxParamBinding>;

///////////////////////////////////////////////////////////////////////////////

struct DeferredContext {

#if defined(ENABLE_COMPUTE_SHADERS)
  static constexpr int KTILEDIMXY = 64;
#else
  static constexpr int KTILEDIMXY = 64;
#endif
  //static constexpr float KNEAR = 0.1f;
  //static constexpr float KFAR  = 100000.0f;
  ////////////////////////////////////////////////////////////////////
  DeferredContext(RenderCompositingNode* node, std::string shadername, int numlights);
  ~DeferredContext();
  ////////////////////////////////////////////////////////////////////
  void updateDebugLights(const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  void gpuInit(Context* target);
  const uint32_t* captureDepthClusters(const CompositorDrawData& drawdata, const ViewData& VD);
  void renderUpdate(RenderCompositingNode* node, CompositorDrawData& drawdata);
  void renderGbuffer(RenderCompositingNode* node, CompositorDrawData& drawdata, const ViewData& VD);
  void renderBaseLighting(RenderCompositingNode* node, CompositorDrawData& drawdata, const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  void beginPointLighting(RenderCompositingNode* node, CompositorDrawData& drawdata, const ViewData& VD, Texture* cookietexture);
  void endPointLighting(CompositorDrawData& drawdata, const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  void beginSpotLighting(RenderCompositingNode* node, CompositorDrawData& drawdata, const ViewData& VD, Texture* cookietexture);
  void endSpotLighting(CompositorDrawData& drawdata, const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  void beginShadowedSpotLighting(RenderCompositingNode* node, CompositorDrawData& drawdata, const ViewData& VD, Texture* cookietexture);
  void endShadowedSpotLighting(CompositorDrawData& drawdata, const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  void beginSpotDecaling(RenderCompositingNode* node, CompositorDrawData& drawdata, const ViewData& VD, Texture* cookietexture);
  void endSpotDecaling(CompositorDrawData& drawdata, const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  void bindViewParams(const ViewData& VD);
  void bindRasterState(Context* ctx, ECullTest culltest, EDepthTest depthtest, Blending blending);
  ////////////////////////////////////////////////////////////////////
  RenderCompositingNode* _node;
  freestyle_mtl_ptr_t _lightingmtl;

  fxpipeline_ptr_t _pipeline_envlighting_model0_mono;
  CompositingPassData _accumCPD;
  CompositingPassData _decalCPD;
  fvec4 _clearColor;
  std::string _shadername;
  lev2::texture_ptr_t brdfIntegrationTexture() const;
  ////////////////////////////////////////////////////////////////////
  int _width    = 0;
  int _height   = 0;
  int _clusterW = 0;
  int _clusterH = 0;
  textureassetptr_t _whiteTexture;
  textureassetptr_t _voltexA;
  bool _enableSDF = false;
  void_lambda_t _onGpuInitialized;
  ////////////////////////////////////////////////////////////////////
  auxparambinding_ptr_t createAuxParamBinding(std::string paramname);
  ////////////////////////////////////////////////////////////////////
  std::unordered_map<std::string, auxparambinding_ptr_t> _auxbindings;
  ////////////////////////////////////////////////////////////////////
  std::vector<PointLight*> _pointlights;
  ////////////////////////////////////////////////////////////////////

  const FxShaderTechnique* _tekBaseLighting              = nullptr;

  const FxShaderTechnique* _tekEnvironmentLighting       = nullptr;
  const FxShaderTechnique* _tekEnvironmentLightingStereo = nullptr;

  const FxShaderTechnique* _tekEnvironmentLightingSDF       = nullptr;
  const FxShaderTechnique* _tekEnvironmentLightingSDFStereo = nullptr;

  const FxShaderTechnique* _tekBaseLightingStereo        = nullptr;
  const FxShaderTechnique* _tekDownsampleDepthCluster    = nullptr;
  //
  const FxShaderTechnique* _tekPointLightingUntextured       = nullptr;
  const FxShaderTechnique* _tekPointLightingTextured         = nullptr;
  const FxShaderTechnique* _tekPointLightingUntexturedStereo = nullptr;
  const FxShaderTechnique* _tekPointLightingTexturedStereo   = nullptr;
  //
  const FxShaderTechnique* _tekSpotLightingUntextured             = nullptr;
  const FxShaderTechnique* _tekSpotLightingTextured               = nullptr;
  const FxShaderTechnique* _tekSpotLightingUntexturedStereo       = nullptr;
  const FxShaderTechnique* _tekSpotLightingTexturedStereo         = nullptr;
  const FxShaderTechnique* _tekSpotLightingTexturedShadowed       = nullptr;
  const FxShaderTechnique* _tekSpotLightingTexturedShadowedStereo = nullptr;
  //
  const FxShaderTechnique* _tekSpotDecalingTextured       = nullptr;
  const FxShaderTechnique* _tekSpotDecalingTexturedStereo = nullptr;
  //

#if defined(ENABLE_COMPUTE_SHADERS)
  FxComputeShader* _lightcollectcomputeshader = nullptr;
#endif

  const FxShaderParam* _parMatIVPArray = nullptr;
  const FxShaderParam* _parMatPArray   = nullptr;
  const FxShaderParam* _parMatVArray   = nullptr;
  const FxShaderParam* _parZndc2eye    = nullptr;
  const FxShaderParam* _parMapGBuf     = nullptr;
  const FxShaderParam* _parMapDepth            = nullptr;
  const FxShaderParam* _parMapDepthCluster     = nullptr;
  const FxShaderParam* _parMapShadowDepth      = nullptr;
  const FxShaderParam* _parMapSpecularEnv      = nullptr;
  const FxShaderParam* _parMapDiffuseEnv       = nullptr;
  const FxShaderParam* _parMapBrdfIntegration  = nullptr;
  const FxShaderParam* _parMapVolTexA  = nullptr;
  const FxShaderParam* _parTime                = nullptr;
  const FxShaderParam* _parNearFar             = nullptr;
  const FxShaderParam* _parInvViewSize         = nullptr;
  const FxShaderParam* _parInvVpDim            = nullptr;
  const FxShaderParam* _parNumLights           = nullptr;
  const FxShaderParam* _parTileDim             = nullptr;
  const FxShaderParam* _parSpecularLevel       = nullptr;
  const FxShaderParam* _parSpecularMipBias     = nullptr;
  const FxShaderParam* _parDiffuseLevel        = nullptr;
  const FxShaderParam* _parAmbientLevel        = nullptr;
  const FxShaderParam* _parSkyboxLevel         = nullptr;
  const FxShaderParam* _parEnvironmentMipBias  = nullptr;
  const FxShaderParam* _parEnvironmentMipScale = nullptr;
  const FxShaderParam* _parDepthFogDistance    = nullptr;
  const FxShaderParam* _parDepthFogPower       = nullptr;
  const FxShaderParamBlock* _lightblock        = nullptr;
  const FxShaderParam* _parLightCookieTexture  = nullptr;
  const FxShaderParam* _parShadowParams        = nullptr;

  ////////////////////////////////////////////////////////////////////


  ////////////////////////////////////////////////////////////////////

  lev2::texture_ptr_t _brdfIntegrationMap = nullptr;

  CaptureBuffer _clustercapture;

  rtgroup_ptr_t _rtgGbuffer;
  rtgroup_ptr_t _rtgLbuffer;

  rtgset_ptr_t _rtgs_gbuffer;
  rtgset_ptr_t _rtgs_laccum;

  rtgroup_ptr_t _rtgDecal        = nullptr;
  rtgroup_ptr_t _rtgDepthCluster = nullptr;

  rtbuffer_ptr_t _rtbDepthCluster = nullptr;

  std::string _layername;
  EBufferFormat _lightAccumFormat;
  EBufferFormat _auxBufferFormat = EBufferFormat::NONE;
  float _specularLevel    = 1.0f;
  float _diffuseLevel     = 1.0f;
  float _depthFogPower    = 1.0f;
  float _depthFogDistance = 1.0f;

  varmap::varmap_ptr_t _vars;
};

///////////////////////////////////////////////////////////////////////////////

class DeferredCompositingNode : public RenderCompositingNode {
  DeclareConcreteX(DeferredCompositingNode, RenderCompositingNode);

public:
  DeferredCompositingNode();
  ~DeferredCompositingNode();
  fvec4 _clearColor;
  fvec4 _fogColor;

private:
  void doGpuInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::rtbuffer_ptr_t GetOutput() const final;
  svar256_t _impl;
};

///////////////////////////////////////////////////////////////////////////////

struct DeferredCompositingNodePbr : public RenderCompositingNode {
  DeclareConcreteX(DeferredCompositingNodePbr, RenderCompositingNode);

public:
  DeferredCompositingNodePbr();
  ~DeferredCompositingNodePbr();

  void doGpuInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::rtbuffer_ptr_t GetOutput() const final;
  lev2::rtgroup_ptr_t GetOutputGroup() const final;
  void overrideShader( std::string path );
  pbr_deferred_context_ptr_t deferredContext();

  svar256_t _impl;

  pbr::commonstuff_ptr_t _pbrcommon;
  std::string _shader_path;

};

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_NVMESH_SHADERS)

class DeferredCompositingNodeNvMs : public RenderCompositingNode {
  DeclareConcreteX(DeferredCompositingNodeNvMs, RenderCompositingNode);

public:
  DeferredCompositingNodeNvMs();
  ~DeferredCompositingNodeNvMs();
  fvec4 _clearColor;
  fvec4 _fogColor;

private:
  void doGpuInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::rtbuffer_ptr_t GetOutput() const final;
  svar256_t _impl;
};

#endif

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::deferrednode
