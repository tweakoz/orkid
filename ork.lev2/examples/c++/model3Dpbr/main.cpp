#include <QWindow>
#include <ork/application/application.h>
#include <ork/kernel/environment.h>
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
#include <ork/lev2/gfx/material_freestyle.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::deferrednode;
typedef SVtxV12C4T16 vtx_t; // position, vertex color, 2 UV sets

struct Instance {
  xgmmodelinst_ptr_t _xgminst;
  ModelDrawable* _drawable = nullptr;
  DrawQueueXfData _xform;
  fvec3 _curpos;
  fvec3 _curaxis;
  float _curangle = 0.0f;
  fvec3 _target;
  fvec3 _targetaxis;
  float _targetangle = 0.0f;
  float _timeout     = 0.0f;
};

int main(int argc, char** argv, char** argp) {
  genviron.init_from_envp(argp);
  auto qtapp              = OrkEzQtApp::create(argc, argv);
  auto qtwin              = qtapp->_mainWindow;
  auto gfxwin             = qtwin->_gfxwin;
  XgmModel* model         = nullptr;
  Texture* envlight       = nullptr;
  XgmModelInst* modelinst = nullptr;
  ModelDrawable* drawable = nullptr;
  DrawQueueXfData drawxf;
  std::vector<Instance> instances;
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
  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    // ctx->debugPushGroup("main.onGpuInit");
    auto modl_asset = asset::AssetManager<XgmModelAsset>::Load("srcdata://tests/pbr1/pbr1");
    model           = modl_asset->GetModel();
    renderer.setContext(ctx);

    for (int i = 0; i < 30; i++) {
      Instance inst;
      inst._drawable = new ModelDrawable;
      inst._xgminst  = std::make_shared<XgmModelInst>(model);
      inst._xgminst->EnableAllMeshes();
      inst._drawable->SetModelInst(inst._xgminst);
      instances.push_back(inst);
    }
    // ctx->debugPopGroup();
  });
  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //////////////////////////////////////////////////////////
  qtapp->onUpdate([&](updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime;
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
    auto DB = DrawableBuffer::LockWriteBuffer(0);
    DB->Reset();
    DB->copyCameras(cameras);
    auto layer = DB->MergeLayer("Default"_pool);
    ////////////////////////////////////////
    // animate and enqueue all instances
    ////////////////////////////////////////
    for (auto& inst : instances) {
      auto drawable = static_cast<Drawable*>(inst._drawable);
      fvec3 delta   = inst._target - inst._curpos;
      inst._curpos += delta.Normal() * dt * 1.0;

      delta         = inst._targetaxis - inst._curaxis;
      inst._curaxis = (inst._curaxis + delta.Normal() * dt * 0.1).Normal();
      inst._curangle += (inst._targetangle - inst._curangle) * dt * 0.1;

      if (inst._timeout < abstime) {
        inst._timeout  = abstime + float(rand() % 255) / 64.0;
        inst._target.x = (float(rand() % 255) / 2.55) - 50;
        inst._target.y = (float(rand() % 255) / 2.55) - 50;
        inst._target.z = (float(rand() % 255) / 2.55) - 50;
        inst._target *= 10.0f;

        fvec3 axis;
        axis.x            = (float(rand() % 255) / 255.0f) - 0.5f;
        axis.y            = (float(rand() % 255) / 255.0f) - 0.5f;
        axis.z            = (float(rand() % 255) / 255.0f) - 0.5f;
        inst._targetaxis  = axis.Normal();
        inst._targetangle = PI2 * (float(rand() % 255) / 255.0f) - 0.5f;
      }

      fquat q;
      q.fromAxisAngle(fvec4(inst._curaxis, inst._curangle));

      inst._xform._worldMatrix->compose(inst._curpos, q, 1.0f);
      drawable->enqueueOnLayer(inst._xform, *layer);
    }
    ////////////////////////////////////////
    DrawableBuffer::UnLockWriteBuffer(DB);
  });
  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](const ui::DrawEvent& drwev) {
    auto DB = DrawableBuffer::acquireReadDB(7);
    if (nullptr == DB)
      return;
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
