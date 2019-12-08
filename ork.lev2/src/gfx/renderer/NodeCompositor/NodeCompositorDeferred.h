////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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

template <typename T> inline bool doRangesOverlap(T amin, T amax, T bmin, T bmax){
  return std::max(amin, bmin) <= std::min(amax, bmax);
}
///////////////////////////////////////////////////////////////////////////////

struct PointLight {

  PointLight() {}
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
  static constexpr float KNEAR               = 0.1f;
  static constexpr float KFAR                = 100000.0f;
  ////////////////////////////////////////////////////////////////////
  DeferredContext(RenderCompositingNode* node, std::string shadername, int numlights);
  ~DeferredContext();
  ////////////////////////////////////////////////////////////////////
  ViewData computeViewData(CompositorDrawData& drawdata);
  ////////////////////////////////////////////////////////////////////
  void update(const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  void gpuInit(GfxTarget* target);
  const uint32_t* captureDepthClusters(CompositorDrawData& drawdata, const ViewData& VD);
  void renderUpdate(CompositorDrawData& drawdata);
  void renderGbuffer(CompositorDrawData& drawdata, const ViewData& VD);
  void renderBaseLighting(CompositorDrawData& drawdata, const ViewData& VD);
  void beginPointLighting(CompositorDrawData& drawdata, const ViewData& VD);
  void endPointLighting(CompositorDrawData& drawdata, const ViewData& VD);
  ////////////////////////////////////////////////////////////////////
  RenderCompositingNode* _node;
  FreestyleMaterial _lightingmtl;
  CompositingPassData _accumCPD;
  fvec4 _clearColor;
  std::string _shadername;
  ////////////////////////////////////////////////////////////////////
  int _width   = 0;
  int _height  = 0;
  int _clusterW = 0;
  int _clusterH = 0;
  ////////////////////////////////////////////////////////////////////
  std::vector<PointLight*> _pointlights;

  ////////////////////////////////////////////////////////////////////

  const FxShaderTechnique* _tekBaseLighting          = nullptr;
  const FxShaderTechnique* _tekPointLighting         = nullptr;
  const FxShaderTechnique* _tekDebugNormal           = nullptr;
  const FxShaderTechnique* _tekBaseLightingStereo    = nullptr;
  const FxShaderTechnique* _tekPointLightingStereo   = nullptr;
  const FxShaderTechnique* _tekDownsampleDepthCluster = nullptr;
  
  
#if defined(ENABLE_COMPUTE_SHADERS)
  FxComputeShader* _lightcollectcomputeshader = nullptr;
#endif

  const FxShaderParam* _parMatIVPArray               = nullptr;
  const FxShaderParam* _parMatPArray                 = nullptr;
  const FxShaderParam* _parMatVArray                 = nullptr;
  const FxShaderParam* _parZndc2eye                  = nullptr;
  const FxShaderParam* _parMapGBufAlbAo              = nullptr;
  const FxShaderParam* _parMapGBufNrmL               = nullptr;
  const FxShaderParam* _parMapGBufRufMtlAlpha        = nullptr;
  const FxShaderParam* _parMapDepth                  = nullptr;
  const FxShaderParam* _parMapDepthCluster           = nullptr;
  const FxShaderParam* _parMapEnvironment            = nullptr;
  const FxShaderParam* _parTime                      = nullptr;
  const FxShaderParam* _parNearFar                   = nullptr;
  const FxShaderParam* _parInvViewSize               = nullptr;
  const FxShaderParam* _parInvVpDim                  = nullptr;
  const FxShaderParam* _parNumLights                 = nullptr;
  const FxShaderParam* _parTileDim                   = nullptr;
  const FxShaderParam* _parEnvironmentIntensity      = nullptr;
  const FxShaderParam* _parEnvironmentMipBias        = nullptr;
  const FxShaderParamBlock* _lightblock              = nullptr;

  ////////////////////////////////////////////////////////////////////

  RtGroupRenderTarget* _accumRT  = nullptr;
  RtGroupRenderTarget* _gbuffRT  = nullptr;
  RtGroupRenderTarget* _clusterRT = nullptr;

  CaptureBuffer _clustercapture;
  RtGroup* _rtgGbuffer = nullptr;
  RtGroup* _rtgDepthCluster = nullptr;
  RtGroup* _rtgLaccum  = nullptr;
  PoolString _layername;
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
  void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::RtBuffer* GetOutput() const final;
  svar256_t _impl;
};

///////////////////////////////////////////////////////////////////////////////

class DeferredCompositingNodeDebugNormal : public RenderCompositingNode {
  DeclareConcreteX(DeferredCompositingNodeDebugNormal, RenderCompositingNode);

public:
  DeferredCompositingNodeDebugNormal();
  ~DeferredCompositingNodeDebugNormal();
  fvec4 _clearColor;
  fvec4 _fogColor;
  lev2::Texture* envTexture() const;
  float environmentIntensity() const { return _environmentIntensity; }
  float environmentMipBias() const { return _environmentMipBias; }

private:
  void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;
  void _readEnvTexture(ork::rtti::ICastable *&tex) const;
  void _writeEnvTexture(ork::rtti::ICastable *const &tex);
  lev2::TextureAsset*                     _environmentTextureAsset = nullptr;

  lev2::RtBuffer* GetOutput() const final;
  svar256_t _impl;
  float _environmentIntensity = 1.0f;
  float _environmentMipBias = 0.0f;
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
  void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::RtBuffer* GetOutput() const final;
  svar256_t _impl;
};

#endif

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::deferrednode
