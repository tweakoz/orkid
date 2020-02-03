#include <QWindow>
#include <ork/application/application.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
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
  XgmModel* model         = nullptr;
  Texture* envlight       = nullptr;
  XgmModelInst* modelinst = nullptr;
  ModelDrawable* drawable = nullptr;
  DrawQueueXfData drawxf;
  //////////////////////////////////////////////////////////
  // initialize compositor (necessary for PBR models)
  //  use a deferredPBR compositing node
  //  which does all the gbuffer and lighting passes
  //////////////////////////////////////////////////////////
  DefaultRenderer renderer;
  LightManagerData lmd;
  auto lightmgr      = std::make_shared<LightManager>(lmd);
  auto compscene     = std::make_shared<CompositingScene>();
  auto compsceneitem = std::make_shared<CompositingSceneItem>();
  auto comptek       = std::make_shared<NodeCompositingTechnique>();
  auto pbrnode       = std::make_shared<DeferredCompositingNodePbr>();
  auto outnode       = std::make_shared<ScreenOutputCompositingNode>();
  comptek->_writeRenderNode(pbrnode.get());
  comptek->_writeOutputNode(outnode.get());
  compsceneitem->mpTechnique = comptek.get();
  compscene->items().AddSorted("item1"_pool, compsceneitem.get());
  CompositingData compositordata;
  compositordata.scenes().AddSorted("scene1"_pool, compscene.get());
  compositordata._activeScene = "scene1"_pool;
  compositordata._activeItem  = "item1"_pool;
  compositordata.mbEnable     = true;
  CompositingImpl compositorimpl(compositordata);
  compositorimpl.bindLighting(lightmgr.get());
  lev2::CompositingPassData TOPCPD;
  TOPCPD.addStandardLayers();
  CameraDataLut cameras;
  CameraData camdata;
  cameras.AddSorted("spawncam"_pool, &camdata);
  //////////////////////////////////////////////////////////
  timer.Start();
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    auto modl_asset = asset::AssetManager<XgmModelAsset>::Load("data://test/pbr1/pbr1");
    model           = modl_asset->GetModel();
    modelinst       = new XgmModelInst(model);
    modelinst->EnableAllMeshes();
    renderer.setContext(ctx);
    drawable = new ModelDrawable;
    drawable->SetModelInst(modelinst);
    auto envl_asset = asset::AssetManager<TextureAsset>::Create("data://environ/envmaps/tozenv_nebula");
    OrkAssert(envl_asset->GetTexture() != nullptr);
    pbrnode->_writeEnvTexture(envl_asset);

    while (asset::AssetManager<TextureAsset>::AutoLoad()) {
    }
  });
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](const ui::DrawEvent& drwev) {
    auto context = drwev.GetTarget();
    RenderContextFrameData RCFD(context); // renderer per/frame data
    RCFD._cimpl = &compositorimpl;
    context->pushRenderContextFrameData(&RCFD);
    auto fbi  = context->FBI();  // FrameBufferInterface
    auto fxi  = context->FXI();  // FX Interface
    auto mtxi = context->MTXI(); // matrix Interface
    auto gbi  = context->GBI();  // GeometryBuffer Interface
    ///////////////////////////////////////
    // compute view and projection matrices
    ///////////////////////////////////////
    float TARGW    = context->mainSurfaceWidth();
    float TARGH    = context->mainSurfaceHeight();
    float aspect   = TARGW / TARGH;
    float phase    = timer.SecsSinceStart() * PI2 * 0.1f;
    float distance = 10.0f;
    auto eye       = fvec3(sinf(phase), 1.0f, -cosf(phase)) * distance;
    fvec3 tgt(0, 0, 0);
    fvec3 up(0, 1, 0);
    auto projection = mtxi->Persp(45, aspect, 0.1, 100.0);
    auto view       = mtxi->LookAt(eye, tgt, up);
    camdata.Lookat(eye, tgt, up);
    camdata.Persp(0.1, 100.0, 45.0);
    ///////////////////////////////////////
    // compositor setup
    ///////////////////////////////////////
    lev2::UiViewportRenderTarget rt(nullptr);
    const SRect tgtrect   = SRect(0, 0, TARGW, TARGH);
    TOPCPD._irendertarget = &rt;
    TOPCPD.SetDstRect(tgtrect);
    compositorimpl.pushCPD(TOPCPD);
    ///////////////////////////////////////
    auto DB = DrawableBuffer::LockWriteBuffer(0);
    DB->Reset();
    DB->copyCameras(cameras);
    auto layer = DB->MergeLayer("Default"_pool);
    ((Drawable*)drawable)->enqueueOnLayer(drawxf, *layer);
    DrawableBuffer::UnLockWriteBuffer(DB);
    ///////////////////////////////////////
    // Draw!
    ///////////////////////////////////////
    fbi->SetClearColor(fvec4(0, 0, 0, 1));
    fbi->SetViewport(0, 0, TARGW, TARGH);
    fbi->SetScissor(0, 0, TARGW, TARGH);
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
