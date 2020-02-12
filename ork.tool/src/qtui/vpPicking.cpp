#include "uiToolsDefault.h"
#include "vpSceneEditor.h"
#include <ork/application/application.h>
#include <ork/kernel/future.hpp>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <orktool/qtui/qtui_tool.h>
#include <pkg/ent/scene.h>
#include <ork/lev2/gfx/material_pbr.inl>
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

DeferredPickOperationContext::DeferredPickOperationContext()
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
  mState = 0;
}

///////////////////////////////////////////////////////////////////////////////
static opq::OperationsQueue gPickOPQ(1, "PickOpQ");
void OuterPickOp(DeferredPickOperationContext* pickctx) {

  assert(pickctx->mViewport != nullptr);
  if (pickctx->mViewport == nullptr)
    return;

  SceneEditorVP* viewport = pickctx->mViewport;

  auto target = viewport->GetTarget();

  const ent::Simulation* psi   = viewport->simulation();
  const ent::SceneData* pscene = viewport->SceneEditor().mpScene;

  if (nullptr == pscene)
    return;
  if (nullptr == psi)
    return;
  if (false == viewport->IsSceneDisplayEnabled())
    return;

  auto outer_op = [=]() {
    ork::opq::assertOnQueue2(gPickOPQ);
    ////////////
    // stop updates, and wait for mainthread to acknowledge
    ////////////
    gUpdateStatus.SetState(EUPD_STOP);
    opq::updateSerialQueue().sync();
    ////////////
    static auto d_buf = new ork::lev2::DrawableBuffer(4);

    rendervar_t db_var;
    db_var.Set<const DrawableBuffer*>(d_buf);

    static CompositingData _gdata;
    static CompositingImpl _gimpl(_gdata);
    static ork::lev2::RenderContextFrameData RCFD(target); //
    RCFD._cimpl = &_gimpl;
    RCFD.setUserProperty("DB"_crc, db_var);

    auto lamb = [&]() {
      ork::opq::assertOnQueue2(opq::updateSerialQueue());
      d_buf->miBufferIndex = 0;
      psi->enqueueDrawablesToBuffer(*d_buf);
      ////////////
      opq::mainSerialQueue().sync();
      ////////////
      auto op_pick = [&]() {
        ork::opq::assertOnQueue2(opq::mainSerialQueue());
        pickctx->mState     = 1;
        auto& pixel_ctx     = pickctx->_pixelctx;
        pixel_ctx.miMrtMask = 3;
        pixel_ctx.mUsage[0] = lev2::PixelFetchContext::EPU_PTR64;
        pixel_ctx.mUsage[1] = lev2::PixelFetchContext::EPU_FLOAT;
        pixel_ctx.mUserData.Set<ork::lev2::RenderContextFrameData*>(&RCFD);

        viewport->GetPixel(pickctx->miX, pickctx->miY, pixel_ctx); // HERE<<<<<<
        pickctx->mpCastable = pixel_ctx.GetObject(viewport->GetPickBuffer(), 0);
        printf("GOTOBJ<%p>\n", pickctx->mpCastable);
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
  if (nullptr == mpPickBuffer)
    return;

  float fx = float(ix) / float(miW);
  float fy = float(iy) / float(miH);

  ctx.mRtGroup  = mpPickBuffer->mpPickRtGroup;
  ctx.mAsBuffer = mpPickBuffer;

  /////////////////////////////////////////////////////////////
  // force a pick refresh

  mpPickBuffer->Draw(ctx);

  /////////////////////////////////////////////////////////////
  int iW = mpPickBuffer->context()->mainSurfaceWidth();
  int iH = mpPickBuffer->context()->mainSurfaceHeight();
  /////////////////////////////////////////////////////////////
  mpPickBuffer->context()->FBI()->SetViewport(0, 0, iW, iH);
  mpPickBuffer->context()->FBI()->SetScissor(0, 0, iW, iH);
  /////////////////////////////////////////////////////////////

  mpPickBuffer->context()->FBI()->GetPixel(fvec4(fx, fy, 0.0f), ctx);
}

}} // namespace ork::ent

///////////////////////////////////////////////////////////////////////////////

template <> void ork::lev2::PickBuffer<ork::ent::SceneEditorVP>::Draw(lev2::PixelFetchContext& ctx) {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());

  const ent::Simulation* psi = mpViewport->simulation();
  ent::SceneData* pscene     = mpViewport->mEditor.mpScene;

  if (nullptr == pscene)
    return;
  if (nullptr == psi)
    return;
  if (false == mpViewport->mbSceneDisplayEnable)
    return;

  auto target = mpViewport->GetTarget();
  static CompositingData _gdata;
  static CompositingImpl _gimpl(_gdata);
  static ork::lev2::RenderContextFrameData RCFD(target); //
  RCFD._cimpl = &_gimpl;

  ///////////////////////////////////////////////////////////////////////////

  mPickIds.clear();

  ork::recursive_mutex& glock = lev2::GfxEnv::GetRef().GetGlobalLock();
  glock.Lock(0x777);
  PickFrameTechnique pktek;
  mpViewport->PushFrameTechnique(&pktek);
  Context* pTEXTARG    = context();
  Context* pPARENTTARG = GetParent()->context();
  pTEXTARG->pushRenderContextFrameData(&RCFD);
  SRect tgt_rect(0, 0, mpViewport->GetW(), mpViewport->GetH());
  ///////////////////////////////////////////////////////////////////////////
  mpViewport->GetRenderer()->setContext(pTEXTARG);
  RCFD.SetLightManager(nullptr);
  ///////////////////////////////////////////////////////////////////////////
  // use source viewport's W/H for camera matrix computation
  ///////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////
  rendervar_t passdata;
  passdata.Set<compositingpassdatastack_t*>(&mpViewport->_compositingGroupStack);
  RCFD.setUserProperty("nodes"_crc, passdata);
  lev2::CompositingPassData compositor_node;
  compositor_node.AddLayer("All"_pool);
  compositor_node.SetDstRect(tgt_rect);
  _gimpl.pushCPD(compositor_node);
  ///////////////////////////////////////////////////////////////////////////
  int itx0 = 0;
  int itx1 = itx0 + GetContextW();
  int ity0 = 0;
  int ity1 = ity0 + GetContextH();
  ///////////////////////////////////////////////////////////////////////////
  // force aspect ratio to that of the parent visible viewport
  //  as opposed to the pickbuffer size
  ///////////////////////////////////////////////////////////////////////////
  float fW = mpViewport->GetW();
  float fH = mpViewport->GetH();
  // compositor_node.cameraMatrices()->_aspectRatio = fW / fH;
  ///////////////////////////////////////////////////////////////////////////
  lev2::UiViewportRenderTarget rt(mpViewport);
  compositor_node._irendertarget = &rt;
  BeginFrame();
  {
    SRect VPRect(itx0, ity0, itx1, ity1);
    ///////////////////////////////////////////////////////////////////////////
    pTEXTARG->FBI()->PushRtGroup(mpPickRtGroup); // Enable Mrt
    pTEXTARG->FBI()->EnterPickState(this);
    pTEXTARG->FBI()->PushViewport(VPRect);
    pTEXTARG->BindMaterial(GfxEnv::GetDefault3DMaterial());
    pTEXTARG->PushModColor(fcolor4::Yellow());

    //{ mpViewport->renderEnqueuedScene(*RCFD); }

    pTEXTARG->PopModColor();
    pTEXTARG->FBI()->PopRtGroup();
    pTEXTARG->FBI()->PopViewport();
    pTEXTARG->FBI()->LeavePickState();
  }
  EndFrame();
  _gimpl.popCPD();
  ///////////////////////////////////////////////////////////////////////////
  SetDirty(false);
  pTEXTARG->popRenderContextFrameData();
  mpViewport->PopFrameTechnique();
  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();

  ///////////////////////////////////////////////////////////////////////////
}
