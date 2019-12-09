////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/material_freestyle.inl>

#include "NodeCompositorDeferred.h"

ImplementReflectionX(ork::lev2::deferrednode::DeferredCompositingNodeDebugNormal, "DeferredCompositingNodeDebugNormal");

// fvec3 LightColor
// fvec4 LightPosR 16byte
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::deferrednode {
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodeDebugNormal::describeX(class_t* c) {
  c->memberProperty("ClearColor", &DeferredCompositingNodeDebugNormal::_clearColor);
  c->memberProperty("FogColor", &DeferredCompositingNodeDebugNormal::_fogColor);
  c->memberProperty("Ambient", &DeferredCompositingNodeDebugNormal::_ambient);
  c->floatProperty("EnvironmentIntensity", float_range{-5,5},&DeferredCompositingNodeDebugNormal::_environmentIntensity);
  c->floatProperty("EnvironmentMipBias", float_range{0,12},&DeferredCompositingNodeDebugNormal::_environmentMipBias);
  c->floatProperty("DiffuseIntensity", float_range{-5,5},&DeferredCompositingNodeDebugNormal::_diffuseIntensity);
  auto texprop = c->accessorProperty("EnvironmentTexture", &DeferredCompositingNodeDebugNormal::_readEnvTexture, &DeferredCompositingNodeDebugNormal::_writeEnvTexture);
  texprop->annotate<ConstString>("editor.class", "ged.factory.assetlist");
  texprop->annotate<ConstString>("editor.assettype", "lev2tex");
  texprop->annotate<ConstString>("editor.assetclass", "lev2tex");

}
void DeferredCompositingNodeDebugNormal::_readEnvTexture(ork::rtti::ICastable *&tex) const {
  tex = _environmentTextureAsset;
}
void DeferredCompositingNodeDebugNormal::_writeEnvTexture(ork::rtti::ICastable *const &tex) {
_environmentTextureAsset = tex ? rtti::autocast(tex) : nullptr;
}

  lev2::Texture* DeferredCompositingNodeDebugNormal::envTexture() const {
  return _environmentTextureAsset ? _environmentTextureAsset->GetTexture() : nullptr;
}


///////////////////////////////////////////////////////////////////////////////
struct IMPL {
  static const int KMAXLIGHTS=8;
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  IMPL(DeferredCompositingNodeDebugNormal* node)
      : _camname(AddPooledString("Camera"))
      , _context(node,"orkshader://deferred",KMAXLIGHTS){
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ~IMPL() {}
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void init(lev2::GfxTarget* target) {
    _context.gpuInit(target);
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void _render(DeferredCompositingNodeDebugNormal* node, CompositorDrawData& drawdata) {
    //_timer.Start();
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto targ                    = RCFD.GetTarget();
    auto CIMPL                   = drawdata._cimpl;
    auto FBI                     = targ->FBI();
    auto this_buf                = FBI->GetThisBuffer();
    auto RSI                     = targ->RSI();
    const auto TOPCPD = CIMPL->topCPD();
    //////////////////////////////////////////////////////
    _context.renderUpdate(drawdata);
    auto VD = _context.computeViewData(drawdata);
    _context.update(VD);
    _context._clearColor = node->_clearColor;
    /////////////////////////////////////////////////////////////////////////////////////////
    targ->debugPushGroup("Deferred::render");
      _context.renderGbuffer(drawdata, VD);
      targ->debugPushGroup("Deferred::LightAccum");
    _context._accumCPD = TOPCPD;
    /////////////////////////////////////////////////////////////////
    auto vprect   = SRect(0, 0, _context._width, _context._height);
    auto quadrect = SRect(0, 0, _context._width, _context._height);
    _context._accumCPD.SetDstRect(vprect);
    _context._accumCPD._irendertarget        = _context._accumRT;
    _context._accumCPD._cameraMatrices       = nullptr;
    _context._accumCPD._stereoCameraMatrices = nullptr;
    _context._accumCPD._stereo1pass          = false;
    CIMPL->pushCPD(_context._accumCPD); // base lighting
      FBI->SetAutoClear(true);
      FBI->PushRtGroup(_context._rtgLaccum);
      targ->BeginFrame();
      FBI->Clear(fvec4(0.1, 0.2, 0.3, 1), 1.0f);
      //////////////////////////////////////////////////////////////////
      // base lighting
      //////////////////////////////////////////////////////////////////
    targ->debugPushGroup("Deferred::BaseLighting");
      _context._lightingmtl.bindTechnique(_context._tekDebugNormal);
      _context._lightingmtl._rasterstate.SetBlending(EBLENDING_OFF);
      _context._lightingmtl._rasterstate.SetDepthTest(EDEPTHTEST_OFF);
      _context._lightingmtl._rasterstate.SetCullTest(ECULLTEST_PASS_BACK);
      _context._lightingmtl.begin(RCFD);
      //////////////////////////////////////////////////////
      _context._lightingmtl.bindParamMatrixArray(_context._parMatIVPArray, VD._ivp, 2);
      _context._lightingmtl.bindParamMatrixArray(_context._parMatVArray, VD._v, 2);
      _context._lightingmtl.bindParamMatrixArray(_context._parMatPArray, VD._p, 2);
      _context._lightingmtl.bindParamCTex(_context._parMapGBufAlbAo, _context._rtgGbuffer->GetMrt(0)->GetTexture());
      _context._lightingmtl.bindParamCTex(_context._parMapGBufNrmL, _context._rtgGbuffer->GetMrt(1)->GetTexture());
      _context._lightingmtl.bindParamCTex(_context._parMapGBufRufMtlAlpha, _context._rtgGbuffer->GetMrt(2)->GetTexture());
      _context._lightingmtl.bindParamCTex(_context._parMapDepth, _context._rtgGbuffer->_depthTexture);

      if( node->envTexture() )
        _context._lightingmtl.bindParamCTex(_context._parMapEnvironment, node->envTexture() );

      _context._lightingmtl.bindParamFloat(_context._parEnvironmentIntensity, node->environmentIntensity() );
      _context._lightingmtl.bindParamFloat(_context._parEnvironmentMipBias, node->environmentMipBias() );
      _context._lightingmtl.bindParamFloat(_context._parDiffuseIntensity, node->diffuseIntensity() );
      _context._lightingmtl.bindParamVec3(_context._parAmbient, node->ambient() );

      _context._lightingmtl.bindParamVec2(_context._parNearFar, fvec2(0.1, 1000));
      _context._lightingmtl.bindParamVec2(_context._parInvViewSize, fvec2(1.0 / float(_context._width), 1.0f / float(_context._height)));
      _context._lightingmtl.commit();
      RSI->BindRasterState(_context._lightingmtl._rasterstate);
      this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0));
      _context._lightingmtl.end(RCFD);
    CIMPL->popCPD();       // base lighting
    targ->debugPopGroup(); // BaseLighting
    targ->EndFrame();
    FBI->PopRtGroup();     // deferredRtg


      targ->debugPopGroup(); // "Deferred::LightAccum"
    targ->debugPopGroup(); // "Deferred::render"
    // float totaltime = _timer.SecsSinceStart();
    // printf( "Deferred::_render totaltime<%g>\n", totaltime );
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  PoolString _camname;

  DeferredContext _context;
  int _sequence = 0;
  std::atomic<int> _lightjobcount;
  ork::Timer _timer;
  FxShaderParamBuffer* _lightbuffer = nullptr;
}; // IMPL

///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNodeDebugNormal::DeferredCompositingNodeDebugNormal() { _impl = std::make_shared<IMPL>(this); }
///////////////////////////////////////////////////////////////////////////////
DeferredCompositingNodeDebugNormal::~DeferredCompositingNodeDebugNormal() {}
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodeDebugNormal::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) { _impl.Get<std::shared_ptr<IMPL>>()->init(pTARG); }
///////////////////////////////////////////////////////////////////////////////
void DeferredCompositingNodeDebugNormal::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.Get<std::shared_ptr<IMPL>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
RtBuffer* DeferredCompositingNodeDebugNormal::GetOutput() const {
  static int i = 0;
  i++;
  return _impl.Get<std::shared_ptr<IMPL>>()->_context._rtgLaccum->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::deferrednode
