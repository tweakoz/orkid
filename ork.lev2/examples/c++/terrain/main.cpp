#include <QWindow>
#include <ork/application/application.h>
#include <ork/kernel/datacache.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/material_freestyle.inl>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::deferrednode;

int main(int argc, char** argv) {

  auto qtapp        = OrkEzQtApp::create(argc, argv);
  auto qtwin        = qtapp->_mainWindow;
  auto gfxwin       = qtwin->_gfxwin;
  Texture* envlight = nullptr;
  hfdrawableinstptr_t _terrainInst;
  TerrainDrawableData _terrainData;
  CallbackDrawable* _terrainDrawable;
  DrawQueueXfData _terrainXform;
  //////////////////////////////////////////////////////////
  // initialize compositor data
  //  use a deferredPBR compositing node
  //  which does all the gbuffer and lighting passes
  //////////////////////////////////////////////////////////
  DefaultRenderer renderer;
  LightManagerData lmd;
  auto lightmgr = std::make_shared<LightManager>(lmd);
  CompositingData compositordata;
  compositordata.presetPBR();
  compositordata.mbEnable     = true;
  auto nodetek                = compositordata.tryNodeTechnique<NodeCompositingTechnique>("scene1"_pool, "item1"_pool);
  auto rendnode               = nodetek->tryRenderNodeAs<deferrednode::DeferredCompositingNodePbr>();
  rendnode->_depthFogDistance = 4000.0f;
  rendnode->_depthFogPower    = 5.0f;
  ///////////////////////////////////////
  // compositor instance
  ///////////////////////////////////////
  CompositingImpl compositorimpl(compositordata);
  compositorimpl.bindLighting(lightmgr.get());
  lev2::CompositingPassData TOPCPD;
  TOPCPD.addStandardLayers();
  CameraDataLut cameras;
  CameraData camdata;
  cameras.AddSorted("spawncam"_pool, &camdata);
  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    ctx->debugPushGroup("main.onGpuInit");
    renderer.setContext(ctx);

    while (asset::AssetManager<TextureAsset>::AutoLoad()) {
    }

    //////////////////////////////////////////////////////////
    _terrainData._rock1 = fvec3(1, 1, 1);
    _terrainData._writeHmapPath("data://terrain/testhmap2_2048.png");
    _terrainInst               = _terrainData.createInstance();
    _terrainInst->_worldHeight = 5000.0f;
    _terrainInst->_worldSizeXZ = 8192.0f;
    _terrainDrawable           = _terrainInst->createCallbackDrawable();
    ctx->debugPopGroup();
  });
  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //////////////////////////////////////////////////////////
  qtapp->onUpdate([&](UpdateData updata) {
    double dt      = updata._dt;
    double abstime = updata._abstime;
    ///////////////////////////////////////
    // compute camera data
    ///////////////////////////////////////
    float phase = abstime * PI2 * 0.01f;
    fvec3 eye(245, 150, -330);
    auto tgt = eye + fvec3(sinf(phase), -0.1f, -cosf(phase));
    fvec3 up(0, 1, 0);
    camdata.Lookat(eye, tgt, up);
    camdata.Persp(0.1, 6000.0, 45.0);
    ///////////////////////////////////////
    // enqueue terrain (and whole frame)
    ///////////////////////////////////////
    auto DB = DrawableBuffer::LockWriteBuffer(0);
    DB->Reset();
    DB->copyCameras(cameras);
    auto layer = DB->MergeLayer("Default"_pool);
    _terrainXform.mWorldMatrix.compose(fvec3(), fquat(), 1.0f);
    _terrainDrawable->enqueueOnLayer(_terrainXform, *layer);
    DrawableBuffer::UnLockWriteBuffer(DB);
  });
  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](const ui::DrawEvent& drwev) {
    auto context = drwev.GetTarget();
    RenderContextFrameData RCFD(context); // renderer per/frame data
    RCFD._cimpl = &compositorimpl;
    context->pushRenderContextFrameData(&RCFD);
    auto fbi = context->FBI(); // FrameBufferInterface
    ///////////////////////////////////////
    // compositor setup
    ///////////////////////////////////////
    lev2::UiViewportRenderTarget rt(nullptr);
    float TARGW           = context->mainSurfaceWidth();
    float TARGH           = context->mainSurfaceHeight();
    auto tgtrect          = ViewportRect(0, 0, TARGW, TARGH);
    TOPCPD._irendertarget = &rt;
    TOPCPD.SetDstRect(tgtrect);
    compositorimpl.pushCPD(TOPCPD);
    ///////////////////////////////////////
    // render !
    ///////////////////////////////////////
    fbi->SetClearColor(fvec4(0, 0, 0, 1));
    fbi->setViewport(tgtrect);
    fbi->setScissor(tgtrect);
    context->beginFrame();
    FrameRenderer framerenderer(RCFD, [&]() {});
    CompositorDrawData drawdata(framerenderer);
    drawdata._properties["primarycamindex"_crcu].Set<int>(0);
    drawdata._properties["cullcamindex"_crcu].Set<int>(0);
    drawdata._properties["irenderer"_crcu].Set<lev2::IRenderer*>(&renderer);
    drawdata._properties["simrunning"_crcu].Set<bool>(true);
    compositorimpl.assemble(drawdata);
    compositorimpl.composite(drawdata);
    compositorimpl.popCPD();
    context->popRenderContextFrameData();
    context->endFrame();
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([&](int w, int h) {
    //
    compositorimpl.compositingContext().Resize(w, h);
  });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return qtapp->exec();
}
