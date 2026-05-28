////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/reflect/properties/registerX.inl>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
CallbackDrawable::CallbackDrawable(DrawableContainer* pent)
    : Drawable()
    , mDataDestroyer(nullptr)
    , mRenderCallback(nullptr)
    , _enqueueOnLayerCallback(nullptr)
    , _renderLambda(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

CallbackDrawable::~CallbackDrawable() {
  if (mDataDestroyer) {
    mDataDestroyer->Destroy();
  }
  mDataDestroyer          = nullptr;
  _enqueueOnLayerCallback = nullptr;
  mRenderCallback         = nullptr;
  _renderLambda           = nullptr;
}
///////////////////////////////////////////////////////////////////////////////

void CallbackDrawable::SetDataDestroyer(ICallbackDrawableDataDestroyer* pdestroyer) {
  mDataDestroyer = pdestroyer;
}
///////////////////////////////////////////////////////////////////////////////
void CallbackDrawable::SetRenderCallback(lev2::CallbackRenderable::cbtype_t cb) {
  mRenderCallback = cb;
}
///////////////////////////////////////////////////////////////////////////////

void CallbackDrawable::setEnqueueOnLayerCallback(Q2LCBType cb) {
  _enqueueOnLayerCallback = cb;
}
///////////////////////////////////////////////////////////////////////////////
void CallbackDrawable::setEnqueueOnLayerLambda(Q2LLambdaType cb) {
  _enqueueOnLayerLambda = cb;
}

///////////////////////////////////////////////////////////////////////////////

void CallbackDrawable::_renderWithLambda(RenderContextInstData& RCID) {
  auto renderable = (const CallbackRenderable*)(RCID._irenderable);
  auto drawable   = (const CallbackDrawable*) renderable->_drawable;
  OrkAssert(drawable != nullptr);
  OrkAssert(drawable->_renderLambda != nullptr);
  drawable->_renderLambda(RCID);
}

///////////////////////////////////////////////////////////////////////////////

void CallbackDrawable::setRenderLambda(RLCBType cb) {
  mRenderCallback = _renderWithLambda;
  _renderLambda   = cb;
}

///////////////////////////////////////////////////////////////////////////////
drawqueueitem_ptr_t CallbackDrawable::enqueueOnLayer(const DrawQueueTransferData& xfdata, DrawQueueLayer& buffer) const {
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
void CallbackDrawable::enqueueToRenderQueue(drawqueueitem_constptr_t item, lev2::IRenderer* renderer) const {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());

  if (!_renderEnabled.load(std::memory_order_acquire)) return;

  const auto& DQDATA = item->_dqxferdata;

  lev2::CallbackRenderable& renderable = renderer->enqueueCallback();
  auto matrix                          = DQDATA._worldTransform->composed();

  renderable._view_relative = DQDATA._worldTransform->_view_relative;
  renderable.SetMatrix(matrix);
  renderable._pickID = _pickID;
  renderable.SetRenderCallback(mRenderCallback);
  renderable._sortkey = _sortkey;
  renderable.SetDrawableDataA(GetUserDataA());
  renderable.SetDrawableDataB(GetUserDataB());
  renderable.SetModColor(DQDATA._modcolor);
  renderable._drawable = this;
  // Propagate per-drawable IBL overrides (PBR2 Phase 0).
  // The Drawable owns the overrides; copy onto the renderable so the
  // per-draw bind in fwdnode_pipeline.cpp sees them via RCID.
  renderable._envmapOverride = _envmapOverride;
  renderable._probeOverride  = _probeOverride;
}
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CallbackRenderable::CallbackRenderable(IRenderer* renderer)
    : IRenderable() {
}
/////////////////////////////////////////////////////////////////////
void CallbackRenderable::Render(const IRenderer* renderer) const {
  renderer->_renderCallbackRenderable(*this);
}
/////////////////////////////////////////////////////////////////////
void CallbackRenderable::SetRenderCallback(cbtype_t cb) {
  mRenderCallback = cb;
}
/////////////////////////////////////////////////////////////////////
CallbackRenderable::cbtype_t CallbackRenderable::GetRenderCallback() const {
  return mRenderCallback;
}
/////////////////////////////////////////////////////////////////////
drawable_ptr_t CallbackDrawableData::createDrawable() const {
  auto drw             = std::make_shared<CallbackDrawable>(nullptr);
  drw->mRenderCallback = mRenderCallback;
  return drw;
}
CallbackDrawableData::CallbackDrawableData() {
}
void CallbackDrawableData::SetRenderCallback(lev2::CallbackRenderable::cbtype_t cb) {
  mRenderCallback = cb;
}
void CallbackDrawableData::describeX(object::ObjectClass* clazz) {
}
/////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
ImplementReflectionX(ork::lev2::CallbackDrawableData, "CallbackDrawableData");
