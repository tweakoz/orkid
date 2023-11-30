#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/application/application.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPicking.h>
using namespace std::string_literals;
using namespace ork;

const int PICKBUFDIM = 255;

namespace ork::lev2::scenegraph {

SgPickBuffer::SgPickBuffer(ork::lev2::Context* ctx, Scene& scene)
    : _context(ctx)
    , _scene(scene) {
  _pick_mvp_matrix           = std::make_shared<fmtx4>();
  _pfc = std::make_shared<PixelFetchContext>();
  _pfc->_gfxContext = ctx;
  _pfc->miMrtMask   = 1;
  _pfc->mUsage[0]   = lev2::PixelFetchContext::EPU_SVARIANT;
  //_pfc->mUsage[1]   = lev2::PixelFetchContext::EPU_FLOAT;
}
///////////////////////////////////////////////////////////////////////////
void SgPickBuffer::pickWithScreenCoord(cameradata_ptr_t cam, fvec2 screencoord, callback_t callback) {
  auto FBI = _context->FBI();
  int W    = _context->mainSurfaceWidth();
  int H    = _context->mainSurfaceHeight();
  float fx = float(screencoord.x) / W;
  float fy = float(screencoord.y) / H;
  fvec2 unitpos(fx, fy);
  auto mtcs = cam->computeMatrices(float(W) / float(H));
  auto ray  = std::make_shared<fray3>();
  mtcs.projectDepthRay(unitpos, *ray.get());
  auto o = ray->mOrigin;
  auto d = ray->mDirection;
  pickWithRay(ray,callback);
}
///////////////////////////////////////////////////////////////////////////
void SgPickBuffer::pickWithRay(fray3_constptr_t ray, callback_t callback) {
    mydraw(ray);
    callback(_pfc->_pickvalues[0]);
}
///////////////////////////////////////////////////////////////////////////
void SgPickBuffer::mydraw(fray3_constptr_t ray) {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  _context->makeCurrentContext();
  auto FBI = _context->FBI();
  ///////////////////////////////////////////////////////////////////////////
  if (nullptr == _compdata) {
    _compdata = new CompositingData;
    _compdata->presetPicking();

    auto csi     = _compdata->findScene("scene1");
    auto itm     = csi->findItem("item1");
    auto tek     = itm->tryTechniqueAs<NodeCompositingTechnique>();
    auto piknode = tek->tryRenderNodeAs<PickingCompositingNode>();
    auto rtgnode = tek->tryOutputNodeAs<RtGroupOutputCompositingNode>();
    piknode->resize(PICKBUFDIM, PICKBUFDIM);
    rtgnode->resize(PICKBUFDIM, PICKBUFDIM);
    piknode->gpuInit(_context, PICKBUFDIM, PICKBUFDIM);
    _pfc->_rtgroup = piknode->GetOutputGroup();
    _compimpl               = _compdata->createImpl();
    _picktexture = _pfc->_rtgroup->GetMrt(0)->texture();
  }
  _compimpl->_compcontext->Resize(PICKBUFDIM, PICKBUFDIM);
  ///////////////////////////////////////////////////////////////////////////
  ork::lev2::RenderContextFrameData RCFD(_context); //
  RCFD.pushCompositor(_compimpl);
  _pfc->mUserData.set<ork::lev2::RenderContextFrameData*>(&RCFD);
  ///////////////////////////////////////////////////////////////////////////

  ork::recursive_mutex& glock = lev2::GfxEnv::GetRef().GetGlobalLock();
  glock.Lock(0x777);
  _context->pushRenderContextFrameData(&RCFD);
  ViewportRect tgt_rect(0, 0, PICKBUFDIM, PICKBUFDIM);
  ///////////////////////////////////////////////////////////////////////////
  // auto irenderer = _scenevp->GetRenderer();
  // irenderer->setContext(_context);
  RCFD.SetLightManager(nullptr);
  ///////////////////////////////////////////////////////////////////////////
  auto DB = _scene._dbufcontext_SG->acquireForReadLocked();
  if (DB) {

    /////////////////////////////////////////////////////////////
    // since we have a pick ray
    //  just point the camera along the ray
    //  and the pixel of interest will be at the center
    //  of the rendered buffer
    /////////////////////////////////////////////////////////////
    _camdat.Persp(0.01, 1000, 0.1);
    auto up = ray->mDirection.crossWith(fvec3(0, 1, 0));
    up      = ray->mDirection.crossWith(up);
    _camdat.Lookat(
      ray->mOrigin, //
      ray->mOrigin + ray->mDirection,
      up);

    auto mtcs                 = _camdat.computeMatrices(1.0);
    fmtx4 P                   = mtcs.GetPMatrix();
    fmtx4 V                   = mtcs.GetVMatrix();
    (*_pick_mvp_matrix.get()) = P*V;
    auto screen_coordinate    = fvec4(0.5, 0.5, 0, 0);
    /////////////////////////////////////////////////////////////

    _pfc->_pickIDlut.clear();
    _pfc->_pickIDvec.clear();

    lev2::UiViewportRenderTarget rt(nullptr);
    RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));
    RCFD.setUserProperty("pickbufferMvpMatrix"_crc, _pick_mvp_matrix);
    RCFD.setUserProperty("pixel_fetch_context"_crc, _pfc);
    lev2::CompositingPassData CPD;
    CPD.AddLayer("All");
    CPD.SetDstRect(tgt_rect);
    CPD._ispicking     = true;
    CPD._irendertarget = &rt;
    ///////////////////////////////////////////////////////////////////////////
    lev2::FrameRenderer framerenderer(RCFD, [&]() {});
    lev2::CompositorDrawData drawdata(framerenderer);
    drawdata._cimpl = _compimpl;
    drawdata._properties["StereoEnable"_crcu].set<bool>(false);
    drawdata._properties["primarycamindex"_crcu].set<int>(0);
    drawdata._properties["cullcamindex"_crcu].set<int>(0);
    drawdata._properties["irenderer"_crcu].set<lev2::IRenderer*>(_scene._renderer.get());
    drawdata._properties["simrunning"_crcu].set<bool>(true);
    drawdata._properties["DB"_crcu].set<const DrawableBuffer*>(DB);
    ///////////////////////////////////////////////////////////////////////////
    // draw the pickbuffer
    ///////////////////////////////////////////////////////////////////////////
    _compimpl->pushCPD(CPD);
    FBI->EnterPickState(nullptr);
    _compimpl->assemble(drawdata);

    _scene._dbufcontext_SG->releaseFromReadLocked(DB);
    
    FBI->LeavePickState();
    _compimpl->popCPD();
    ///////////////////////////////////////////??
    // fetch the pixel, yo.
    ///////////////////////////////////////////??
    FBI->GetPixel(screen_coordinate, *_pfc);
    ///////////////////////////////////////////??

  } // if(DB)
  ///////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////
  _context->popRenderContextFrameData();
  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
  ///////////////////////////////////////////////////////////////////////////}
}
} // namespace ork::lev2::scenegraph
