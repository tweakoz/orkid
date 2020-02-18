#include <QWindow>
#include <ork/application/application.h>
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
typedef SVtxV12C4T16 vtx_t; // position, vertex color, 2 UV sets

int main(int argc, char** argv) {
  auto qtapp  = OrkEzQtApp::create(argc, argv);
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;
  Timer timer;
  Texture* envlight = nullptr;
  hfdrawableinstptr_t _terrainInst;
  TerrainDrawableData _terrainData;
  CallbackDrawable* _terrainDrawable;
  DrawQueueXfData _terrainXform;
  //////////////////////////////////////////////////////////
  // initialize compositor (necessary for PBR models)
  //  use a deferredPBR compositing node
  //  which does all the gbuffer and lighting passes
  //////////////////////////////////////////////////////////
  DefaultRenderer renderer;
  LightManagerData lmd;
  auto lightmgr = std::make_shared<LightManager>(lmd);
  CompositingData compositordata;
  compositordata.presetPBR();
  compositordata.mbEnable     = true;
  auto nodetek                = compositordata.tryNodeTecnique<NodeCompositingTechnique>("scene1"_pool, "item1"_pool);
  auto rendnode               = nodetek->tryRenderNodeAs<deferrednode::DeferredCompositingNodePbr>();
  rendnode->_depthFogDistance = 4000.0f;
  rendnode->_depthFogPower    = 5.0f;

  CompositingImpl compositorimpl(compositordata);
  compositorimpl.bindLighting(lightmgr.get());
  lev2::CompositingPassData TOPCPD;
  TOPCPD.addStandardLayers();
  CameraDataLut cameras;
  CameraData camdata;
  cameras.AddSorted("spawncam"_pool, &camdata);
  float prevtime = timer.SecsSinceStart();
  //////////////////////////////////////////////////////////
  timer.Start();
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
  qtapp->onDraw([&](const ui::DrawEvent& drwev) {
    auto context = drwev.GetTarget();
    RenderContextFrameData RCFD(context); // renderer per/frame data
    RCFD._cimpl = &compositorimpl;
    context->pushRenderContextFrameData(&RCFD);
    auto fbi      = context->FBI();  // FrameBufferInterface
    auto fxi      = context->FXI();  // FX Interface
    auto mtxi     = context->MTXI(); // matrix Interface
    auto gbi      = context->GBI();  // GeometryBuffer Interface
    float curtime = timer.SecsSinceStart();
    float dt      = curtime - prevtime;
    prevtime      = curtime;
    ///////////////////////////////////////
    // compute view and projection matrices
    ///////////////////////////////////////
    float TARGW  = context->mainSurfaceWidth();
    float TARGH  = context->mainSurfaceHeight();
    float aspect = TARGW / TARGH;
    float phase  = curtime * PI2 * 0.1f;
    fvec3 eye(245, 150, -330);
    auto tgt = eye + fvec3(sinf(phase), -0.2f, -cosf(phase));
    fvec3 up(0, 1, 0);
    camdata.Lookat(eye, tgt, up);
    camdata.Persp(0.1, 6000.0, 45.0);
    ///////////////////////////////////////
    // compositor setup
    ///////////////////////////////////////
    lev2::UiViewportRenderTarget rt(nullptr);
    auto tgtrect          = ViewportRect(0, 0, TARGW, TARGH);
    TOPCPD._irendertarget = &rt;
    TOPCPD.SetDstRect(tgtrect);
    compositorimpl.pushCPD(TOPCPD);
    ///////////////////////////////////////
    auto DB = DrawableBuffer::LockWriteBuffer(0);
    DB->Reset();
    DB->copyCameras(cameras);
    auto layer = DB->MergeLayer("Default"_pool);

    ////////////////////////////////////////

    _terrainXform.mWorldMatrix.compose(fvec3(), fquat(), 1.0f);
    _terrainDrawable->enqueueOnLayer(_terrainXform, *layer);

    ////////////////////////////////////////

    DrawableBuffer::UnLockWriteBuffer(DB);
    ///////////////////////////////////////
    // Draw!
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
