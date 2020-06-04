#include "uiToolsDefault.h"
#include "vpSceneEditor.h"
#include <ork/application/application.h>
#include <ork/kernel/future.hpp>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <orktool/qtui/qtui_tool.h>
#include <pkg/ent/scene.h>
#include <ork/lev2/gfx/material_pbr.inl>
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

ScenePickBuffer::ScenePickBuffer(SceneEditorVP* vp, Context* ctx)
    : PickBuffer(vp, ctx, 0, 0) {
  _scenevp = vp;
}
void ScenePickBuffer::Draw(lev2::PixelFetchContext& ctx) {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());

  const ent::Simulation* sim = _scenevp->simulation();
  ent::SceneData* pscene     = _scenevp->mEditor.mpScene;

  if (nullptr == pscene)
    return;
  if (nullptr == sim)
    return;
  if (false == _scenevp->mbSceneDisplayEnable)
    return;

  float mainvpW      = _scenevp->width();
  float mainvpH      = _scenevp->height();
  float mainVPAspect = mainvpW / mainvpH;

  this->resize(2048, 2048);
  //_rtgroup->Resize(2048, 2048);

  auto target = ctx._gfxContext;
  target->makeCurrentContext();
  ///////////////////////////////////////////////////////////////////////////
  static CompositingData* _gdata = nullptr;
  if (nullptr == _gdata) {
    _gdata = new CompositingData;
    _gdata->presetPicking();
  }
  ///////////////////////////////////////////////////////////////////////////
  static auto _gimpl = _gdata->createImpl();
  ork::lev2::RenderContextFrameData RCFD(target); //
  RCFD._cimpl = _gimpl;
  ///////////////////////////////////////////////////////////////////////////

  mPickIds.clear();

  ork::recursive_mutex& glock = lev2::GfxEnv::GetRef().GetGlobalLock();
  glock.Lock(0x777);
  // PickFrameTechnique pktek;
  //_scenevp->PushFrameTechnique(&pktek);
  Context* pTEXTARG = context();
  pTEXTARG->pushRenderContextFrameData(&RCFD);
  ViewportRect tgt_rect(0, 0, _scenevp->width(), _scenevp->height());
  ///////////////////////////////////////////////////////////////////////////
  auto irenderer = _scenevp->GetRenderer();
  irenderer->setContext(pTEXTARG);
  RCFD.SetLightManager(nullptr);
  ///////////////////////////////////////////////////////////////////////////
  // force aspect ratio to that of the parent visible viewport
  //  as opposed to the pickbuffer size
  ///////////////////////////////////////////////////////////////////////////
  // CPD.cameraMatrices()->_aspectRatio = fW / fH;
  ///////////////////////////////////////////////////////////////////////////
  auto DB = DrawableBuffer::acquireForRead(7); // mDbLock.Aquire(7);
  if (DB) {
    lev2::UiViewportRenderTarget rt(_scenevp);
    rendervar_t passdata;
    passdata.Set<compositingpassdatastack_t*>(&_scenevp->_compositingGroupStack);
    RCFD.setUserProperty("nodes"_crc, passdata);
    RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));
    lev2::CompositingPassData CPD;
    CPD.AddLayer("All");
    CPD.SetDstRect(tgt_rect);
    CPD._ispicking     = true;
    CPD._irendertarget = &rt;
    _gimpl->pushCPD(CPD);
    ///////////////////////////////////////////////////////////////////////////
    auto simmode = sim->GetSimulationMode();
    bool running = (simmode == ent::ESCENEMODE_RUN);
    lev2::FrameRenderer framerenderer(RCFD, [&]() {});
    lev2::CompositorDrawData drawdata(framerenderer);
    drawdata._cimpl = _gimpl;
    drawdata._properties["primarycamindex"_crcu].Set<int>(_scenevp->miCameraIndex);
    drawdata._properties["cullcamindex"_crcu].Set<int>(_scenevp->miCullCameraIndex);
    drawdata._properties["irenderer"_crcu].Set<lev2::IRenderer*>(irenderer);
    drawdata._properties["simrunning"_crcu].Set<bool>(running);
    drawdata._properties["DB"_crcu].Set<const DrawableBuffer*>(DB);
    ///////////////////////////////////////////////////////////////////////////
    auto FBI    = pTEXTARG->FBI();
    auto vprect = _rtgroup->viewportRect();
    ///////////////////////////////////////////////////////////////////////////
    FBI->PushRtGroup(_rtgroup); // Enable Mrt
    FBI->EnterPickState(this);
    //_context->BindMaterial(GfxEnv::GetDefault3DMaterial());
    _context->PushModColor(fcolor4::Yellow());

    drawdata._cimpl   = _gimpl;
    bool assembled_ok = _gimpl->assemble(drawdata);

    DrawableBuffer::releaseFromRead(DB); // mDbLock.Aquire(7);

    if (assembled_ok)
      _gimpl->composite(drawdata);

    _scenevp->DrawManip(drawdata, target);

    _context->PopModColor();
    FBI->PopRtGroup();
    FBI->LeavePickState();
  } // if(DB)
  ///////////////////////////////////////////////////////////////////////////
  _gimpl->popCPD();
  ///////////////////////////////////////////////////////////////////////////
  // SetDirty(false);
  pTEXTARG->popRenderContextFrameData();
  //_scenevp->PopFrameTechnique();
  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();

  ///////////////////////////////////////////////////////////////////////////
}

DeferredPickOperationContext::DeferredPickOperationContext(ui::event_constptr_t srcev)
    : miX(0)
    , miY(0)
    , is_left(false)
    , is_mid(false)
    , is_right(false)
    , is_ctrl(false)
    , is_shift(false)
    , mpCastable(nullptr)
    , mOnPick(nullptr)
    , mHandler(nullptr)
    , mViewport(nullptr)

