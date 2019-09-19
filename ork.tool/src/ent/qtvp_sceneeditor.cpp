////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/timer.h>
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
#include <ork/reflect/RegisterProperty.h>
#include <orktool/qtui/qtvp_edrenderer.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>

#include <orktool/toolcore/dataflow.h>

///////////////////////////////////////////////////////////////////////////////

#include "qtui_scenevp.h"
#include "qtvp_uievh.h"
#include <pkg/ent/CompositingSystem.h>
#include <pkg/ent/editor/edmainwin.h>

#include <ork/gfx/camera.h>
#include <ork/kernel/future.hpp>
#include <ork/lev2/lev2_asset.h>

#include <pkg/ent/LightingSystem.h>
#include <pkg/ent/scene.hpp>

///////////////////////////////////////////////////////////////////////////////

extern bool gtoggle_hud;
extern bool gtoggle_pickbuffer;

using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////

template class ork::lev2::PickBuffer<ork::ent::SceneEditorVP>;

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

void SceneEditorView::Describe() { reflect::RegisterFunctor("SlotModelDirty", &SceneEditorView::SlotModelDirty); }

///////////////////////////////////////////////////////////////////////////////

orkset<SceneEditorInitCb> SceneEditorVP::mInitCallbacks;

///////////////////////////////////////////////////////////////////////////////

UpdateThread* gupdatethread = 0;

