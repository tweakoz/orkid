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
  FreestyleMaterial material;
  const FxShaderTechnique* fxtechnique = nullptr;
  const FxShaderParam* fxparameterMVP  = nullptr;
  Timer timer;
  XgmModel* model         = nullptr;
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
  auto defnode       = std::make_shared<DeferredCompositingNodePbr>();
  auto outnode       = std::make_shared<ScreenOutputCompositingNode>();
  comptek->_writeRenderNode(defnode.get());
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
  //////////////////////////////////////////////////////////
  timer.Start();
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    material.gpuInit(ctx, "orkshader://solid");
    fxtechnique    = material.technique("vtxcolor");
    fxparameterMVP = material.param("MatMVP");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx, fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", fxparameterMVP);
    material._rasterstate.SetCullTest(ECULLTEST_OFF);

    auto modl_asset = asset::AssetManager<XgmModelAsset>::Load("data://test/pbr1/pbr1");
    model           = modl_asset->GetModel();
    modelinst       = new XgmModelInst(model);
    modelinst->EnableAllMeshes();
    renderer.setContext(ctx);
    drawable = new ModelDrawable;
    drawable->SetModelInst(modelinst);
  });
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](const ui::DrawEvent& drwev) {
    auto context = drwev.GetTarget();
    RenderContextFrameData RCFD(context); // renderer per/frame data
    RCFD._cimpl = &compositorimpl;
    auto fbi    = context->FBI();  // FrameBufferInterface
    auto fxi    = context->FXI();  // FX Interface
    auto mtxi   = context->MTXI(); // matrix Interface
    auto gbi    = context->GBI();  // GeometryBuffer Interface
    ///////////////////////////////////////
    // compute view and projection matrices
    ///////////////////////////////////////
    float TARGW  = context->mainSurfaceWidth();
    float TARGH  = context->mainSurfaceHeight();
    float aspect = TARGW / TARGH;
    float phase  = timer.SecsSinceStart() * PI2 * 0.1f;
    fvec3 eye(sinf(phase) * 5.0f, 5.0f, -cosf(phase) * 5.0f);
    fvec3 tgt(0, 0, 0);
    fvec3 up(0, 1, 0);
    auto projection = mtxi->Persp(45, aspect, 0.1, 100.0);
    auto view       = mtxi->LookAt(eye, tgt, up);
    CameraData camdata;
    camdata.Lookat(eye, tgt, up);
    camdata.Persp(0.1, 100.0, 45.0);
    auto cameramatrices = camdata.computeMatrices(aspect);
    ///////////////////////////////////////
    // compositor setup
    ///////////////////////////////////////
    // lev2::UiViewportRenderTarget rt(this);
    const SRect tgtrect = SRect(0, 0, TARGW, TARGH);
    lev2::CompositingPassData TOPCPD;
    TOPCPD.SetDstRect(tgtrect);
    TOPCPD.addStandardLayers();
    // TOPCPD._irendertarget = &rt;
    TOPCPD.SetDstRect(tgtrect);
    TOPCPD.setCameraMatrices(&cameramatrices);
    ///////////////////////////////////////
    auto DB    = DrawableBuffer::LockWriteBuffer(0);
    auto layer = DB->MergeLayer("A"_pool);
    layer->Reset(*DB);
    auto& DBITEM = layer->Queue(drawxf, drawable);
    DrawableBuffer::UnLockWriteBuffer(DB);
    ///////////////////////////////////////
    // Draw!
    ///////////////////////////////////////
    fbi->SetClearColor(fvec4(0, 0, 0, 1));
    context->beginFrame();
    compositorimpl.pushCPD(TOPCPD);
    FrameRenderer framerenderer(RCFD, [&]() {
      //
    });
    CompositorDrawData drawdata(framerenderer);
    drawdata._properties["primarycamindex"_crcu].Set<int>(0);
    drawdata._properties["cullcamindex"_crcu].Set<int>(0);
    drawdata._properties["irenderer"_crcu].Set<lev2::IRenderer*>(&renderer);
    drawdata._properties["simrunning"_crcu].Set<bool>(true);
    compositorimpl.assemble(drawdata);
    compositorimpl.composite(drawdata);
    compositorimpl.popCPD();
    context->endFrame();
  });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return qtapp->exec();
}
