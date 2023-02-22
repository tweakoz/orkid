////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
CallbackDrawable::CallbackDrawable(DrawableOwner* pent)
    : Drawable()
    , mDataDestroyer(nullptr)
    , mRenderCallback(nullptr)
    , _enqueueOnLayerCallback(nullptr)
    , _renderLambda(nullptr){
}
///////////////////////////////////////////////////////////////////////////////

CallbackDrawable::~CallbackDrawable() {
  if (mDataDestroyer) {
    mDataDestroyer->Destroy();
  }
  mDataDestroyer          = nullptr;
  _enqueueOnLayerCallback = nullptr;
  mRenderCallback         = nullptr;
  _renderLambda = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void CallbackDrawable::_renderWithLambda(RenderContextInstData& RCID) {
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    auto drawable = renderable->_drawable;
    OrkAssert(drawable!=nullptr);
    OrkAssert(drawable->_renderLambda!=nullptr);
    drawable->_renderLambda(RCID);
}

///////////////////////////////////////////////////////////////////////////////

void CallbackDrawable::setRenderLambda(RLCBType cb) {
  mRenderCallback = _renderWithLambda;
  _renderLambda = cb;
}

///////////////////////////////////////////////////////////////////////////////
drawablebufitem_ptr_t CallbackDrawable::enqueueOnLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  auto item = buffer.enqueueDrawable(xfdata, this);
  if (_enqueueOnLayerCallback) {
    _enqueueOnLayerCallback(item);
  }
  if (_enqueueOnLayerLambda) {
    _enqueueOnLayerLambda(item);
  }
  return item;
}
///////////////////////////////////////////////////////////////////////////////
void CallbackDrawable::enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());

  const auto& DQDATA = item->mXfData;

  lev2::CallbackRenderable& renderable = renderer->enqueueCallback();
  auto matrix                   = DQDATA._worldTransform->composed();

  // auto str                             = matrix.dump4x3cn();
  // printf("XFX: %s\n", str.c_str());
  renderable.SetMatrix(matrix);
  renderable.SetObject(GetOwner());
  renderable.SetRenderCallback(mRenderCallback);
  renderable.SetSortKey(_sortkey);
  renderable.SetDrawableDataA(GetUserDataA());
  renderable.SetDrawableDataB(GetUserDataB());
  renderable.SetModColor(DQDATA._modcolor);
  renderable._drawable = this;
}
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CallbackRenderable::CallbackRenderable(IRenderer* renderer)
    : IRenderable()
    , mSortKey(0)
    , mMaterialIndex(0)
    , mMaterialPassIndex(0)
    , _drawable(nullptr)
    //, mUserData1()
    , mRenderCallback(0) {
}
/////////////////////////////////////////////////////////////////////
void CallbackRenderable::Render(const IRenderer* renderer) const {
  renderer->RenderCallback(*this);
}
/////////////////////////////////////////////////////////////////////
void CallbackRenderable::SetSortKey(uint32_t skey) {
  mSortKey = skey;
}
/////////////////////////////////////////////////////////////////////
//void CallbackRenderable::SetUserData0(IRenderable::var_t pdata) {
  //mUserData0 = pdata;
//}
/////////////////////////////////////////////////////////////////////
//const IRenderable::var_t& CallbackRenderable::GetUserData0() const {
  //return mUserData0;
//}
/////////////////////////////////////////////////////////////////////
//void CallbackRenderable::SetUserData1(IRenderable::var_t pdata) {
  //mUserData1 = pdata;
//}
/////////////////////////////////////////////////////////////////////
//const IRenderable::var_t& CallbackRenderable::GetUserData1() const {
  //return mUserData1;
//}
/////////////////////////////////////////////////////////////////////
void CallbackRenderable::SetRenderCallback(cbtype_t cb) {
  mRenderCallback = cb;
}
/////////////////////////////////////////////////////////////////////
CallbackRenderable::cbtype_t CallbackRenderable::GetRenderCallback() const {
  return mRenderCallback;
}
/////////////////////////////////////////////////////////////////////
uint32_t CallbackRenderable::ComposeSortKey(const IRenderer* renderer) const {
  return mSortKey;
}
/////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
