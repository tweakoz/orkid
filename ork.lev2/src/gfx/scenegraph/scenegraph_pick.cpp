#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/application/application.h>
#include <ork/kernel/opq.h>
using namespace std::string_literals;
using namespace ork;

namespace ork::lev2::scenegraph {

PickBuffer::PickBuffer(ork::lev2::Context* ctx, Scene& scene)
    : lev2::PickBuffer(nullptr, ctx, 0, 0)
    , _scene(scene) {

  _rtgroup->Resize(3, 3);
}
///////////////////////////////////////////////////////////////////////////
uint64_t PickBuffer::pickWithRay(fray3_constptr_t ray) {

  lev2::PixelFetchContext pfc;
  pfc._gfxContext = _context;
  pfc.mRtGroup    = _rtgroup;
  pfc.miMrtMask   = (1 << 0); //| (1 << 1); // ObjectID and ObjectUVD
  pfc.mUsage[0]   = lev2::PixelFetchContext::EPU_PTR64;
  Draw(pfc);
  auto p = pfc.GetPointer(0);
  return uint64_t(p);
}
///////////////////////////////////////////////////////////////////////////
void PickBuffer::Draw(lev2::PixelFetchContext& pfc) {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  auto target = pfc._gfxContext;
  target->makeCurrentContext();
  ///////////////////////////////////////////////////////////////////////////
  if (nullptr == _compdata) {
    _compdata = new CompositingData;
    _compdata->presetPicking();
    _compimpl = _compdata->createImpl();
  }
  ///////////////////////////////////////////////////////////////////////////
  ork::lev2::RenderContextFrameData RCFD(target); //
  RCFD._cimpl = _compimpl;
  ///////////////////////////////////////////////////////////////////////////

  // mPickIds.clear();

  ork::recursive_mutex& glock = lev2::GfxEnv::GetRef().GetGlobalLock();
  glock.Lock(0x777);
  target->pushRenderContextFrameData(&RCFD);
  ViewportRect tgt_rect(0, 0, 3, 3);
  ///////////////////////////////////////////////////////////////////////////
  // auto irenderer = _scenevp->GetRenderer();
  // irenderer->setContext(target);
  RCFD.SetLightManager(nullptr);
  ///////////////////////////////////////////////////////////////////////////
  auto DB = DrawableBuffer::acquireReadDB(0x1234);
  if (DB) {
    lev2::UiViewportRenderTarget rt(nullptr);
    // lev2::UiViewportRenderTarget rt(_scenevp);
    // rendervar_t passdata;
    // passdata.Set<compositingpassdatastack_t*>(&_scenevp->_compositingGroupStack);
    // RCFD.setUserProperty("nodes"_crc, passdata);
    RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));
    lev2::CompositingPassData CPD;
    CPD.AddLayer("All"_pool);
    CPD.SetDstRect(tgt_rect);
    CPD._ispicking     = true;
    CPD._irendertarget = &rt;
    _compimpl->pushCPD(CPD);
    ///////////////////////////////////////////////////////////////////////////
    lev2::FrameRenderer framerenderer(RCFD, [&]() {});
    lev2::CompositorDrawData drawdata(framerenderer);
    drawdata._cimpl = _compimpl;
    drawdata._properties["OutputWidth"_crcu].Set<int>(3);
    drawdata._properties["OutputHeight"_crcu].Set<int>(3);
    drawdata._properties["StereoEnable"_crcu].Set<bool>(false);
    drawdata._properties["primarycamindex"_crcu].Set<int>(0);
    drawdata._properties["cullcamindex"_crcu].Set<int>(0);
    drawdata._properties["irenderer"_crcu].Set<lev2::IRenderer*>(&_scene._renderer);
    drawdata._properties["simrunning"_crcu].Set<bool>(true);
    drawdata._properties["DB"_crcu].Set<const DrawableBuffer*>(DB);
    ///////////////////////////////////////////////////////////////////////////
    auto FBI    = target->FBI();
    auto vprect = _rtgroup->viewportRect();
    ///////////////////////////////////////////////////////////////////////////
    FBI->PushRtGroup(_rtgroup); // Enable Mrt
    FBI->EnterPickState(this);
    _context->PushModColor(fcolor4::Yellow());
    drawdata._cimpl   = _compimpl;
    bool assembled_ok = _compimpl->assemble(drawdata);
    DrawableBuffer::releaseReadDB(DB);
    ///////////////////////////////////////////??
    if (assembled_ok)
      _compimpl->composite(drawdata);
    ///////////////////////////////////////////??
    _context->PopModColor();
    FBI->PopRtGroup();
    FBI->LeavePickState();
    _compimpl->popCPD();
  } // if(DB)
  ///////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////
  target->popRenderContextFrameData();
  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
  ///////////////////////////////////////////////////////////////////////////}
}
} // namespace ork::lev2::scenegraph
