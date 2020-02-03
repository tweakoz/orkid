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

struct Instance {
  XgmModelInst* _xgminst   = nullptr;
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
  std::vector<Instance> instances;
  //////////////////////////////////////////////////////////
  // initialize compositor (necessary for PBR models)
  //  use a deferredPBR compositing node
  //  which does all the gbuffer and lighting passes
  //////////////////////////////////////////////////////////
  DefaultRenderer renderer;
  LightManagerData lmd;
  auto lightmgr              = std::make_shared<LightManager>(lmd);
  auto compscene             = std::make_shared<CompositingScene>();
  auto compsceneitem         = std::make_shared<CompositingSceneItem>();
  auto comptek               = std::make_shared<NodeCompositingTechnique>();
  auto pbrnode               = comptek->createRenderNode<DeferredCompositingNodePbr>();
  auto outnode               = comptek->createOutputNode<ScreenOutputCompositingNode>();
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
  float prevtime = timer.SecsSinceStart();
  //////////////////////////////////////////////////////////
  timer.Start();
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    auto modl_asset = asset::AssetManager<XgmModelAsset>::Load("data://test/pbr1/pbr1");
    model           = modl_asset->GetModel();
    renderer.setContext(ctx);
    auto envl_asset = asset::AssetManager<TextureAsset>::Create("data://environ/envmaps/tozenv_nebula");
    OrkAssert(envl_asset->GetTexture() != nullptr);
    pbrnode->_writeEnvTexture(envl_asset);

    while (asset::AssetManager<TextureAsset>::AutoLoad()) {
    }

    for (int i = 0; i < 10; i++) {
      Instance inst;
      inst._drawable = new ModelDrawable;
      inst._xgminst  = new XgmModelInst(model);
      inst._xgminst->EnableAllMeshes();
      inst._drawable->SetModelInst(inst._xgminst);
      instances.push_back(inst);
    }
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
    float TARGW    = context->mainSurfaceWidth();
    float TARGH    = context->mainSurfaceHeight();
    float aspect   = TARGW / TARGH;
    float phase    = curtime * PI2 * 0.1f;
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

    ////////////////////////////////////////
    // animate instances
    ////////////////////////////////////////

    for (auto& inst : instances) {
      auto drawable = static_cast<Drawable*>(inst._drawable);

      fvec3 delta = inst._target - inst._curpos;
      inst._curpos += delta.Normal() * dt * 0.3;

      delta         = inst._targetaxis - inst._curaxis;
      inst._curaxis = (inst._curaxis + delta.Normal() * dt * 0.1).Normal();
      inst._curangle += (inst._targetangle - inst._curangle) * dt * 0.1;

      if (inst._timeout < curtime) {
        inst._timeout  = curtime + float(rand() % 255) / 64.0;
        inst._target.x = (float(rand() % 255) / 25.0f) - 5.0f;
        inst._target.y = (float(rand() % 255) / 25.0f) - 5.0f;
        inst._target.z = (float(rand() % 255) / 25.0f) - 5.0f;

        fvec3 axis;
        axis.x            = (float(rand() % 255) / 255.0f) - 0.5f;
        axis.y            = (float(rand() % 255) / 255.0f) - 0.5f;
        axis.z            = (float(rand() % 255) / 255.0f) - 0.5f;
        inst._targetaxis  = axis.Normal();
        inst._targetangle = PI2 * (float(rand() % 255) / 255.0f) - 0.5f;
      }

      fquat q;
      q.FromAxisAngle(fvec4(inst._curaxis, inst._curangle));

      inst._xform.mWorldMatrix.compose(inst._curpos, q, 1.0f);
      drawable->enqueueOnLayer(inst._xform, *layer);
    }

    ////////////////////////////////////////

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
