#include <QWindow>
#include <ork/application/application.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>

#include <ork/lev2/gfx/scenegraph/scenegraph.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::deferrednode;

int main(int argc, char** argv) {
  auto qtapp              = OrkEzQtApp::create(argc, argv);
  auto qtwin              = qtapp->_mainWindow;
  auto gfxwin             = qtwin->_gfxwin;
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
  auto lightmgr = std::make_shared<LightManager>(lmd);
  CompositingData compositordata;
  compositordata.presetPBR();
  compositordata.mbEnable = true;
  auto nodetek            = compositordata.tryNodeTechnique<NodeCompositingTechnique>("scene1"_pool, "item1"_pool);
  auto outpnode           = nodetek->tryOutputNodeAs<ScreenOutputCompositingNode>();
  // outpnode->setSuperSample(4);
  CompositingImpl compositorimpl(compositordata);
  compositorimpl.bindLighting(lightmgr.get());
  lev2::CompositingPassData TOPCPD;
  TOPCPD.addStandardLayers();
  CameraDataLut cameras;
  CameraData camdata;
  cameras.AddSorted("spawncam"_pool, &camdata);

  auto sg_graph = std::make_shared<scenegraph::SceneGraph>();
  auto sg_layer = sg_graph->createLayer("default");

  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    // ctx->debugPushGroup("main.onGpuInit");
    auto modl_asset = asset::AssetManager<XgmModelAsset>::Load("data://test/pbr1/pbr1");
    model           = modl_asset->GetModel();
    renderer.setContext(ctx);

    // todo add instanced model to scenegraph layer


    // ctx->debugPopGroup();
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
    float phase    = abstime * PI2 * 0.1f;
    float distance = 10.0f;
    auto eye       = fvec3(sinf(phase), 1.0f, -cosf(phase)) * distance;
    fvec3 tgt(0, 0, 0);
    fvec3 up(0, 1, 0);
    camdata.Lookat(eye, tgt, up);
    camdata.Persp(0.1, 100.0, 45.0);
    ///////////////////////////////////////
    sg_graph->enqueueToRenderer();
    ////////////////////////////////////////
  });
  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](const ui::DrawEvent& drwev) {
    auto DB = DrawableBuffer::acquireReadDB(7); 
    if(nullptr == DB) return;
    auto context = drwev.GetTarget();
    RenderContextFrameData RCFD(context); // renderer per/frame data
    RCFD._cimpl = &compositorimpl;
    RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));
    context->pushRenderContextFrameData(&RCFD);
    auto fbi  = context->FBI();  // FrameBufferInterface
    auto fxi  = context->FXI();  // FX Interface
    auto mtxi = context->MTXI(); // matrix Interface
    auto gbi  = context->GBI();  // GeometryBuffer Interface
    ///////////////////////////////////////
    // compositor setup
    ///////////////////////////////////////
    float TARGW = context->mainSurfaceWidth();
    float TARGH = context->mainSurfaceHeight();
    lev2::UiViewportRenderTarget rt(nullptr);
    auto tgtrect          = ViewportRect(0, 0, TARGW, TARGH);
    TOPCPD._irendertarget = &rt;
    TOPCPD.SetDstRect(tgtrect);
    compositorimpl.pushCPD(TOPCPD);
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
    drawdata._properties["DB"_crcu].Set<const DrawableBuffer*>(DB);
    compositorimpl.assemble(drawdata);
    compositorimpl.composite(drawdata);
    compositorimpl.popCPD();
    context->popRenderContextFrameData();
    context->endFrame();
    DrawableBuffer::releaseReadDB(DB);
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
