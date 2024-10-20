////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/string/deco.inl>
#include <orktool/qtui/qtui_tool.h>

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
//
#include <ork/kernel/timer.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>

#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/reflect/properties/register.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>

#include <orktool/toolcore/dataflow.h>

///////////////////////////////////////////////////////////////////////////////

#include <pkg/ent/CompositingSystem.h>
#include <pkg/ent/editor/edmainwin.h>

#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/kernel/future.hpp>
#include <ork/lev2/lev2_asset.h>

#include <pkg/ent/LightingSystem.h>
#include <pkg/ent/scene.hpp>

#include "uiToolsDefault.h"
#include "vpRenderer.h"
#include "vpSceneEditor.h"
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/profiling.inl>
#include <ork/lev2/ui/context.h>

#define GL_ERRORCHECK()                                                                                                            \
  {                                                                                                                                \
    int iErr = GetGlError();                                                                                                       \
    OrkAssert(iErr == 0);                                                                                                          \
  }

///////////////////////////////////////////////////////////////////////////////

extern bool gtoggle_hud;
extern bool gtoggle_pickbuffer;

using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneEditorView, "SceneEditorView");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneEditorVP, "SceneEditorVP");

namespace ork {
namespace lev2 {
int GetGlError(void);

}
void SetCurrentThreadName(const char* pname);
namespace ent {

///////////////////////////////////////////////////////////////////////////////

void SceneEditorView::Describe() {
  reflect::RegisterFunctor("SlotModelDirty", &SceneEditorView::SlotModelDirty);
}

///////////////////////////////////////////////////////////////////////////////

orkset<SceneEditorInitCb> SceneEditorVP::mInitCallbacks;

///////////////////////////////////////////////////////////////////////////////

UpdateThread* gupdatethread = 0;

void SceneEditorVP::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DisableSceneDisplay() {
  ork::opq::assertOnQueue2(ork::opq::mainSerialQueue());
  mbSceneDisplayEnable = false;
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::EnableSceneDisplay() {
  ork::opq::assertOnQueue2(ork::opq::mainSerialQueue());
  mbSceneDisplayEnable = true;
}

SceneEditorVP::SceneEditorVP(const std::string& name, SceneEditorBase& the_ed, EditorMainWindow& MainWin)
    : ui::Viewport(name, 1, 1, 1, 1, fcolor3(0.0f, 0.0f, 0.0f), 1.0f)
    , mMainWindow(MainWin)
    , miPickDirtyCount(0)
    , mEditor(the_ed)
    , mpBasicFrameTek(0)
    , mpCurrentHandler(0)
    , mGridMode(0)
    , _renderer(new ork::tool::Renderer(the_ed))
    , mSceneView(this)
    , _editorCamera(0)
    , miCullCameraIndex(-1)
    , miCameraIndex(0)
    , mCompositorSceneIndex(0)
    , mCompositorSceneItemIndex(0)
    , mbSceneDisplayEnable(false)
    , _updateThread(nullptr) {
  mRenderLock = 0;

#if defined(__APPLE__) // install 3 button emulator
  pushEventFilter<ui::Apple3ButtonMouseEmulationFilter>();
#endif

  _overlayCamMatrices = std::make_shared<ork::lev2::CameraMatrices>();

  ///////////////////////////////////////////////////////////

  _simchannelsubscriber = msgrouter::channel("Simulation")->subscribe([=](msgrouter::content_t c) {
    if (auto as_sei = c.TryAs<ork::ent::SimulationEvent>()) {
      auto& sei = as_sei.value();
      switch (sei.GetEvent()) {
        case ork::ent::SimulationEvent::ESIEV_DISABLE_UPDATE: {
          auto lamb = [=]() { gUpdateStatus.SetState(EUPD_STOP); };
          opq::Op(lamb).QueueASync(opq::updateSerialQueue());
          break;
        }
        case ork::ent::SimulationEvent::ESIEV_ENABLE_UPDATE: {
          auto lamb = [=]() { gUpdateStatus.SetState(EUPD_START); };
          opq::Op(lamb).QueueASync(opq::updateSerialQueue());
          break;
        }
        case ork::ent::SimulationEvent::ESIEV_DISABLE_VIEW: {
          auto lamb = [=]() {
            this->DisableSceneDisplay();
            //#disable path that would lead to gfx globallock
            //# maybe show a "loading" screen or something
          };
          // mDbLock.ReleaseCurrent();
          opq::Op(lamb).QueueASync(opq::mainSerialQueue());
          break;
        }
        case ork::ent::SimulationEvent::ESIEV_ENABLE_VIEW: {
          auto lamb = [=]() {
            this->EnableSceneDisplay();
            //#disable path that would lead to gfx globallock
            //# maybe show a "loading" screen or something
          };
          opq::Op(lamb).QueueASync(opq::mainSerialQueue());
          // mDbLock.ReleaseCurrent();
          break;
        }
        case ork::ent::SimulationEvent::ESIEV_BIND:
          // mDbLock.ReleaseCurrent();
          break;
        case ork::ent::SimulationEvent::ESIEV_START: {
          auto lamb = [=]() { UpdateRefreshPolicy(); };
          opq::Op(lamb).QueueASync(opq::mainSerialQueue());
          break;
        }
        case ork::ent::SimulationEvent::ESIEV_STOP: {
          auto lamb = [=]() { UpdateRefreshPolicy(); };
          opq::Op(lamb).QueueASync(opq::mainSerialQueue());
          break;
        }
        case ork::ent::SimulationEvent::ESIEV_USER:
          break;
      }
    }
  });

  ///////////////////////////////////////////////////////////

  lev2::DrawableBuffer::ClearAndSyncWriters();

  ///////////////////////////////////////////////////////////

  mpBasicFrameTek = new BasicFrameTechnique();

  PushFrameTechnique(mpBasicFrameTek);

  _updateThread = new UpdateThread(this);
  _updateThread->start();
}

///////////////////////////////////////////////////////////////////////////

SceneEditorVP::~SceneEditorVP() {
  delete _updateThread;
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::Init() {

  ///////////////////////////////////////////////////////////
  // INIT BUILT IN TOOL HANDLERS

  ork::msleep(500);
  RegisterToolHandler("0Default", new DefaultUiHandler(mEditor));
  RegisterToolHandler("1ManipTrans", new ManipTransHandler(mEditor));
  RegisterToolHandler("2ManipRot", new ManipRotHandler(mEditor));

  mpDefaultHandler = mToolHandlers["0Default"];
  bindToolHandler("0Default");

  ///////////////////////////////////////////////////////////
  // INIT MODULAR TOOL HANDLERS

  for (auto initcb : mInitCallbacks)
    initcb(*this);
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::RegisterInitCallback(ork::ent::SceneEditorInitCb icb) {
  mInitCallbacks.insert(icb);
}
void SceneEditorVP::IncPickDirtyCount(int icount) {
  // _pickbuffer->SetDirty(true);
}
void SceneEditorView::SlotModelDirty() {
  mVP->IncPickDirtyCount(1);
}

///////////////////////////////////////////////////////////////////////////

SceneEditorView::SceneEditorView(SceneEditorVP* vp)
    : mVP(vp) {
}
///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DoInit(ork::lev2::Context* pTARG) {
  _pickbuffer = new ScenePickBuffer(this, pTARG);
  // orkprintf("PickBuffer<%p>\n", _pickbuffer);
  pTARG->FBI()->SetClearColor(fcolor4(0.0f, 0.0f, 0.0f, 0.0f));
  //////////////////////////////////////////////////////////
  // setup progress handler
  //////////////////////////////////////////////////////////
  auto handler = [this](opq::progressdata_ptr_t data) { //
    this->drawProgressFrame(data);
  };
  opq::setProgressHandler(handler);
  //////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::drawProgressFrame(opq::progressdata_ptr_t data) {
  // auto name_str = deco::decorate(fvec3(1, 0.5, 0.1), data->_queue_name);
  // auto grpn_str = deco::decorate(fvec3(1, 0.3, 0.0), data->_task_name);
  // auto pend_str = deco::format(fvec3(1, 1, 0.1), "%d", data->_num_pending);
  // printf(
  //  "WTF opq<%s> CompletionGroup<%s> ops pending<%s>     \r", //
  // name_str.c_str(),
  // grpn_str.c_str(),
  // pend_str.c_str());
  _ctxbase->progressHandler(data);
}

///////////////////////////////////////////////////////////////////////////

bool SceneEditorVP::isCompositorEnabled() {
  mRenderLock             = 1;
  bool compositor_enabled = false;
  auto compsys            = compositingSystem();
  if (simulation()) {
    if (compsys)
      compositor_enabled = compsys->enabled();
  }
  mRenderLock = 0;
  return compositor_enabled;
}

///////////////////////////////////////////////////////////////////////////
// Draw INTO the onscreen target
///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DoDraw(ui::drawevent_constptr_t drwev) {
  EASY_BLOCK("SceneEditorVP::DoDraw");

  if (nullptr == _ctxbase) {
    _ctxbase = _target->GetCtxBase();
  }

  int TARGW          = _target->mainSurfaceWidth();
  int TARGH          = _target->mainSurfaceHeight();
  const auto tgtrect = ViewportRect(0, 0, TARGW, TARGH);
  _renderer->setContext(_target);
  ////////////////////////////////////////////////
  lev2::RenderContextFrameData RCFD(_target);
  _target->pushRenderContextFrameData(&RCFD);
  /////////////////////////////////////////////////////////////////////////////////
  bool compositor_enabled = isCompositorEnabled();
  /////////////////////////////////////////////////////////////////////////////////
  lev2::UiViewportRenderTarget rt(this);
  /////////////////////////////////
  lev2::CompositingPassData TOPCPD;
  TOPCPD.SetDstRect(tgtrect);
  TOPCPD._irendertarget = &rt;
  TOPCPD.SetDstRect(tgtrect);
  /////////////////////////////////
  // We must have a compositor to continue...
  /////////////////////////////////
  auto compsys = compositingSystem();
  auto sim     = simulation();
  auto FBI     = _target->FBI();
  static CompositingData _gdata;
  static auto _gimpl = _gdata.createImpl();
  RCFD._cimpl        = _gimpl;
  _gimpl->pushCPD(TOPCPD);
  GL_ERRORCHECK();
  if (nullptr == compsys or nullptr == sim) {
    //////////////////////////////////////////////////////////////////////
    // no compositor case
    //////////////////////////////////////////////////////////////////////
    // still want to draw something so we know the editor is alive..
    GL_ERRORCHECK();
    _target->beginFrame();
    GL_ERRORCHECK();
    // we must still consume DrawableBuffers (since the compositor cannot)
    const DrawableBuffer* DB = DrawableBuffer::acquireForRead(7);
    FBI->SetAutoClear(true);
    FBI->setViewport(ViewportRect(0, 0, TARGW, TARGH));
    FBI->setScissor(ViewportRect(0, 0, TARGW, TARGH));
    this->Clear();
    DrawHUD(RCFD);
    DrawBorder(RCFD);
    if (DB) {
      DrawableBuffer::releaseFromRead(DB);
    } // release consumed DB
    _target->endFrame();
    _target->popRenderContextFrameData();
    _gimpl->popCPD();
    GL_ERRORCHECK();
    return;
  }
  RCFD._cimpl = compsys->_impl;
  RCFD._cimpl->pushCPD(TOPCPD);
  auto simmode = sim->GetSimulationMode();
  bool running = (simmode == ent::ESCENEMODE_RUN);
  ////////////////////////////////////////////////
  // FrameRenderer (and content)
  // rendering callback will be invoked from within compositor
  //  assembly pass(es)
  ////////////////////////////////////////////////
  //////////////////////////////////////////////////
  // setup viewport (main) rendertarget at top of stack
  //  so we can composite into it..
  //////////////////////////////////////////////////
  GL_ERRORCHECK();
  _target->debugPushGroup("toolvp::DRAWBEGIN");
  _renderer->setContext(_target);
  auto mainrect = _target->mainSurfaceRectAtWindowPos();
  SetRect(mainrect._x, mainrect._y, mainrect._w, mainrect._h);
  FBI->SetAutoClear(true);
  FBI->setViewport(ViewportRect(0, 0, TARGW, TARGH));
  FBI->setScissor(ViewportRect(0, 0, TARGW, TARGH));
  _target->debugPopGroup();
  GL_ERRORCHECK();
  _target->debugPushGroup("toolvp::DRAWBEGIN");
  _target->beginFrame();
  _target->debugPopGroup();
  GL_ERRORCHECK();
  _target->debugPushGroup("toolvp::DRAWBEGIN");
  this->Clear();
  _target->debugPopGroup();
  GL_ERRORCHECK();
  _target->debugPushGroup("toolvp::DRAWBEGIN");
  _target->debugPopGroup();
  GL_ERRORCHECK();
  //////////////////////////////////////////////////
  lev2::FrameRenderer framerenderer(RCFD, [&]() { renderMisc(RCFD); });
  lev2::CompositorDrawData drawdata(framerenderer);
  const DrawableBuffer* DB = nullptr;
  bool assembled_ok        = false;
  {
    EASY_BLOCK("acquireDB");
    DB = DrawableBuffer::acquireForRead(7); // mDbLock.Aquire(7);
  }
  //////////////////////////////////////////////////
  // composite assembly:
  //   render (or assemble) content into pre-compositing buffers
  //////////////////////////////////////////////////
  if (DB) {
    RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));
    drawdata._properties["primarycamindex"_crcu].Set<int>(miCameraIndex);
    drawdata._properties["cullcamindex"_crcu].Set<int>(miCullCameraIndex);
    drawdata._properties["irenderer"_crcu].Set<lev2::IRenderer*>(GetRenderer());
    drawdata._properties["simrunning"_crcu].Set<bool>(running);
    drawdata._properties["DB"_crcu].Set<const DrawableBuffer*>(DB);
    GL_ERRORCHECK();
    _target->debugPushGroup("toolvp::assemble");
    drawdata._cimpl = compsys->_impl;
    assembled_ok    = compsys->_impl->assemble(drawdata);
    DrawableBuffer::releaseFromRead(DB); // mDbLock.Aquire(7);
    //////////////////////////////////////////////////
    GL_ERRORCHECK();
    _target->debugMarker(FormatString("toolvp::assembled_ok<%d>", int(assembled_ok)));
    GL_ERRORCHECK();
    _target->debugPopGroup();
    GL_ERRORCHECK();
  }
  //////////////////////////////////////////////////
  // final compositing :
  //   combine previously assembled content
  //   into final image
  //////////////////////////////////////////////////
  if (assembled_ok) {
    _target->debugPushGroup("toolvp::composite");
    EASY_BLOCK("composite");
    compsys->_impl->composite(drawdata);
    _target->debugPopGroup();
  }
  //////////////////////////////////////////////////
  if (mEditor.mpScene) {
    drawdata._properties["simrunning"_crcu].Set<bool>(running);
    DrawManip(drawdata, _target);
  }
  //////////////////////////////////////////////////
  // todo - lock sim
  //////////////////////////////////////////////////
  GL_ERRORCHECK();
  RCFD._cimpl->popCPD();
  RCFD._cimpl = _gimpl;
  //////////////////////////////////////////////////
  // after composite:
  //  render hud and other 2d non-content layers
  //////////////////////////////////////////////////
  _target->debugPushGroup("toolvp::DRAWEND");
  if (gtoggle_hud) {

    EASY_BLOCK("HUD");

    DrawHUD(RCFD);
    _target->debugPushGroup("toolvp::DRAWEND::Children");
    drawChildren(drwev);
    _target->debugPopGroup();
    if (false == FBI->isPickState())
      DrawBorder(RCFD);
  }
  //////////////////////////////////////////////////
  {
    EASY_BLOCK("ENDFRAME");
    _target->endFrame();
    _target->debugPopGroup();
  }
  //////////////////////////////////////////////////
  // update editor camera (TODO - move to engine)
  //////////////////////////////////////////////////
  if (auto trycam = drawdata._properties["seleditcam"_crcu].TryAs<const CameraData*>()) {
    auto CAMDAT   = trycam.value();
    _editorCamera = CAMDAT ? CAMDAT->getUiCamera() : nullptr;
    ManipManager().SetActiveCamera(_editorCamera);
    _target->debugMarker(FormatString("toolvp::_editorCamera<%p>", _editorCamera));
  }
  ///////////////////////////////////////////////////////
  // filth up the pick buffer
  ///////////////////////////////////////////////////////
  if (miPickDirtyCount > 0) {
    if (_pickbuffer) {
      //_pickbuffer->SetDirty(true);
      miPickDirtyCount--;
    }
  }
  _gimpl->popCPD();
  _target->popRenderContextFrameData();
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////

ent::CompositingSystem* SceneEditorVP::compositingSystem() {
  auto psi = mEditor.GetActiveSimulation();
  return (psi != nullptr) ? psi->compositingSystem() : nullptr;
}

///////////////////////////////////////////////////////////////////////////////

const ent::Simulation* SceneEditorVP::simulation() {
  const ent::Simulation* psi = 0;
  if (mEditor.mpScene) {
    if (StringPoolStack::Top()) {
      psi = mEditor.GetActiveSimulation();
    }
  }
  return psi;
}

///////////////////////////////////////////////////////////////////////////////
// Queue and Render Scene into active target
//  this may go to the onscreen or pickbuffer targets
///////////////////////////////////////////////////////////////////////////////

struct ScopedSimFramer {
  ScopedSimFramer(const Simulation* sim)
      : _sim(sim) {
    sim->beginRenderFrame();
  }
  ~ScopedSimFramer() {
    _sim->endRenderFrame();
  }
  const Simulation* _sim;
};

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::renderMisc(lev2::RenderContextFrameData& RCFD) {
  ///////////////////////////////////////////////////////////////////////////
  // auto sim = simulation();
  // if((nullptr==sim))
  // return;
  ///////////////////////////////////////////////////////////////////////////
  // ScopedSimFramer framescope(sim);
  ///////////////////////////////////////////////////////////////////////////
  const auto& topCPD        = RCFD.topCPD();
  lev2::IRenderTarget* pIRT = topCPD._irendertarget;
  auto gfxtarg              = RCFD.GetTarget();
  auto FBI                  = gfxtarg->FBI();
  auto MTXI                 = gfxtarg->MTXI();
  ///////////////////////////////////////////////////////////////////////////
  // RENDER!
  ///////////////////////////////////////////////////////////////////////////
  // FBI->GetThisBuffer()->SetDirty(false);
  // gfxtarg->BindMaterial(lev2::GfxEnv::GetDefault3DMaterial());
  // static lev2::SRasterState defstate;
  // gfxtarg->RSI()->BindRasterState(defstate, true);
  /////////////////////////////////////////
  // gfxtarg->debugPushGroup("toolvp::DrawGrid");
  // if (false == FBI->isPickState())
  // DrawGrid(RCFD);
  // gfxtarg->debugPopGroup();
  ///////////////////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::UpdateRefreshPolicy() {
  auto sim = simulation();

  if (nullptr == sim) {
    //_ctxbase->SetRefreshPolicy(lev2::CTXBASE::EREFRESH_WHENDIRTY);
    return;
  }

  ///////////////////////////////////////////////////////////
  // refresh control

  static orkstack<ent::ESimulationMode> semodestack;

  if (_ctxbase) {
    ent::ESimulationMode ecurmode = semodestack.size() ? semodestack.top() : ent::ESCENEMODE_EDIT;
    ent::ESimulationMode enewmode = sim->GetSimulationMode();

    // if( enewmode != ecurmode )
    {
      switch (enewmode) {
        case ent::ESCENEMODE_EDIT: {
          lev2::RefreshPolicyItem policy;
          policy._policy = lev2::EREFRESH_FIXEDFPS;
          policy._fps    = 2;
          _ctxbase->_setRefreshPolicy(policy);
          break;
        }
        case ent::ESCENEMODE_RUN:
        case ent::ESCENEMODE_SINGLESTEP:
        case ent::ESCENEMODE_PAUSE: {
          lev2::RefreshPolicyItem policy;
          policy._policy = lev2::EREFRESH_FASTEST;
          _ctxbase->_setRefreshPolicy(policy);
          break;
        }
        default:
          break;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawHUD(lev2::RenderContextFrameData& FrameData) {
  lev2::Context* pTARG = FrameData.GetTarget();
  _target->debugPushGroup("toolvp::DrawHUD");
  auto MTXI              = pTARG->MTXI();
  auto GBI               = pTARG->GBI();
  const auto& topCPD     = FrameData.topCPD();
  const auto& frame_rect = topCPD.GetDstRect();

  int itx0 = frame_rect._x;
  int itx1 = frame_rect._x + frame_rect._w;
  int ity0 = frame_rect._y;
  int ity1 = frame_rect._y + frame_rect._h;

  if (pTARG->hiDPI()) {
    itx0 /= 2;
    itx1 /= 2;
    ity0 /= 2;
    ity1 /= 2;
  }

  static Texture* pplaytex = ork::asset::AssetManager<ork::lev2::TextureAsset>::declare("lev2://textures/play_icon")->GetTexture();
  static Texture* ppaustex = ork::asset::AssetManager<ork::lev2::TextureAsset>::declare("lev2://textures/pause_icon")->GetTexture();

  /////////////////////////////////////////////////
  lev2::GfxMaterialUI UiMat(pTARG);
  MTXI->PushPMatrix(fmtx4::Identity());
  MTXI->PushVMatrix(fmtx4::Identity());
  MTXI->PushMMatrix(fmtx4::Identity());

  static SRasterState defstate;
  pTARG->RSI()->BindRasterState(defstate);
  {
    /////////////////////////////////////////////////
    // little spinner so i know which window is active
    pTARG->PushModColor(fcolor4::White());

    {
      static float gfspinner = 0.0f;
      float fx               = sinf(gfspinner);
      float fy               = cosf(gfspinner);

      int ilen   = 16;
      int ibaseX = itx1 - ilen;
      int ibaseY = ity1 - ilen;
      lev2::VtxWriter<SVtxV12C4T16> vw;

      vw.Lock(pTARG, &GfxEnv::GetSharedDynamicVB(), 2);
      {
        u32 ucolor = 0xffffffff;
        ork::fvec2 uvZ(0.0f, 0.0f);
        float fZ = 0.0f;

        lev2::SVtxV12C4T16 v0(fvec3(float(ibaseX - int(fx * ilen)), float(ibaseY - int(fy * ilen)), fZ), uvZ, ucolor);
        lev2::SVtxV12C4T16 v1(fvec3(float(ibaseX + int(fx * ilen)), float(ibaseY + int(fy * ilen)), fZ), uvZ, ucolor);

        vw.AddVertex(v0);
        vw.AddVertex(v1);
      }
      vw.UnLock(pTARG);
      MTXI->PushUIMatrix();
      GBI->DrawPrimitive(&UiMat, vw, lev2::PrimitiveType::LINES, 2);
      MTXI->PopUIMatrix();
      gfspinner += (PI2 / 60.0f);
    }
    pTARG->PopModColor();
    pTARG->PushModColor(fcolor4::Yellow());
    {

      float gfspinner = OldSchool::GetRef().GetLoResRelTime() * PI;
      float fx        = sinf(gfspinner);
      float fy        = cosf(gfspinner);

      int ilen   = 16;
      int ibaseX = itx1 - ilen;
      int ibaseY = ity1 - ilen;

      lev2::VtxWriter<SVtxV12C4T16> vw;

      vw.Lock(pTARG, &GfxEnv::GetSharedDynamicVB(), 2);
      {
        u32 ucolor = 0xffff00ff;
        ork::fvec2 uvZ(0.0f, 0.0f);
        float fZ = 0.0f;

        lev2::SVtxV12C4T16 v0(fvec3(float(ibaseX - int(fx * ilen)), float(ibaseY - int(fy * ilen)), fZ), uvZ, ucolor);
        lev2::SVtxV12C4T16 v1(fvec3(float(ibaseX + int(fx * ilen)), float(ibaseY + int(fy * ilen)), fZ), uvZ, ucolor);

        vw.AddVertex(v0);
        vw.AddVertex(v1);
      }
      vw.UnLock(pTARG);
      MTXI->PushUIMatrix();
      GBI->DrawPrimitive(&UiMat, vw, lev2::PrimitiveType::LINES, 2);
      MTXI->PopUIMatrix();
    }
    pTARG->PopModColor();
    /////////////////////////////////////////////////
    if (gtoggle_pickbuffer && mEditor.GetActiveSimulation()) {

      ent::ESimulationMode emode = mEditor.GetActiveSimulation()->GetSimulationMode();

      Texture* ptex = (emode == ent::ESCENEMODE_RUN) ? pplaytex : ppaustex;

      static lev2::GfxMaterialUITextured UiMat(pTARG);

      //////////////////////////////////////////
      // Test PickBuffer
      //////////////////////////////////////////

      GfxMaterial* nextmtl = nullptr;
      if (_pickbuffer) {
        lev2::PixelFetchContext pfc;
        pfc._gfxContext = pTARG;
        _pickbuffer->Draw(pfc);
        if (_pickbuffer->_rtgroup) {
          auto mrt           = _pickbuffer->_rtgroup->GetMrt(0);
          static auto texmtl = std::make_shared<lev2::GfxMaterialUITextured>(pTARG);
          ptex               = mrt->texture();
          texmtl->SetTexture(lev2::ETEXDEST_DIFFUSE, ptex);
          ptex->TexSamplingMode().PresetPointAndClamp();
          pTARG->TXI()->ApplySamplingMode(ptex);
          nextmtl = texmtl.get();
        }
        UiMat.SetTexture(lev2::ETEXDEST_DIFFUSE, ptex);
      } else
        nextmtl = &UiMat;

      //////////////////////////////////////////

      UiMat._rasterstate.SetBlending(lev2::Blending::OFF);
      pTARG->PushModColor(fcolor4::White());
      {
        const int ksize = 512;
        lev2::VtxWriter<SVtxV12C4T16> vw;

        vw.Lock(pTARG, &GfxEnv::GetSharedDynamicVB(), 6);
        {
          float fZ   = 0.0f;
          u32 ucolor = 0xffffffff;
          f32 x0     = 4.0f;
          f32 y0     = f32(ity1 - ksize);
          f32 w0     = f32(ksize + 4);
          f32 h0     = f32(ksize - 18);
          f32 x1     = x0 + w0;
          f32 y1     = y0 + h0;
          ork::fvec2 uv0(0.0f, 1.0f);
          ork::fvec2 uv1(1.0f, 1.0f);
          ork::fvec2 uv2(1.0f, 0.0f);
          ork::fvec2 uv3(0.0f, 0.0f);
          ork::fvec3 vv0(x0, y0, fZ);
          ork::fvec3 vv1(x1, y0, fZ);
          ork::fvec3 vv2(x1, y1, fZ);
          ork::fvec3 vv3(x0, y1, fZ);

          lev2::SVtxV12C4T16 v0(vv0, uv0, ucolor);
          lev2::SVtxV12C4T16 v1(vv1, uv1, ucolor);
          lev2::SVtxV12C4T16 v2(vv2, uv2, ucolor);
          lev2::SVtxV12C4T16 v3(vv3, uv3, ucolor);

          vw.AddVertex(v0);
          vw.AddVertex(v1);
          vw.AddVertex(v2);

          vw.AddVertex(v2);
          vw.AddVertex(v3);
          vw.AddVertex(v0);
        }
        vw.UnLock(pTARG);
        MTXI->PushUIMatrix();
        GBI->DrawPrimitive(nextmtl, vw, lev2::PrimitiveType::TRIANGLES, 6);
        MTXI->PopUIMatrix();
      }
      pTARG->PopModColor();

      if (emode == ent::ESCENEMODE_RUN) {
        ork::lev2::DrawHudEvent drawHudEvent(pTARG, 1);
        if (mEditor.GetActiveSimulation()->GetApplication())
          mEditor.GetActiveSimulation()->GetApplication()->Notify(&drawHudEvent);
      }
    }
    /////////////////////////////////////////////////
    /////////////////////////////////////////////////
    /////////////////////////////////////////////////
    int iy = 4;
    int ix = 4;
    for (auto it : mToolHandlers) {
      SceneEditorVPToolHandler* phandler = it.second;
      bool bhilite                       = (phandler == mpCurrentHandler);
      phandler->DrawToolIcon(pTARG, ix, iy, bhilite);
      iy += 36;
    }
    /////////////////////////////////////////////////
    /////////////////////////////////////////////////
    // PerformanceTracker::GetRef().Draw(pTARG);
    /////////////////////////////////////////////////
    /////////////////////////////////////////////////
  }

  MTXI->PopPMatrix(); // back to ortho
  MTXI->PopVMatrix(); // back to ortho
  MTXI->PopMMatrix(); // back to ortho

  _target->debugPopGroup();
} // namespace ent

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawGrid(ork::lev2::RenderContextFrameData& RCFD) {
  const auto& topCPD = RCFD.topCPD();
  auto cammatrices   = topCPD.cameraMatrices();
  if (nullptr == cammatrices)
    return;

  _target->debugPushGroup("toolvp::DrawGrid");
  auto& GRID = ManipManager().Grid();
  switch (mGridMode) {
    case 0:
      GRID.SetGridMode(lev2::Grid3d::EGRID_XZ);
      GRID.Calc(*cammatrices);
      break;
    case 1:
      GRID.SetGridMode(lev2::Grid3d::EGRID_XZ);
      GRID.Calc(*cammatrices);
      GRID.Render(RCFD);
      break;
    case 2:
      GRID.SetGridMode(lev2::Grid3d::EGRID_XY);
      GRID.Calc(*cammatrices);
      GRID.Render(RCFD);
      break;
  }
  _target->debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawManip(lev2::CompositorDrawData& CDD, ork::lev2::Context* pProxyTarg) {
  FrameRenderer& framerenderer      = CDD.mFrameRenderer;
  RenderContextFrameData& RCFD      = framerenderer.framedata();
  auto CIMPL                        = RCFD._cimpl;
  ork::lev2::Context* pOutputTarget = RCFD.GetTarget();

  CDD._properties["OutputWidth"_crcu].Set<int>(width());
  CDD._properties["OutputHeight"_crcu].Set<int>(height());
  CDD._properties["StereoEnable"_crcu].Set<bool>(false);
  lev2::CompositingPassData myCPD;
  myCPD.defaultSetup(CDD);

  /////////////////////////////////////////
  const DrawableBuffer* DB = DrawableBuffer::acquireForRead(7);
  if (DB) {
    auto spncam = (CameraData*)DB->cameraData("spawncam"_pool);
    if (spncam) {
      (*_overlayCamMatrices.get()) = spncam->computeMatrices(float(width()) / float(height()));
    }
    CDD._properties["defcammtx"_crcu].Set<const CameraMatrices*>(_overlayCamMatrices.get());
    DrawableBuffer::releaseFromRead(DB); // mDbLock.Aquire(7);
  }
  /////////////////////////////////////////

  myCPD._cameraMatrices = _overlayCamMatrices.get();
  RCFD._cimpl->pushCPD(myCPD);
  const auto& topCPD = RCFD.topCPD();
  OrkAssert(RCFD._cimpl == CIMPL);
  pOutputTarget->debugPushGroup("toolvp::DrawManip");
  pOutputTarget->FBI()->clearDepth(0.0);
  auto MTXI = pOutputTarget->MTXI();
  MTXI->PushPMatrix(_overlayCamMatrices->_pmatrix);
  MTXI->PushVMatrix(_overlayCamMatrices->_vmatrix);
  MTXI->PushMMatrix(fmtx4::Identity());
  {
    switch (ManipManager().GetUIMode()) {
      case ManipManager::EUIMODE_MANIP_WORLD_TRANSLATE:
      case ManipManager::EUIMODE_MANIP_LOCAL_ROTATE: {
        GetRenderer()->setContext(pProxyTarg);

        ///////////////////////////////////////
        const fvec4& ScreenXNorm = pProxyTarg->MTXI()->GetScreenRightNormal();

        fmtx4 MatW;
        ManipManager().GetCurTransform().GetMatrix(MatW);
        const fvec4 V0 = MatW.GetTranslation();
        const fvec4 V1 = V0 + ScreenXNorm * float(30.0f);
        fvec2 VP(float(pProxyTarg->mainSurfaceWidth()), float(pProxyTarg->mainSurfaceHeight()));
        fvec3 Pos = MatW.GetTranslation();
        fvec3 UpVector;
        fvec3 RightVector;
        _overlayCamMatrices->GetPixelLengthVectors(Pos, VP, UpVector, RightVector);
        float rscale = RightVector.Mag(); // float(100.0f);
        // printf( "OUTERmanip rscale<%f>\n", rscale );

        static float fRSCALE = 1.0f;

        if (myCPD.isPicking()) {
          ManipManager().SetViewScale(fRSCALE);
        } else {
          float fW = float(pOutputTarget->mainSurfaceWidth());
          float fH = float(pOutputTarget->mainSurfaceHeight());
          fRSCALE  = ManipManager().CalcViewScale(fW, fH, _overlayCamMatrices.get());
          ManipManager().SetViewScale(fRSCALE);
        }

        ///////////////////////////////////////

        ManipManager().Setup(GetRenderer());
        ManipManager().DrawCurrentManipSet(pOutputTarget);

        GetRenderer()->setContext(pOutputTarget);
        break;
      }
      default:
        break;
    }
  }
  MTXI->PopPMatrix();
  MTXI->PopVMatrix();
  MTXI->PopMMatrix();
  RCFD._cimpl->popCPD();
  pOutputTarget->debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawBorder(lev2::RenderContextFrameData& RCFD) {
  const auto& topCPD = RCFD.topCPD();
  bool bhasfocus     = _uicontext->hasKeyboardFocus();
  float fw           = topCPD.GetDstRect()._w;
  float fh           = topCPD.GetDstRect()._h;
  auto TGT           = RCFD.GetTarget();

  _target->debugPushGroup("toolvp::DrawBorder");

  auto MTXI       = TGT->MTXI();
  ork::fmtx4 mtxP = MTXI->Ortho(0.0f, fw, 0.0f, fh, 0.0f, 1.0f);
  GfxMaterialUI matui(TGT);
  TGT->PushModColor(bhasfocus ? ork::fcolor4::Red() : ork::fcolor4::Black());
  MTXI->PushPMatrix(mtxP);
  MTXI->PushVMatrix(ork::fmtx4::Identity());
  MTXI->PushMMatrix(ork::fmtx4::Identity());
  {
    typedef SVtxV12C4T16 vtx_t;
    DynamicVertexBuffer<vtx_t>& vb = GfxEnv::GetSharedDynamicVB();

    int ivcount = 16;

    VtxWriter<vtx_t> vwriter;
    vwriter.Lock(TGT, &vb, ivcount);

    float fx1 = fw - 1.0f;
    float fy1 = fh - 1.0f;
    vwriter.AddVertex(vtx_t(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(vtx_t(fx1, 0.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(vtx_t(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(vtx_t(fx1, 1.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));

    vwriter.AddVertex(vtx_t(fx1, 0.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(vtx_t(fx1, fy1, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(vtx_t(fx1 - 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(vtx_t(fx1 - 1.0f, fy1, 0.0f, 0.0f, 0.0f, 0xffffffff));

    vwriter.AddVertex(vtx_t(fx1, fy1, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(vtx_t(0.0f, fy1, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(vtx_t(fx1, fy1 - 1.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(vtx_t(0.0f, fy1 - 1.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));

    vwriter.AddVertex(vtx_t(0.0f, fy1, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(vtx_t(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(vtx_t(0.0f, fy1, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(vtx_t(1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));

    vwriter.UnLock(TGT);

    TGT->GBI()->DrawPrimitive(&matui, vwriter, ork::lev2::PrimitiveType::LINES);
  }
  MTXI->PopPMatrix(); // back to ortho
  MTXI->PopVMatrix(); // back to ortho
  MTXI->PopMMatrix(); // back to ortho
  TGT->PopModColor();

  _target->debugPopGroup();
}

} // namespace ent
} // namespace ork

///////////////////////////////////////////////////////////////////////////
