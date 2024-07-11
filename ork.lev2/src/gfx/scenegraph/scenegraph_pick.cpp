#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/application/application.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPicking.h>
using namespace std::string_literals;
using namespace ork;

const int PICKBUFDIM = 127;

namespace ork::lev2::scenegraph {

SgPickBuffer::SgPickBuffer(ork::lev2::Context* ctx, Scene& scene)
    : _context(ctx)
    , _scene(scene) {
  _pick_mvp_matrix           = std::make_shared<fmtx4>();
  if(scene._pickFormat==0){
  _pfc = std::make_shared<PixelFetchContext>(4);
  _pfc->_usage[3]   = lev2::PixelFetchContext::EPU_FVEC4;
  _pfc->_usage[2]   = lev2::PixelFetchContext::EPU_FVEC4;
  _pfc->_usage[1]   = lev2::PixelFetchContext::EPU_FVEC4;
  }
  else{
  _pfc = std::make_shared<PixelFetchContext>(1);
  }
  _pfc->_usage[0]   = lev2::PixelFetchContext::EPU_SVARIANT;
  _pfc->_gfxContext = ctx;
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
    callback(_pfc);
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
    switch(_scene._pickFormat){
      case 0:
        _pickIDtexture = _pfc->_rtgroup->GetMrt(0)->texture();
        _pickPOStexture = _pfc->_rtgroup->GetMrt(1)->texture();
        _pickNRMtexture = _pfc->_rtgroup->GetMrt(2)->texture();
        _pickUVtexture = _pfc->_rtgroup->GetMrt(3)->texture();
        break;
      case 1:
        _pickIDtexture = _pfc->_rtgroup->GetMrt(0)->texture();
        break;
      default:
        OrkAssert(false);
        break;
    }
  }
  _compimpl->_compcontext->Resize(PICKBUFDIM, PICKBUFDIM);
  ///////////////////////////////////////////////////////////////////////////
  auto RCFD = std::make_shared<ork::lev2::RenderContextFrameData>(_context); //
  RCFD->pushCompositor(_compimpl);
  _pfc->mUserData.set<ork::lev2::RenderContextFrameData*>(RCFD.get());
  ///////////////////////////////////////////////////////////////////////////

  ork::recursive_mutex& glock = lev2::GfxEnv::GetRef().GetGlobalLock();
  glock.Lock(0x777);
  _context->pushRenderContextFrameData(RCFD);
  ViewportRect tgt_rect(0, 0, PICKBUFDIM, PICKBUFDIM);
  ///////////////////////////////////////////////////////////////////////////
  // auto irenderer = _scenevp->GetRenderer();
  // irenderer->setContext(_context);
  RCFD->SetLightManager(nullptr);
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

    _pfc->beginPickRender();

    lev2::UiViewportRenderTarget rt(nullptr);
    RCFD->setUserProperty("DB"_crc, lev2::rendervar_t(DB));
    RCFD->setUserProperty("pickbufferMvpMatrix"_crc, _pick_mvp_matrix);
    RCFD->setUserProperty("pixel_fetch_context"_crc, _pfc);
    RCFD->setUserProperty("is_sg_pick"_crcu, true);
    lev2::CompositingPassData CPD;
    CPD._debugName = "scenegraph_pick";
    CPD.AddLayer("All");
    CPD.SetDstRect(tgt_rect);
    CPD._ispicking     = true;
    CPD._irendertarget = &rt;
    ///////////////////////////////////////////////////////////////////////////
    lev2::CompositorDrawData drawdata(RCFD);
    drawdata._cimpl = _compimpl;
    drawdata._properties["StereoEnable"_crcu].set<bool>(false);
    drawdata._properties["primarycamindex"_crcu].set<int>(0);
    drawdata._properties["cullcamindex"_crcu].set<int>(0);
    drawdata._properties["irenderer"_crcu].set<lev2::IRenderer*>(_scene._renderer.get());
    drawdata._properties["simrunning"_crcu].set<bool>(true);
    drawdata._properties["DB"_crcu].set<const DrawQueue*>(DB);
    ///////////////////////////////////////////////////////////////////////////
    // draw the pickbuffer
    ///////////////////////////////////////////////////////////////////////////
    _compimpl->pushCPD(CPD);
    FBI->EnterPickState(nullptr);
    //OrkBreak();
    _compimpl->assemble(drawdata);

    _scene._dbufcontext_SG->releaseFromReadLocked(DB);
    
    FBI->LeavePickState();
    _compimpl->popCPD();
    ///////////////////////////////////////////??
    // fetch the pixel, yo.
    ///////////////////////////////////////////??
    _pfc->endPickRender();
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
