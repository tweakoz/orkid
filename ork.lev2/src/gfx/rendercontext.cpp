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
#include <ork/lev2/gfx/frametek.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/viewport.h>

template class ork::orklut<ork::PoolString, anyp>;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

const RenderContextInstData RenderContextInstData::Default;

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
    : mpShadowBuffer(0)
    , meMode(ERENDMODE_STANDARD)
    , mCameraData(0)
    , mPickCameraData(0)
    , mLightManager(0)
    , mpTarget(0) {}

void RenderContextFrameData::SetUserProperty(const char* prop, anyp val) {
  PoolString PSprop                     = AddPooledString(prop);
  orklut<PoolString, anyp>::iterator it = mUserProperties.find(PSprop);
  if (it == mUserProperties.end())
    mUserProperties.AddSorted(PSprop, val);
  else
    it->second = val;
}
anyp RenderContextFrameData::GetUserProperty(const char* prop) {
  PoolString PSprop                           = AddPooledString(prop);
  orklut<PoolString, anyp>::const_iterator it = mUserProperties.find(PSprop);
  if (it != mUserProperties.end()) {
    return it->second;
  }
  anyp rval;
  return rval;
}

bool RenderContextFrameData::IsPickMode() const { return mpTarget ? mpTarget->FBI()->IsPickState() : false; }

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

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
