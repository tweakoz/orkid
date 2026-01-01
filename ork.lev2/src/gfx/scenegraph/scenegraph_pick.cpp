#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/application/application.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPicking.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
using namespace std::string_literals;
using namespace ork;

namespace ork::lev2::scenegraph {

// Use the constant from the header
static constexpr int PICKBUFDIM = PICKBUFFER_DIM;

SgPickBuffer::SgPickBuffer(ork::lev2::Context* ctx, Scene& scene)
    : _context(ctx)
    , _scene(scene) {
  _pick_mvp_matrix           = std::make_shared<fmtx4>();
  if(scene._pickFormat==0){
  _pfc = std::make_shared<PixelFetchContext>(4);
  _pfc->_usage[3]   = lev2::PixelFetchContext::EPixelUsage::FVEC4;
  _pfc->_usage[2]   = lev2::PixelFetchContext::EPixelUsage::FVEC4;
  _pfc->_usage[1]   = lev2::PixelFetchContext::EPixelUsage::FVEC4;
  }
  else{
  _pfc = std::make_shared<PixelFetchContext>(1);
  }
  _pfc->_usage[0]   = lev2::PixelFetchContext::EPixelUsage::SVARIANT;
  _pfc->_gfxContext = ctx;
  gpuInit(ctx);
}
///////////////////////////////////////////////////////////////////////////
void SgPickBuffer::gpuInit(ork::lev2::Context* ctx) {
  if (_compdata != nullptr) {
    return; // Already initialized
  }

  _compdata = new CompositingData;
  _compdata->presetPicking();

  auto csi     = _compdata->findScene("scene1");
  auto itm     = csi->findItem("item1");
  auto tek     = itm->tryTechniqueAs<NodeCompositingTechnique>();
  auto piknode = tek->tryRenderNodeAs<PickingCompositingNode>();
  auto rtgnode = tek->tryOutputNodeAs<RtGroupOutputCompositingNode>();
  piknode->resize(PICKBUFDIM, PICKBUFDIM);
  rtgnode->resize(PICKBUFDIM, PICKBUFDIM);
  piknode->gpuInit(ctx, PICKBUFDIM, PICKBUFDIM);
  _pfc->_rtgroup = piknode->GetOutputGroup();
  _compimpl = _compdata->createImpl();

  switch(_scene._pickFormat){
    case 0:
      _pickIDtexture = _pfc->_rtgroup->texture(0);
      _pickPOStexture = _pfc->_rtgroup->texture(1);
      _pickNRMtexture = _pfc->_rtgroup->texture(2);
      _pickUVtexture = _pfc->_rtgroup->texture(3);
      printf("SgPickBuffer::gpuInit pickFormat=0\n");
      printf("  _pickIDtexture: %p w=%d h=%d\n",
             _pickIDtexture.get(),
             _pickIDtexture ? _pickIDtexture->_width : -1,
             _pickIDtexture ? _pickIDtexture->_height : -1);
      printf("  _pickPOStexture: %p w=%d h=%d\n",
             _pickPOStexture.get(),
             _pickPOStexture ? _pickPOStexture->_width : -1,
             _pickPOStexture ? _pickPOStexture->_height : -1);
      printf("  _pickNRMtexture: %p w=%d h=%d\n",
             _pickNRMtexture.get(),
             _pickNRMtexture ? _pickNRMtexture->_width : -1,
             _pickNRMtexture ? _pickNRMtexture->_height : -1);
      printf("  _pickUVtexture: %p w=%d h=%d\n",
             _pickUVtexture.get(),
             _pickUVtexture ? _pickUVtexture->_height : -1,
             _pickUVtexture ? _pickUVtexture->_height : -1);
      break;
    case 1:
      _pickIDtexture = _pfc->_rtgroup->texture(0);
      printf("SgPickBuffer::gpuInit pickFormat=1\n");
      printf("  _pickIDtexture: %p w=%d h=%d\n",
             _pickIDtexture.get(),
             _pickIDtexture ? _pickIDtexture->_width : -1,
             _pickIDtexture ? _pickIDtexture->_height : -1);
      break;
    default:
      OrkAssert(false);
      break;
  }
}
///////////////////////////////////////////////////////////////////////////
void SgPickBuffer::pickWithScreenCoord(cameradata_ptr_t cam, fvec2 screencoord, callback_t callback) {
  auto FBI = _context->FBI();
  int W    = _context->mainSurfaceWidth();
  int H    = _context->mainSurfaceHeight();
  float fx = float(screencoord.x) / W;
  float fy = float(screencoord.y) / H;
  // Flip Y for Vulkan coordinate system: screen Y=0 is top, but frustum Y=1 is top
  // (due to Y-flip in projection matrix for Vulkan NDC)
  fvec2 unitpos(fx, 1.0f - fy);
  auto mtcs = cam->computeMatrices(float(W) / float(H));
  auto ray  = std::make_shared<fray3>();
  mtcs.projectDepthRay(unitpos, *ray.get());
  auto o = ray->mOrigin;
  auto d = ray->mDirection;
  pickWithRay(ray,callback);
}
///////////////////////////////////////////////////////////////////////////
void SgPickBuffer::pickWithRay(fray3_constptr_t ray, callback_t callback) {
    mydraw(ray, callback);
}
///////////////////////////////////////////////////////////////////////////
void SgPickBuffer::mydraw(fray3_constptr_t ray, callback_t callback) {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  _context->makeCurrentContext();
  auto FBI = _context->FBI();
  ///////////////////////////////////////////////////////////////////////////
  gpuInit(_context);  // Ensure initialized (no-op if already done)
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
    drawdata._properties["SinglePassStereo"_crcu].set<bool>(false);
    drawdata._properties["primarycamindex"_crcu].set<int>(0);
    drawdata._properties["cullcamindex"_crcu].set<int>(0);
    drawdata._properties["irenderer"_crcu].set<lev2::IRenderer*>(_scene._currentRenderer().get());
    drawdata._properties["simrunning"_crcu].set<bool>(true);
    drawdata._properties["DB"_crcu].set<const DrawQueue*>(DB);
    ///////////////////////////////////////////////////////////////////////////
    // FRAME 1: Render the pick buffer
    // Must wrap rendering in beginFrame/endFrame to ensure valid command buffer
    ///////////////////////////////////////////////////////////////////////////
    _context->beginFrame(false);  // non-visual frame for pick rendering
    _compimpl->pushCPD(CPD);
    FBI->EnterPickState(nullptr);
    _compimpl->assemble(drawdata);

    _scene._dbufcontext_SG->releaseFromReadLocked(DB);

    FBI->LeavePickState();
    _compimpl->popCPD();
    _context->endFrame();
    ///////////////////////////////////////////////////////////////////////////
    // FRAME 2: Capture the pixel
    // Separate frame for the async capture operation
    ///////////////////////////////////////////////////////////////////////////
    _pfc->endPickRender();

    // Capture center pixel asynchronously
    int center_x = PICKBUFDIM / 2;
    int center_y = PICKBUFDIM / 2;

    // Create completion callback that invokes user callback
    auto pfc = _pfc;
    auto on_complete = [callback, pfc]() {
      // Invoke user callback with the pixel fetch context
      // This will be called when GPU readback completes
      if (callback) {
        callback(pfc);
      }
    };

    _context->beginFrame(false);  // non-visual frame for capture
    _pendingCapture = FBI->capturePixelAsync(_pfc, center_x, center_y, on_complete);
    _context->endFrame();
    ///////////////////////////////////////////////////////////////////////////

  } // if(DB)
  ///////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////
  _context->popRenderContextFrameData();
  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
  ///////////////////////////////////////////////////////////////////////////
}
} // namespace ork::lev2::scenegraph
