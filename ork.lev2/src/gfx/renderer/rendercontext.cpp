////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>

#include <ork/application/application.h>
#include <ork/kernel/any.h>
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/renderer/frametek.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/compositor.h>

template class ork::orklut<ork::CrcString, ork::lev2::rendervar_t>;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

const RenderContextInstData RenderContextInstData::Default;

///////////////////////////////////////////////////////////////////////////////

fmtx4 StereoCamera::VL() const {
  return _left->GetVMatrix();
}
fmtx4 StereoCamera::VR() const {
  return _right->GetVMatrix();
}
fmtx4 StereoCamera::PL() const {
  return _left->GetPMatrix();
}
fmtx4 StereoCamera::PR() const {
  return _right->GetPMatrix();
}
fmtx4 StereoCamera::VPL() const {
  return _left->GetVMatrix()*_left->GetPMatrix();
}
fmtx4 StereoCamera::VPR() const {
  return _right->GetVMatrix()*_right->GetPMatrix();
}
fmtx4 StereoCamera::VMONO() const {
  return _mono->GetVMatrix();
}
fmtx4 StereoCamera::PMONO() const {
  return _mono->GetPMatrix();
}
fmtx4 StereoCamera::VPMONO() const {
  return _mono->GetVMatrix()*_mono->GetPMatrix();
}

fmtx4 StereoCamera::MVPL(const fmtx4& M) const {
  return (M*VL())*PL();
}
fmtx4 StereoCamera::MVPR(const fmtx4& M) const {
  return (M*VR())*PR();
}
fmtx4 StereoCamera::MVPMONO(const fmtx4& M) const {
  return (M*VMONO())*PMONO();
}

///////////////////////////////////////////////////////////////////////////////

RenderContextInstData::RenderContextInstData()
    : miMaterialIndex(0)
    , miMaterialPassIndex(0)
    , mpActiveRenderer(0)
    , mpDagRenderable(0)
    , mpLightingGroup(0)
    , mDPTopEnvMap(0)
    , mDPBotEnvMap(0)
    , mMaterialInst(0)
    , mbIsSkinned(false)
    , mbForzeNoZWrite(false)
    , mLightMap(0)
    , mbVertexLit(false)
    , mRenderGroupState(ERGST_NONE) {
  for (int i = 0; i < kMaxEngineParamFloats; i++)
    mEngineParamFloats[i] = 0.0f;
}

void RenderContextInstData::SetEngineParamFloat(int idx, float fv) {
  OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

  mEngineParamFloats[idx] = fv;
}

float RenderContextInstData::GetEngineParamFloat(int idx) const {
  OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

  return mEngineParamFloats[idx];
}

///////////////////////////////////////////////////////////////////////////////

/*U32 Renderer::ComposeSortKey( U32 texIndex, U32 depthIndex, U32 passIndex, U32 transIndex ) const
{
    static const u32 ktexbits = 10;
    static const u32 kpassbits = 3;
    static const u32 kdepthbits = 18;
    static const u32 ktransbits = 1;

    static const u32 ktexmask = (1<<ktexbits)-1;
    static const u32 kpassmask = (1<<kpassbits)-1;
    static const u32 kdepthmask = (1<<kdepthbits)-1;
    static const u32 ktransmask = (1<<ktransbits)-1;

    static const u32 kdepthshift = 0;
    static const u32 ktexshift = kdepthshift+kdepthbits;
    static const u32 kpassshift = ktexshift+ktexbits;
    static const u32 ktransshift = kpassshift+kpassbits;

    U32 uval	= ((transIndex & ktransmask) << ktransshift)
                | ((passIndex & kpassmask) << kpassshift)
                | ((texIndex & ktexmask) << ktexshift)
                | ((depthIndex & kdepthmask) << kdepthshift)
                ;

    return 0xffffffff-uval;
}*/

RenderContextFrameData::RenderContextFrameData()
    : mCameraData(0)
    , mLightManager(0)
    , mpTarget(0) {}

void RenderContextFrameData::setUserProperty(CrcString key, rendervar_t val) {
  auto it = _userProperties.find(key);
  if (it == _userProperties.end())
    _userProperties.AddSorted(key, val);
  else
    it->second = val;
}
void RenderContextFrameData::unSetUserProperty(CrcString key) {
  auto it = _userProperties.find(key);
  if (it == _userProperties.end())
    _userProperties.erase(it);
}

rendervar_t RenderContextFrameData::getUserProperty(CrcString key) const {
  auto it = _userProperties.find(key);
  if (it != _userProperties.end()) {
    return it->second;
  }
  rendervar_t rval(nullptr);
  return rval;
}

bool RenderContextFrameData::isPicking() const { return mpTarget ? mpTarget->FBI()->IsPickState() : false; }

void RenderContextFrameData::SetTarget(GfxTarget* ptarg) {
  mpTarget = ptarg;
  if (ptarg) {
    ptarg->SetRenderContextFrameData(this);
  }
}

void RenderContextFrameData::ClearLayers() { mLayers.clear(); }
void RenderContextFrameData::AddLayer(const PoolString& layername) { mLayers.insert(layername); }
bool RenderContextFrameData::HasLayer(const PoolString& layername) const { return (mLayers.find(layername) != mLayers.end()); }

///////////////////////////////////////////////////////////////////////////////

void RenderContextFrameData::PushRenderTarget(IRenderTarget* ptarg) { mRenderTargetStack.push(ptarg); }
IRenderTarget* RenderContextFrameData::GetRenderTarget() {
  IRenderTarget* pt = mRenderTargetStack.top();
  return pt;
}
void RenderContextFrameData::PopRenderTarget() { mRenderTargetStack.pop(); }

void RenderContextFrameData::setLayerName(const char* layername) {
  lev2::rendervar_t passdata;
  passdata.Set<const char*>(layername);
  setUserProperty("pass"_crc, passdata);
}

const DrawableBuffer* RenderContextFrameData::GetDB() const{
  lev2::rendervar_t pvdb   = getUserProperty("DB"_crc);
  const DrawableBuffer* DB = pvdb.Get<const DrawableBuffer*>();
  return DB;
}

///////////////////////////////////////////////////////////////////////////////

void RenderContextFrameData::addStandardLayers() {
  AddLayer("Default"_pool);
  AddLayer("A"_pool);
  AddLayer("B"_pool);
  AddLayer("C"_pool);
  AddLayer("D"_pool);
  AddLayer("E"_pool);
  AddLayer("F"_pool);
  AddLayer("G"_pool);
  AddLayer("H"_pool);
  AddLayer("I"_pool);
  AddLayer("J"_pool);
  AddLayer("K"_pool);
  AddLayer("L"_pool);
  AddLayer("M"_pool);
  AddLayer("N"_pool);
  AddLayer("O"_pool);
  AddLayer("P"_pool);
  AddLayer("Q"_pool);

}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
