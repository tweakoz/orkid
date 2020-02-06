////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"
#include <ork/lev2/gfx/material_freestyle.inl>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>

namespace ork::lev2::deferrednode {

class DeferredCompositingNode;

template <typename T> inline bool doRangesOverlap(T amin, T amax, T bmin, T bmax) {
  return std::max(amin, bmin) <= std::min(amax, bmax);
}
///////////////////////////////////////////////////////////////////////////////

struct PointLight {

  PointLight() {
  }
  fvec3 _pos;
  fvec3 _dst;
  fvec3 _color;
  float _radius;
  int _counter   = 0;
  float dist2cam = 0;
  AABox _aabox;
  fvec3 _aamin, _aamax;
  int _minX, _minY;
  int _maxX, _maxY;
  float _minZ, _maxZ;

  void next() {
    float x  = float((rand() & 0x3fff) - 0x2000);
    float z  = float((rand() & 0x3fff) - 0x2000);
    float y  = float((rand() & 0x1fff) - 0x1000);
    _dst     = fvec3(x, y, z);
    _counter = 256 + rand() & 0xff;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ViewData {
  bool _isStereo = false;
  fmtx4 _ivp[2];
  fmtx4 _v[2];
  fmtx4 _p[2];
  fvec3 _camposmono;
  fmtx4 IVPL, IVPR, IVPM;
  fmtx4 VL, VR, VM;
  fmtx4 PL, PR, PM;
  fmtx4 VPL, VPR, VPM;
  fvec2 _zndc2eye;
};

///////////////////////////////////////////////////////////////////////////////

struct DeferredContext {

#if defined(ENABLE_COMPUTE_SHADERS)
  static constexpr int KTILEDIMXY = 64;
#else
  static constexpr int KTILEDIMXY = 64;
#endif
  static constexpr float KNEAR = 0.1f;
  static constexpr float KFAR  = 100000.0f;
  ////////////////////////////////////////////////////////////////////
  DeferredContext(RenderCompositingNode* node, std::string shadername, int numlights);
  ~DeferredContext();
  ////////////////////////////////////////////////////////////////////
  ViewData computeViewData(CompositorDrawData& drawdata);
  ////////////////////////////////////////////////////////////////////
  void updateDebugLights(const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  void gpuInit(Context* target);
  const uint32_t* captureDepthClusters(const CompositorDrawData& drawdata, const ViewData& VD);
  void renderUpdate(CompositorDrawData& drawdata);
  void renderGbuffer(CompositorDrawData& drawdata, const ViewData& VD);
  void renderBaseLighting(CompositorDrawData& drawdata, const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  void beginPointLighting(CompositorDrawData& drawdata, const ViewData& VD, Texture* cookietexture);
  void endPointLighting(CompositorDrawData& drawdata, const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  void beginSpotLighting(CompositorDrawData& drawdata, const ViewData& VD, Texture* cookietexture);
  void endSpotLighting(CompositorDrawData& drawdata, const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  void beginShadowedSpotLighting(CompositorDrawData& drawdata, const ViewData& VD, Texture* cookietexture);
  void endShadowedSpotLighting(CompositorDrawData& drawdata, const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  void beginSpotDecaling(CompositorDrawData& drawdata, const ViewData& VD, Texture* cookietexture);
  void endSpotDecaling(CompositorDrawData& drawdata, const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  void bindViewParams(const ViewData& VD);
  void bindRasterState(Context* ctx, ECullTest culltest, EDepthTest depthtest, EBlending blending);
  ////////////////////////////////////////////////////////////////////
  RenderCompositingNode* _node;
  FreestyleMaterial _lightingmtl;
  CompositingPassData _accumCPD;
  CompositingPassData _decalCPD;
  fvec4 _clearColor;
  std::string _shadername;
  lev2::Texture* brdfIntegrationTexture() const;
  ////////////////////////////////////////////////////////////////////
  int _width                   = 0;
  int _height                  = 0;
  int _clusterW                = 0;
  int _clusterH                = 0;
  lev2::Texture* _whiteTexture = nullptr;
  ////////////////////////////////////////////////////////////////////
  std::vector<PointLight*> _pointlights;

  ////////////////////////////////////////////////////////////////////

  const FxShaderTechnique* _tekBaseLighting              = nullptr;
  const FxShaderTechnique* _tekEnvironmentLighting       = nullptr;
  const FxShaderTechnique* _tekEnvironmentLightingStereo = nullptr;
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

  const FxShaderParam* _parMatIVPArray         = nullptr;
  const FxShaderParam* _parMatPArray           = nullptr;
  const FxShaderParam* _parMatVArray           = nullptr;
  const FxShaderParam* _parZndc2eye            = nullptr;
  const FxShaderParam* _parMapGBufAlbAo        = nullptr;
  const FxShaderParam* _parMapGBufNrmL         = nullptr;
  const FxShaderParam* _parMapGBufRufMtlAlpha  = nullptr;
  const FxShaderParam* _parMapDepth            = nullptr;
  const FxShaderParam* _parMapDepthCluster     = nullptr;
  const FxShaderParam* _parMapSpecularEnv      = nullptr;
  const FxShaderParam* _parMapDiffuseEnv       = nullptr;
  const FxShaderParam* _parMapBrdfIntegration  = nullptr;
  const FxShaderParam* _parTime                = nullptr;
  const FxShaderParam* _parNearFar             = nullptr;
  const FxShaderParam* _parInvViewSize         = nullptr;
  const FxShaderParam* _parInvVpDim            = nullptr;
  const FxShaderParam* _parNumLights           = nullptr;
  const FxShaderParam* _parTileDim             = nullptr;
  const FxShaderParam* _parSpecularLevel       = nullptr;
  const FxShaderParam* _parDiffuseLevel        = nullptr;
  const FxShaderParam* _parAmbientLevel        = nullptr;
  const FxShaderParam* _parSkyboxLevel         = nullptr;
  const FxShaderParam* _parEnvironmentMipBias  = nullptr;
  const FxShaderParam* _parEnvironmentMipScale = nullptr;
  const FxShaderParam* _parDepthFogDistance    = nullptr;
  const FxShaderParam* _parDepthFogPower       = nullptr;
  const FxShaderParamBlock* _lightblock        = nullptr;
  const FxShaderParam* _parLightCookieTexture  = nullptr;

  ////////////////////////////////////////////////////////////////////

  RtGroupRenderTarget* _accumRT      = nullptr;
  RtGroupRenderTarget* _gbuffRT      = nullptr;
  RtGroupRenderTarget* _decalRT      = nullptr;
  RtGroupRenderTarget* _clusterRT    = nullptr;
  lev2::Texture* _brdfIntegrationMap = nullptr;

  CaptureBuffer _clustercapture;
  RtGroup* _rtgGbuffer      = nullptr;
  RtGroup* _rtgDecal        = nullptr;
  RtGroup* _rtgDepthCluster = nullptr;
  RtGroup* _rtgLaccum       = nullptr;
  PoolString _layername;
  float _specularLevel    = 1.0f;
  float _diffuseLevel     = 1.0f;
  float _depthFogPower    = 1.0f;
  float _depthFogDistance = 1.0f;
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
  void DoInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::RtBuffer* GetOutput() const final;
  svar256_t _impl;
};

///////////////////////////////////////////////////////////////////////////////

struct DeferredCompositingNodePbr : public RenderCompositingNode {
  DeclareConcreteX(DeferredCompositingNodePbr, RenderCompositingNode);

public:
  DeferredCompositingNodePbr();
  ~DeferredCompositingNodePbr();

  lev2::Texture* envSpecularTexture() const;
  lev2::Texture* envDiffuseTexture() const;

  float environmentIntensity() const {
    return _environmentIntensity;
  }
  float environmentMipBias() const {
    return _environmentMipBias;
  }
  float environmentMipScale() const {
    return _environmentMipScale;
  }
  float diffuseLevel() const {
    return _diffuseLevel;
  }
  float specularLevel() const {
    return _specularLevel;
  }
  fvec3 ambientLevel() const {
    return _ambientLevel;
  }
  float skyboxLevel() const {
    return _skyboxLevel;
  }
  float depthFogDistance() const {
    return _depthFogDistance;
  }
  float depthFogPower() const {
    return _depthFogPower;
  }

  void DoInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;
  void _readEnvTexture(ork::rtti::ICastable*& tex) const;
  void _writeEnvTexture(ork::rtti::ICastable* const& tex);

  void setEnvTexturePath(file::Path path);

  lev2::TextureAsset* _environmentTextureAsset = nullptr;

  lev2::RtBuffer* GetOutput() const final;
  svar256_t _impl;
  float _environmentIntensity = 1.0f;
  float _environmentMipBias   = 0.0f;
  float _environmentMipScale  = 0.0f;
  float _diffuseLevel         = 1.0f;
  float _specularLevel        = 1.0f;
  float _skyboxLevel          = 1.0f;
  float _depthFogDistance     = 1000.0f;
  float _depthFogPower        = 1.0f;
  fvec3 _ambientLevel;
  lev2::Texture* _filtenvSpecularMap = nullptr;
  lev2::Texture* _filtenvDiffuseMap  = nullptr;
  fvec4 _clearColor;
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
  void DoInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::RtBuffer* GetOutput() const final;
  svar256_t _impl;
};

#endif

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::deferrednode