{
  mEV = std::make_shared<ui::Event>(*srcev.get());

  miX      = srcev->miX;
  miY      = srcev->miY;
  is_shift = srcev->mbSHIFT;
  is_ctrl  = srcev->mbCTRL;
  is_left  = srcev->mbLeftButton;
  is_right = srcev->mbRightButton;

  mState = 0;
}

///////////////////////////////////////////////////////////////////////////////
static auto gPickOPQ = std::make_shared<opq::OperationsQueue>(1, "PickOpQ");
void OuterPickOp(defpickopctx_ptr_t pickctx) {

  assert(pickctx->mViewport != nullptr);
  if (pickctx->mViewport == nullptr)
    return;

  SceneEditorVP* viewport = pickctx->mViewport;

  auto target = pickctx->_gfxContext;

  const ent::Simulation* psi   = viewport->simulation();
  const ent::SceneData* pscene = viewport->SceneEditor().mpScene;

  if (nullptr == pscene)
    return;
  if (nullptr == psi)
    return;
  if (nullptr == target)
    return;
  if (false == viewport->IsSceneDisplayEnabled())
    return;

  auto outer_op = [=]() {
    ork::opq::assertOnQueue2(gPickOPQ);
    ////////////
    // stop updates, and wait for mainthread to acknowledge
    ////////////
    gUpdateStatus.SetState(EUPD_STOP);
    opq::updateSerialQueue()->sync();
    ////////////
    static auto d_buf = new ork::lev2::DrawableBuffer(4);
    static CompositingData _gdata;
    static auto _gimpl = _gdata.createImpl();
    auto lamb          = [&]() {
      ork::opq::assertOnQueue2(opq::updateSerialQueue());
      d_buf->miBufferIndex = 0;
      //////////////////////////////////////////////////////////////////////////
      // enqueue scene to picking specific DrawableBuffer
      //////////////////////////////////////////////////////////////////////////
      psi->enqueueDrawablesToBuffer(*d_buf);
      ////////////
      opq::mainSerialQueue()->sync();
      ////////////
      auto op_pick = [=]() {
        auto target = pickctx->_gfxContext;
        ork::lev2::RenderContextFrameData RCFD(target);
        RCFD._cimpl = _gimpl;
        rendervar_t db_var;
        db_var.Set<const DrawableBuffer*>(d_buf);
        RCFD.setUserProperty("DB"_crc, db_var);
        ork::opq::assertOnQueue2(opq::mainSerialQueue());
        pickctx->mState       = 1;
        auto& pixel_ctx       = pickctx->_pixelctx;
        pixel_ctx._gfxContext = target;
        pixel_ctx.miMrtMask   = 3;
        pixel_ctx.mUsage[0]   = lev2::PixelFetchContext::EPU_PTR64;
        pixel_ctx.mUsage[1]   = lev2::PixelFetchContext::EPU_FLOAT;
        pixel_ctx.mUserData.Set<ork::lev2::RenderContextFrameData*>(&RCFD);
        target->makeCurrentContext();

        viewport->GetPixel(pickctx->miX, pickctx->miY, pixel_ctx); // HERE<<<<<<
        const auto& colr0   = pickctx->_pixelctx._pickvalues[0];
        const auto& colr1   = pickctx->_pixelctx._pickvalues[1];
        pickctx->mpCastable = pixel_ctx.GetObject(viewport->pickbuffer(), 0);
        // printf("GOTCLR0<%g %g %g %g>\n", colr0.x, colr0.y, colr0.z, colr0.w);
        // printf("GOTCLR1<%g %g %g %g>\n", colr1.x, colr1.y, colr1.z, colr1.w);
        // printf("GOTOBJ<%p>\n", pickctx->mpCastable);
        if (pickctx->mOnPick) {
          auto on_pick = [=]() {
            pickctx->mOnPick(pickctx);
            pickctx->mState = 2;
            gUpdateStatus.SetState(EUPD_START);
          };
          opq::Op(on_pick).QueueASync(opq::updateSerialQueue());
        } else {
          pickctx->mState = 3;
          gUpdateStatus.SetState(EUPD_START);
        }
      };
      opq::Op(op_pick).QueueSync(opq::mainSerialQueue());
    };
    opq::Op(lamb).QueueSync(opq::updateSerialQueue()); // HERE<<<<<<
  };
  opq::Op(outer_op).QueueASync(gPickOPQ);
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::GetPixel(int ix, int iy, lev2::PixelFetchContext& ctx) {
  if (nullptr == _pickbuffer)
    return;

  float fx = float(ix) / float(miW);
  float fy = float(iy) / float(miH);

  ctx.mRtGroup = _pickbuffer->_rtgroup;

  /////////////////////////////////////////////////////////////
  int iW = _pickbuffer->context()->mainSurfaceWidth();
  int iH = _pickbuffer->context()->mainSurfaceHeight();
  /////////////////////////////////////////////////////////////
  _pickbuffer->context()->FBI()->setViewport(ViewportRect(0, 0, iW, iH));
  _pickbuffer->context()->FBI()->setScissor(ViewportRect(0, 0, iW, iH));
  /////////////////////////////////////////////////////////////
  // force a pick refresh
  /////////////////////////////////////////////////////////////

  _pickbuffer->Draw(ctx);

  /////////////////////////////////////////////////////////////

  _pickbuffer->context()->FBI()->GetPixel(fvec4(fx, fy, 0.0f), ctx);
}
}} // namespace ork::ent

///////////////////////////////////////////////////////////////////////////////