void SceneEditorVP::Describe() {}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DisableSceneDisplay() {
  ork::AssertOnOpQ2(ork::MainThreadOpQ());
  mbSceneDisplayEnable = false;
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::EnableSceneDisplay() {
  ork::AssertOnOpQ2(ork::MainThreadOpQ());
  mbSceneDisplayEnable = true;
}

SceneEditorVP::SceneEditorVP(const std::string& name, SceneEditorBase& the_ed, EditorMainWindow& MainWin)
    : ui::Viewport(name, 1, 1, 1, 1, CColor3(0.0f, 0.0f, 0.0f), 1.0f)
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

  ///////////////////////////////////////////////////////////

  _simchannelsubscriber = msgrouter::channel("Simulation")->subscribe([=](msgrouter::content_t c) {
    if (auto as_sei = c.TryAs<ork::ent::SimulationEvent>()) {
      auto& sei = as_sei.value();
      switch (sei.GetEvent()) {
        case ork::ent::SimulationEvent::ESIEV_DISABLE_UPDATE: {
          auto lamb = [=]() { gUpdateStatus.SetState(EUPD_STOP); };
          Op(lamb).QueueASync(UpdateSerialOpQ());
          break;
        }
        case ork::ent::SimulationEvent::ESIEV_ENABLE_UPDATE: {
          auto lamb = [=]() { gUpdateStatus.SetState(EUPD_START); };
          Op(lamb).QueueASync(UpdateSerialOpQ());
          break;
        }
        case ork::ent::SimulationEvent::ESIEV_DISABLE_VIEW: {
          auto lamb = [=]() {
            this->DisableSceneDisplay();
            //#disable path that would lead to gfx globallock
            //# maybe show a "loading" screen or something
          };
          Op(lamb).QueueASync(MainThreadOpQ());
          // mDbLock.ReleaseCurrent();
          break;
        }
        case ork::ent::SimulationEvent::ESIEV_ENABLE_VIEW: {
          auto lamb = [=]() {
            this->EnableSceneDisplay();
            //#disable path that would lead to gfx globallock
            //# maybe show a "loading" screen or something
          };
          Op(lamb).QueueASync(MainThreadOpQ());
          // mDbLock.ReleaseCurrent();
          break;
        }
        case ork::ent::SimulationEvent::ESIEV_BIND:
          // mDbLock.ReleaseCurrent();
          break;
        case ork::ent::SimulationEvent::ESIEV_START:
          break;
        case ork::ent::SimulationEvent::ESIEV_STOP:
          break;
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

SceneEditorVP::~SceneEditorVP() { delete _updateThread; }

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::Init() {

  ///////////////////////////////////////////////////////////
  // INIT BUILT IN TOOL HANDLERS

  ork::msleep(500);
  RegisterToolHandler("0Default", new TestVPDefaultHandler(mEditor));
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

void SceneEditorVP::RegisterInitCallback(ork::ent::SceneEditorInitCb icb) { mInitCallbacks.insert(icb); }
void SceneEditorVP::IncPickDirtyCount(int icount) { mpPickBuffer->SetDirty(true); }
void SceneEditorView::SlotModelDirty() { mVP->IncPickDirtyCount(1); }

///////////////////////////////////////////////////////////////////////////

SceneEditorView::SceneEditorView(SceneEditorVP* vp)
    : mVP(vp) {}
///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DoInit(ork::lev2::GfxTarget* pTARG) {
  mpPickBuffer = new lev2::PickBuffer<SceneEditorVP>(
      pTARG->FBI()->GetThisBuffer(), this, 0, 0, 1024, 1024, lev2::PickBufferBase::EPICK_FACE_VTX);
  mpPickBuffer->RefClearColor().SetRGBAU32(0);
  mpPickBuffer->CreateContext();
  mpPickBuffer->GetContext()->FBI()->SetClearColor(fcolor4(0.0f, 0.0f, 0.0f, 0.0f));
  int iw = pTARG->GetW();
  int ih = pTARG->GetH();

  pTARG->FBI()->SetClearColor(fcolor4(0.0f, 0.0f, 0.0f, 0.0f));

  orkprintf("PickBuffer<%p>\n", mpPickBuffer);
}

///////////////////////////////////////////////////////////////////////////

bool SceneEditorVP::isCompositorEnabled() {
  mRenderLock             = 1;
  bool compositor_enabled = false;
  auto compsys            = compositingSystem();
  if (simulation()) {
    ent::ESimulationMode emode = simulation()->GetSimulationMode();
    if (compsys)
      switch (emode) {
        case ent::ESCENEMODE_RUN:
        case ent::ESCENEMODE_SINGLESTEP: {
          compositor_enabled = compsys->enabled();
          break;
        }
        default:
          break;
      }
  }
  mRenderLock = 0;
  return compositor_enabled;
}

///////////////////////////////////////////////////////////////////////////
// Draw INTO the onscreen target
///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DoDraw(ui::DrawEvent& drwev) {
  int TARGW           = mpTarget->GetW();
  int TARGH           = mpTarget->GetH();
  const SRect tgtrect = SRect(0, 0, TARGW, TARGH);
  _renderer->SetTarget(mpTarget);
  ////////////////////////////////////////////////
  lev2::RenderContextFrameData RCFD;
  RCFD.SetDstRect(tgtrect);
  RCFD.SetTarget(mpTarget);
  /////////////////////////////////////////////////////////////////////////////////
  bool compositor_enabled = isCompositorEnabled();
  /////////////////////////////////////////////////////////////////////////////////
  lev2::UiViewportRenderTarget rt(this);
  auto FBI = mpTarget->FBI();
  /////////////////////////////////
  auto DRAWBEGIN = [&]() {
    RCFD.PushRenderTarget(&rt);
    RCFD.SetTarget(mpTarget);
    _renderer->SetTarget(mpTarget);
    RCFD.SetDstRect(tgtrect);
    FBI->SetAutoClear(true);
    FBI->SetViewport(0, 0, TARGW, TARGH);
    FBI->SetScissor(0, 0, TARGW, TARGH);
    mpTarget->BeginFrame();
  };
  /////////////////////////////////
  auto DRAWEND = [&]() {
    if (gtoggle_hud) {
      DrawHUD(RCFD);
      DrawChildren(drwev);
    }
    mpTarget->EndFrame();
    RCFD.PopRenderTarget();
  };
  ////////////////////////////////////////////////
  // FrameRenderer (and content)
  ////////////////////////////////////////////////
  lev2::FrameRenderer framerenderer(RCFD, [&]() {
    if (false == mbSceneDisplayEnable)
      return;
    ///////////////////////////////////////////////////////////////////////////
    _renderer->SetTarget(mpTarget);
    SetRect(mpTarget->GetX(), mpTarget->GetY(), mpTarget->GetW(), mpTarget->GetH());
    ///////////////////////////////////////////////////////////////////////////
    auto NODE              = CompositingPassData::FromRCFD(RCFD);
    NODE.updateCompositingSize(mpTarget->GetW(),mpTarget->GetH());
    GetClearColorRef() = NODE._clearColor.xyz();
    this->Clear();
    if (NODE.mbDrawSource)
      NODE.renderPass(RCFD,[&](){
        mSceneView.UpdateRefreshPolicy(RCFD, simulation());
        renderEnqueuedScene(RCFD);
      });
    ///////////////////////////////////////////////////////
    // filth up the pick buffer
    ///////////////////////////////////////////////////////
    if (miPickDirtyCount > 0) {
      if (mpPickBuffer) {
        mpPickBuffer->SetDirty(true);
        miPickDirtyCount--;
      }
    }
    ///////////////////////////////////////////////////////
  });
  /////////////////////////////////
  // Compositor ?
  /////////////////////////////////
  RCFD.PushRenderTarget(&rt);
  if (compositor_enabled) {
    auto compsys = compositingSystem();
    compsys->_impl.renderContent(framerenderer);
    DRAWBEGIN();
    compsys->_impl.composeToScreen(mpTarget);
    DRAWEND();
  }
  /////////////////////////////////
  else // No Compositor
  /////////////////////////////////
  {
    auto DB     = lev2::DrawableBuffer::BeginDbRead(7);
    mRenderLock = 1;
    RCFD.setUserProperty("DB", rendervar_t(DB));
    if (DB) {
      DRAWBEGIN();
      rendervar_t passdata;
      passdata.Set<compositingpassdatastack_t*>(&mCompositingGroupStack);
      RCFD.setUserProperty("nodes"_crc, passdata);
      lev2::CompositingPassData node;
      mCompositingGroupStack.push(node);
      mpBasicFrameTek->_shouldBeginAndEndFrame = false;
      mpBasicFrameTek->Render(framerenderer);
      mCompositingGroupStack.pop();
      DRAWEND();
      lev2::DrawableBuffer::EndDbRead(DB);
    }
    mRenderLock = 0;
  }
  RCFD.SetDstRect(tgtrect);
  RCFD.PopRenderTarget();
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
    if (ApplicationStack::Top()) {
      psi = mEditor.GetActiveSimulation();
    }
  }
  return psi;
}

///////////////////////////////////////////////////////////////////////////////
// Queue and Render Scene into active target
//  this may go to the onscreen or pickbuffer targets
///////////////////////////////////////////////////////////////////////////////

#define GL_ERRORCHECK()                                                                                                            \
  {                                                                                                                                \
    int iErr = GetGlError();                                                                                                       \
    OrkAssert(iErr == 0);                                                                                                          \
  }

///////////////////////////////////////////////////////////////////////////

struct ScopedSimFramer {
  ScopedSimFramer(const Simulation* sim)
      : _sim(sim) {
    sim->beginRenderFrame();
  }
  ~ScopedSimFramer() { _sim->endRenderFrame(); }
  const Simulation* _sim;
};

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::renderEnqueuedScene(lev2::RenderContextFrameData& RCFD) {
  ///////////////////////////////////////////////////////////////////////////
  auto sim = simulation();
  if (nullptr == sim)
    return;
  ///////////////////////////////////////////////////////////////////////////
  ScopedSimFramer framescope(sim);
  ///////////////////////////////////////////////////////////////////////////
  auto DB = RCFD.GetDB();
  if (0 == DB)
    return;
  ///////////////////////////////////////////////////////////////////////////
  lev2::IRenderTarget* pIRT = RCFD.GetRenderTarget();
  auto gfxtarg              = RCFD.GetTarget();
  auto FBI                  = gfxtarg->FBI();
  auto MTXI                 = gfxtarg->MTXI();
  ///////////////////////////////////////////////////////////////////////////
  auto NODE     = CompositingPassData::FromRCFD(RCFD);
  auto CAMDAT   = NODE.getCamera(RCFD, miCameraIndex, miCullCameraIndex);
  auto& CAMCCTX = RCFD.GetCameraCalcCtx();
  CameraData TempCamData;
  if (CAMDAT) {
    TempCamData = *CAMDAT;
    TempCamData.BindGfxTarget(gfxtarg);
    TempCamData.CalcCameraData(CAMCCTX);
    RCFD.SetCameraData(&TempCamData);
  } else
    return;
  ///////////////////////////////////////////////////////////////////////////
  _editorCamera = CAMDAT ? CAMDAT->getEditorCamera() : nullptr;
  if (_editorCamera) {
    _editorCamera->AttachViewport(this);
    _editorCamera->RenderUpdate();
  }
  ManipManager().SetActiveCamera(_editorCamera);
  ///////////////////////////////////////////////////////////////////////////
  // DrawableBuffer -> RenderQueue enqueue
  ///////////////////////////////////////////////////////////////////////////
  auto rend = GetRenderer();
  for (const PoolString& layer_name : NODE.getLayerNames())
    DB->enqueueLayerToRenderQueue(layer_name, rend);
  ///////////////////////////////////////////////////////////////////////////
  // RENDER!
  ///////////////////////////////////////////////////////////////////////////
  FBI->GetThisBuffer()->SetDirty(false);
  gfxtarg->BindMaterial(lev2::GfxEnv::GetDefault3DMaterial());
  static lev2::SRasterState defstate;
  gfxtarg->RSI()->BindRasterState(defstate, true);
  MTXI->PushPMatrix(CAMCCTX.mPMatrix);
  MTXI->PushVMatrix(CAMCCTX.mVMatrix);
  MTXI->PushMMatrix(fmtx4::Identity);
  /////////////////////////////////////////
  rend->drawEnqueuedRenderables();
  /////////////////////////////////////////
  if (mEditor.mpScene)
    DrawManip(RCFD, gfxtarg);
  /////////////////////////////////////////
  if (false == FBI->IsPickState())
    DrawGrid(RCFD);
  /////////////////////////////////////////
  MTXI->PopPMatrix(); // back to ortho
  MTXI->PopVMatrix(); // back to ortho
  MTXI->PopMMatrix(); // back to ortho
  if (false == FBI->IsPickState())
    DrawSpinner(RCFD);
  ///////////////////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorView::UpdateRefreshPolicy(lev2::RenderContextFrameData& FrameData, const Simulation* sinst) {
  if (0 == sinst)
    return;

  ///////////////////////////////////////////////////////////
  // refresh control

  lev2::CTXBASE* pctxb = FrameData.GetTarget()->GetCtxBase();

  lev2::GfxTarget* ptarg = FrameData.GetTarget();

  static orkstack<ent::ESimulationMode> semodestack;

  if (pctxb) {
    ent::ESimulationMode ecurmode = semodestack.size() ? semodestack.top() : ent::ESCENEMODE_EDIT;
    ent::ESimulationMode enewmode = sinst->GetSimulationMode();

    // if( enewmode != ecurmode )
    {
      switch (enewmode) {
        case ent::ESCENEMODE_EDIT:
          pctxb->SetRefreshPolicy(lev2::CTXBASE::EREFRESH_WHENDIRTY);
          break;
        case ent::ESCENEMODE_RUN:
        case ent::ESCENEMODE_SINGLESTEP:
        case ent::ESCENEMODE_PAUSE:
          pctxb->SetRefreshPolicy(lev2::CTXBASE::EREFRESH_FASTEST);
          break;
        default:
          break;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawHUD(lev2::RenderContextFrameData& FrameData) {
  lev2::GfxTarget* pTARG = FrameData.GetTarget();
  auto MTXI              = pTARG->MTXI();
  auto GBI               = pTARG->GBI();

  const SRect& frame_rect = FrameData.GetDstRect();

  int itx0 = frame_rect.miX;
  int itx1 = frame_rect.miX2;
  int ity0 = frame_rect.miY;
  int ity1 = frame_rect.miY2;

  static Texture* pplaytex = ork::asset::AssetManager<ork::lev2::TextureAsset>::Create("lev2://textures/play_icon")->GetTexture();
  static Texture* ppaustex = ork::asset::AssetManager<ork::lev2::TextureAsset>::Create("lev2://textures/pause_icon")->GetTexture();

  /////////////////////////////////////////////////
  lev2::GfxMaterialUI UiMat(pTARG);
  MTXI->PushPMatrix(fmtx4::Identity);
  MTXI->PushVMatrix(fmtx4::Identity);
  MTXI->PushMMatrix(fmtx4::Identity);

  static SRasterState defstate;
  pTARG->RSI()->BindRasterState(defstate);
  {
    /////////////////////////////////////////////////
    // little spinner so i know which window is active
    pTARG->PushModColor(fcolor4::White());

    pTARG->FXI()->InvalidateStateBlock();
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
      pTARG->BindMaterial(&UiMat);
      GBI->DrawPrimitive(vw, lev2::EPRIM_LINES, 2);
      pTARG->BindMaterial(0);
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
      pTARG->BindMaterial(&UiMat);
      GBI->DrawPrimitive(vw, lev2::EPRIM_LINES, 2);
      pTARG->BindMaterial(0);
      MTXI->PopUIMatrix();
    }
    pTARG->PopModColor();
    /////////////////////////////////////////////////
    if (gtoggle_pickbuffer && mEditor.GetActiveSimulation()) {

      ent::ESimulationMode emode = mEditor.GetActiveSimulation()->GetSimulationMode();

      Texture* ptex = (emode == ent::ESCENEMODE_RUN) ? pplaytex : ppaustex;

      static lev2::GfxMaterialUITextured UiMat(pTARG);

      if (mpPickBuffer) {
        if (mpPickBuffer->mpPickRtGroup) {
          auto mrt = mpPickBuffer->mpPickRtGroup->GetMrt(1);
          auto mtl = mrt->GetMaterial();
          ptex     = mrt->GetTexture();
          if (mtl) {
            pTARG->BindMaterial(mtl);
          }
        }
        UiMat.SetTexture(lev2::ETEXDEST_DIFFUSE, ptex);
      } else {
        pTARG->BindMaterial(&UiMat);
      }
      UiMat.mRasterState.SetBlending(lev2::EBLENDING_OFF);
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
        GBI->DrawPrimitive(vw, lev2::EPRIM_TRIANGLES, 6);
        MTXI->PopUIMatrix();
      }
      pTARG->PopModColor();
      pTARG->BindMaterial(0);

      if (emode == ent::ESCENEMODE_RUN) {
        ork::lev2::DrawHudEvent drawHudEvent(pTARG, 1);
        if (mEditor.GetActiveSimulation()->GetApplication())
          mEditor.GetActiveSimulation()->GetApplication()->Notify(&drawHudEvent);
      }
    }
    pTARG->BindMaterial(0);
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

  if (_editorCamera) {
    _editorCamera->draw(pTARG);
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawGrid(ork::lev2::RenderContextFrameData& fdata) {
  auto& GRID = ManipManager().Grid();
  switch (mGridMode) {
    case 0:
      GRID.SetGridMode(lev2::Grid3d::EGRID_XZ);
      GRID.Calc(*fdata.GetCameraData());
      break;
    case 1:
      GRID.SetGridMode(lev2::Grid3d::EGRID_XZ);
      GRID.Calc(*fdata.GetCameraData());
      GRID.Render(fdata);
      break;
    case 2:
      GRID.SetGridMode(lev2::Grid3d::EGRID_XY);
      GRID.Calc(*fdata.GetCameraData());
      GRID.Render(fdata);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawManip(ork::lev2::RenderContextFrameData& fdata, ork::lev2::GfxTarget* pProxyTarg) {
  const CameraData* pcamdata = fdata.GetCameraData();
  if (0 == pcamdata)
    return;
  const CameraCalcContext& ccctx = fdata.GetCameraCalcCtx();

  ork::lev2::GfxTarget* pOutputTarget = fdata.GetTarget();
  auto MTXI                           = pOutputTarget->MTXI();
  MTXI->PushPMatrix(ccctx.mPMatrix);
  MTXI->PushVMatrix(ccctx.mVMatrix);
  MTXI->PushMMatrix(fmtx4::Identity);
  {
    switch (ManipManager().GetUIMode()) {
      case ManipManager::EUIMODE_MANIP_WORLD_TRANSLATE:
      case ManipManager::EUIMODE_MANIP_LOCAL_ROTATE: {
        GetRenderer()->SetTarget(pProxyTarg);
        fdata.SetTarget(pProxyTarg);

        ///////////////////////////////////////
        const fvec4& ScreenXNorm = pProxyTarg->MTXI()->GetScreenRightNormal();

        fmtx4 MatW;
        ManipManager().GetCurTransform().GetMatrix(MatW);
        const fvec4 V0 = MatW.GetTranslation();
        const fvec4 V1 = V0 + ScreenXNorm * float(30.0f);
        fvec2 VP(float(pProxyTarg->GetW()), float(pProxyTarg->GetH()));
        const CameraData* camdat = pProxyTarg->GetRenderContextFrameData()->GetCameraData();
        fvec3 Pos                = MatW.GetTranslation();
        fvec3 UpVector;
        fvec3 RightVector;
        camdat->GetPixelLengthVectors(Pos, VP, UpVector, RightVector);
        float rscale = RightVector.Mag(); // float(100.0f);
        // printf( "OUTERmanip rscale<%f>\n", rscale );

        static float fRSCALE = 1.0f;

        if (fdata.isPicking()) {
          ManipManager().SetViewScale(fRSCALE);
        } else {
          float fW = float(pOutputTarget->GetW());
          float fH = float(pOutputTarget->GetH());
          fRSCALE  = ManipManager().CalcViewScale(fW, fH, camdat);
          ManipManager().SetViewScale(fRSCALE);
        }

        ///////////////////////////////////////

        ManipManager().Setup(GetRenderer());
        ManipManager().Queue(GetRenderer());
        GetRenderer()->SetTarget(pOutputTarget);
        break;
      }
      default:
        break;
    }
  }
  MTXI->PopPMatrix();
  MTXI->PopVMatrix();
  MTXI->PopMMatrix();
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawSpinner(lev2::RenderContextFrameData& FrameData) {
  bool bhasfocus  = HasKeyboardFocus();
  float fw        = FrameData.GetDstRect().miW;
  float fh        = FrameData.GetDstRect().miH;
  auto TGT        = FrameData.GetTarget();
  auto MTXI       = TGT->MTXI();
  ork::fmtx4 mtxP = MTXI->Ortho(0.0f, fw, 0.0f, fh, 0.0f, 1.0f);
  GfxMaterialUI matui(FrameData.GetTarget());
  TGT->BindMaterial(&matui);
  TGT->PushModColor(bhasfocus ? ork::fcolor4::Red() : ork::fcolor4::Black());
  MTXI->PushPMatrix(mtxP);
  MTXI->PushVMatrix(ork::fmtx4::Identity);
  MTXI->PushMMatrix(ork::fmtx4::Identity);
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

    TGT->GBI()->DrawPrimitive(vwriter, ork::lev2::EPRIM_LINES);
  }
  MTXI->PopPMatrix(); // back to ortho
  MTXI->PopVMatrix(); // back to ortho
  MTXI->PopMMatrix(); // back to ortho
  TGT->PopModColor();
  TGT->BindMaterial(0);
}

} // namespace ent
} // namespace ork

///////////////////////////////////////////////////////////////////////////
