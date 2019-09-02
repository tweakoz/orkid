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

#include <ork/reflect/RegisterProperty.h>
#include <orktool/qtui/qtvp_edrenderer.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>

#include <orktool/toolcore/dataflow.h>

///////////////////////////////////////////////////////////////////////////////

#include "qtui_scenevp.h"
#include "qtvp_uievh.h"
#include <pkg/ent/Compositor.h>
#include <pkg/ent/editor/edmainwin.h>

#include <ork/gfx/camera.h>
#include <ork/kernel/future.hpp>
#include <ork/lev2/lev2_asset.h>

#include <pkg/ent/Lighting.h>
#include <pkg/ent/scene.hpp>

///////////////////////////////////////////////////////////////////////////////

extern bool gtoggle_hud;
extern bool gtoggle_pickbuffer;

using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////

template class ork::lev2::CPickBuffer<ork::ent::SceneEditorVP>;

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneEditorView, "SceneEditorView");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneEditorVP, "SceneEditorVP");

namespace ork {
namespace lev2 {
int GetGlError(void);

}
void SetCurrentThreadName(const char* pname);
namespace ent {

#if defined(NDEBUG)
#error
#endif

///////////////////////////////////////////////////////////////////////////////

void SceneEditorView::Describe() {
  reflect::RegisterFunctor("SlotModelDirty", &SceneEditorView::SlotModelDirty);
  reflect::RegisterFunctor("SlotObjectSelected", &SceneEditorView::SlotObjectSelected);
  reflect::RegisterFunctor("SlotObjectDeSelected", &SceneEditorView::SlotObjectDeSelected);
}

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
    : ui::Viewport(name, 1, 1, 1, 1, CColor3(0.0f, 0.0f, 0.0f), 1.0f), mMainWindow(MainWin), miPickDirtyCount(0), mbHeadLight(true),
      mEditor(the_ed), mpBasicFrameTek(0), mpCurrentHandler(0), mpCurrentToolIcon(0), mGridMode(0),
      mRenderer(new ork::tool::Renderer(the_ed)), mSceneView(this), _editorCamera(0), mFramePerfItem("SceneEditorVP::Draw()"),
      miCullCameraIndex(-1), miCameraIndex(0), mCompositorSceneIndex(0), mCompositorSceneItemIndex(0), mbSceneDisplayEnable(false),
      mUpdateThread(nullptr) {
  mRenderLock = 0;
  ork::event::Broadcaster& bcaster = ork::event::Broadcaster::GetRef();
  bcaster.AddListenerOnChannel(this, ork::ent::SceneInst::EventChannel());

  ///////////////////////////////////////////////////////////

  ent::DrawableBuffer::ClearAndSyncWriters();

  ///////////////////////////////////////////////////////////

  mpBasicFrameTek = new BasicFrameTechnique();

  PushFrameTechnique(mpBasicFrameTek);

#if defined(_THREADED_RENDERER)
  mUpdateThread = new UpdateThread(this);
  mUpdateThread->start();
#endif
}

///////////////////////////////////////////////////////////////////////////

SceneEditorVP::~SceneEditorVP() {
  delete mUpdateThread;

  ork::event::Broadcaster& bcaster = ork::event::Broadcaster::GetRef();
  bcaster.RemoveListenerOnChannel(this, ork::ent::SceneInst::EventChannel());
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::Init() {
  ((ork::tool::Renderer*)(mRenderer))->Init();

  ///////////////////////////////////////////////////////////
  // INIT BUILT IN TOOL HANDLERS

  ork::msleep(500);
  RegisterToolHandler("0Default", new TestVPDefaultHandler(mEditor));
  RegisterToolHandler("1ManipTrans", new ManipTransHandler(mEditor));
  RegisterToolHandler("2ManipRot", new ManipRotHandler(mEditor));
  // RegisterToolHandler( "3Collision", new PaintCollisionHandler( this, mEditor ) );
  // RegisterToolHandler( "4Bsp", new BspToolHandler( this, mEditor ) );

  mpDefaultHandler = mToolHandlers["0Default"];
  bindToolHandler("0Default");

  ///////////////////////////////////////////////////////////
  // INIT MODULAR TOOL HANDLERS

  for (orkset<SceneEditorInitCb>::iterator it = mInitCallbacks.begin(); it != mInitCallbacks.end(); it++) {
    (*it)(*this);
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::RegisterInitCallback(ork::ent::SceneEditorInitCb icb) { mInitCallbacks.insert(icb); }

///////////////////////////////////////////////////////////////////////////////

void SceneEditorView::SlotObjectSelected(ork::Object* pobj) {
  EntData* pdata = rtti::autocast(pobj);
  if (pdata) {
    fvec3 pos = pdata->GetDagNode().GetTransformNode().GetTransform().GetPosition();
    // if( mVP->GetCamera() )
    //	mVP->GetCamera()->CamFocus = pos;
    // mVP->mpPickBuffer->SetDirty(true);
  }
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorView::SlotObjectDeSelected(ork::Object* pobj) {}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::IncPickDirtyCount(int icount) { mpPickBuffer->SetDirty(true); }

void SceneEditorView::SlotModelDirty() { mVP->IncPickDirtyCount(1); }

///////////////////////////////////////////////////////////////////////////

SceneEditorView::SceneEditorView(SceneEditorVP* vp) : mVP(vp) {}
///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DoInit(ork::lev2::GfxTarget* pTARG) {
  mpPickBuffer = new lev2::CPickBuffer<SceneEditorVP>(pTARG->FBI()->GetThisBuffer(), this, 0, 0, 1024, 1024,
                                                      lev2::PickBufferBase::EPICK_FACE_VTX);
  mpPickBuffer->RefClearColor().SetRGBAU32(0);
  mpPickBuffer->CreateContext();
  mpPickBuffer->GetContext()->FBI()->SetClearColor(fcolor4(0.0f, 0.0f, 0.0f, 0.0f));
  int iw = pTARG->GetW();
  int ih = pTARG->GetH();

  pTARG->FBI()->SetClearColor(fcolor4(0.0f, 0.0f, 0.0f, 0.0f));

  orkprintf("PickBuffer<%p>\n", mpPickBuffer);
}

///////////////////////////////////////////////////////////////////////////
// Draw INTO the onscreen target
///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DoDraw(ui::DrawEvent& drwev) {
  bool update_running = gUpdateStatus.GetState() == EUPD_RUNNING;

  // printf( "SceneEditorVP::DoDraw() updrun<%d>\n", int(update_running) );

  // if( false == update_running ) return;

  const SRect tgtrect = SRect(0, 0, mpTarget->GetW(), mpTarget->GetH());
  FrameRenderer the_renderer(this);
  lev2::UiViewportRenderTarget rt(this);
  the_renderer.GetFrameData().SetDstRect(tgtrect);
  mRenderer->SetTarget(mpTarget);
  the_renderer.GetFrameData().SetTarget(mpTarget);

  /////////////////////////////////////////////////////////////////////////////////

  mRenderLock = 1;

  bool bFX = false;
  auto pCMCI = GetCMCI();
  if (GetSceneInst()) {
    ent::ESceneInstMode emode = GetSceneInst()->GetSceneInstMode();
    if (pCMCI)
      switch (emode) {
        case ent::ESCENEMODE_RUN:
        case ent::ESCENEMODE_SINGLESTEP: {
          bFX = pCMCI->IsEnabled();
          break;
        }
        default:
          break;
      }
  }

  mRenderLock = 0;

  /////////////////////////////////////////////////////////////////////////////////

  the_renderer.GetFrameData().PushRenderTarget(&rt);
  {
    /////////////////////////////////
    // Compositor ?
    /////////////////////////////////

    if (bFX && pCMCI) {
      float frame_rate = pCMCI ? pCMCI->GetCurrentFrameRate() : 0.0f;
      bool externally_fixed_rate = (frame_rate != 0.0f);
      const ent::SceneInst* psi = this->GetSceneInst();

      RenderSyncToken syntok;
      /////////////////////////////
      bool have_token = false;
      if (externally_fixed_rate) {
        Timer totim;
        totim.Start();
        while (false == have_token && (totim.SecsSinceStart() < 2.0f)) {
          have_token = ent::DrawableBuffer::mOfflineRenderSynchro.try_pop(syntok);
          usleep(1000);
        }
      }
      /////////////////////////////
      // render it
      /////////////////////////////

      ent::CompositorSystemDrawData compositorDrawData(the_renderer);
      pCMCI->Draw(compositorDrawData);

      ////////////////////////////////////////////
      // FrameTechnique FinalMRT + HUD -> screen
      ////////////////////////////////////////////
      the_renderer.GetFrameData().PushRenderTarget(&rt);
      if (externally_fixed_rate && have_token) {
        ////////////////////////////////////////
        // setup destination buffer as offscreen buffer
        //  (for write to disk)
        ////////////////////////////////////////

        int itw = mpTarget->GetW();
        int ith = mpTarget->GetH();

        the_renderer.GetFrameData().SetTarget(mpTarget);
        mRenderer->SetTarget(mpTarget);
        the_renderer.GetFrameData().SetDstRect(tgtrect);
        mpTarget->FBI()->SetAutoClear(true);
        mpTarget->BeginFrame();
        pCMCI->composeToScreen(mpTarget);
        mpTarget->EndFrame(); // the_renderer );
        ////////////////////////////////////////
        // write to disk
        ////////////////////////////////////////
        auto buf = mpTarget->FBI()->GetThisBuffer();
        file::Path::NameType fnamesyn;
        fnamesyn.format("outputframes/frame%04d.tga", syntok.mFrameIndex);
        // mpTarget->FBI()->Capture( *buf, file::Path(fnamesyn.c_str()) );
        ////////////////////////////////////////

        ////////////////////////////////////////
        // return the token
        ////////////////////////////////////////

        DrawableBuffer::mOfflineUpdateSynchro.push(syntok);

      } else {
        the_renderer.GetFrameData().SetTarget(mpTarget);
        mRenderer->SetTarget(mpTarget);
        the_renderer.GetFrameData().SetDstRect(tgtrect);
        mpTarget->FBI()->SetAutoClear(true);

        mpTarget->FBI()->SetViewport(0, 0, mpTarget->GetW(), mpTarget->GetH());
        mpTarget->FBI()->SetScissor(0, 0, mpTarget->GetW(), mpTarget->GetH());
        mpTarget->BeginFrame();
        pCMCI->composeToScreen(mpTarget);
        /////////////////////////////////////////////////////////////////////
        // HUD
        /////////////////////////////////////////////////////////////////////

        if (gtoggle_hud) {
          DrawHUD(the_renderer.GetFrameData());
          DrawChildren(drwev);
        }

        /////////////////////////////////////////////////////////////////////
        mpTarget->EndFrame(); // the_renderer );
      }
      the_renderer.GetFrameData().PopRenderTarget();
    }
    /////////////////////////////////
    else // No Compositor
    /////////////////////////////////
    {
      const ent::DrawableBuffer* DB = DrawableBuffer::BeginDbRead(7); // mDbLock.Aquire(7);

      mRenderLock = 1;

      the_renderer.GetFrameData().SetUserProperty("DB", anyp(DB));

      if (DB) {

        anyp PassData;
        PassData.Set<orkstack<ent::CompositingPassData>*>(&mCompositingGroupStack);
        the_renderer.GetFrameData().SetUserProperty("nodes", PassData);
        the_renderer.GetFrameData().PushRenderTarget(&rt);
        the_renderer.GetFrameData().SetTarget(mpTarget);
        mRenderer->SetTarget(mpTarget);
        the_renderer.GetFrameData().SetDstRect(tgtrect);
        mpTarget->FBI()->SetAutoClear(true);
        mpTarget->FBI()->SetViewport(0, 0, mpTarget->GetW(), mpTarget->GetH());
        mpTarget->FBI()->SetScissor(0, 0, mpTarget->GetW(), mpTarget->GetH());
        mpTarget->BeginFrame();
        {
          CompositingPassData node;
          node.mpGroup = nullptr;
          node.mpFrameTek = nullptr;
          mCompositingGroupStack.push(node);
          mpBasicFrameTek->mbDoBeginEndFrame = false;
          mpBasicFrameTek->Render(the_renderer);
          mCompositingGroupStack.pop();

          if (gtoggle_hud) {
            DrawHUD(the_renderer.GetFrameData());
            DrawChildren(drwev);
          }
        }
        mpTarget->EndFrame(); // the_renderer );
        the_renderer.GetFrameData().PopRenderTarget();

        DrawableBuffer::EndDbRead(DB);
      }

      mRenderLock = 0;
    }
  }
  the_renderer.GetFrameData().PopRenderTarget();

  the_renderer.GetFrameData().SetDstRect(tgtrect);
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::FrameRenderer::Render() {
  switch (GetFrameData().GetRenderingMode()) {
    case ork::lev2::RenderContextFrameData::ERENDMODE_STANDARD:
    case ork::lev2::RenderContextFrameData::ERENDMODE_LIGHTPREPASS:
      mpViewport->Draw3dContent(GetFrameData());
      break;
    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////

ent::CompositingSystem* SceneEditorVP::GetCMCI() {
  auto psi = mEditor.GetActiveSceneInst();
  return (psi != nullptr) ? psi->GetCMCI() : nullptr;
}

///////////////////////////////////////////////////////////////////////////
// Draw 3d content for the editor
// this will always eventually go to the onscreen target
//  though it MAY go thru an intermediate rendertarget first
///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::Draw3dContent(lev2::RenderContextFrameData& FrameData) {
  if (false == mbSceneDisplayEnable)
    return;

  lev2::GfxTarget* pTARG = FrameData.GetTarget();
  ///////////////////////////////////////////////////////////////////////////
  mFramePerfItem.Enter();
  ///////////////////////////////////////////////////////////////////////////
  lev2::IRenderTarget* pIT = FrameData.GetRenderTarget();
  ///////////////////////////////////////////////////////////////////////////
  const SRect& frame_rect = FrameData.GetDstRect();
  int ix = pTARG->GetX();
  int iy = pTARG->GetY();
  int iw = pTARG->GetW();
  int ih = pTARG->GetH();
  SetRect(ix, iy, iw, ih);
  ///////////////////////////////////////////////////////////////////////////
  float fW = float(pTARG->GetW());
  float fH = float(pTARG->GetH());
  FrameData.GetCameraCalcCtx().mfAspectRatio = fW / fH;
  ///////////////////////////////////////////////////////////////////////////
  ent::SceneData* pscene = mEditor.mpScene;

  anyp PassData = FrameData.GetUserProperty("nodes");
  orkstack<ent::CompositingPassData>* cstack = 0;
  cstack = PassData.Get<orkstack<ent::CompositingPassData>*>();
  OrkAssert(cstack != 0);

  CompositingPassData node = cstack->top();
  auto pFTEK = dynamic_cast<lev2::BuiltinFrameTechniques*>(node.mpFrameTek);
  ///////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////
  if (pFTEK) {
    const char* EffectName = "none";
    float fFxAmt = 0.0f;
    float fFbAmt = 0.0f;
    float fFinResMult = 0.5f;
    float fFxResMult = 0.5f;
    lev2::Texture* pFbUvMap = nullptr;
    bool bpostfxfb = false;

    if (node.mpGroup) {
      const CompositingGroupEffect& effect = node.mpGroup->GetEffect();
      EffectName = effect.GetEffectName();
      fFxAmt = effect.GetEffectAmount();
      fFbAmt = effect.GetFeedbackAmount();
      pFbUvMap = effect.GetFbUvMap();
      bpostfxfb = effect.IsPostFxFeedback();
      fFinResMult = effect.GetFinalRezScale();
      fFxResMult = effect.GetFxRezScale();
    }

    ////////////////////////////////////////

    pFTEK->SetEffect(EffectName, fFxAmt, fFbAmt);
    pFTEK->SetFbUvMap(pFbUvMap);
    pFTEK->SetPostFxFb(bpostfxfb);

    ////////////////////////////////////////
    // set buffer sizes
    ////////////////////////////////////////

    int ifxW = int(fFxResMult * fW);
    int ifxH = int(fFxResMult * fH);
    int ifinalW = int(fFinResMult * fW);
    int ifinalH = int(fFinResMult * fH);
    pFTEK->ResizeFinalBuffer(ifinalW, ifinalH);
    pFTEK->ResizeFxBuffer(ifxW, ifxH);

    ////////////////////////////////////////
  }
  ///////////////////////////////////////////////////////////////////////////
  SRect VPRect(0, 0, pIT->GetW(), pIT->GetH());

  pTARG->FBI()->PushViewport(VPRect);
  pTARG->FBI()->PushScissor(VPRect);
  {
    mRenderer->SetTarget(pTARG);
    FrameData.AddLayer(AddPooledLiteral("Default"));
    FrameData.AddLayer(AddPooledLiteral("A"));
    FrameData.AddLayer(AddPooledLiteral("B"));
    FrameData.AddLayer(AddPooledLiteral("C"));
    FrameData.AddLayer(AddPooledLiteral("D"));
    FrameData.AddLayer(AddPooledLiteral("E"));
    FrameData.AddLayer(AddPooledLiteral("F"));
    FrameData.AddLayer(AddPooledLiteral("G"));
    FrameData.AddLayer(AddPooledLiteral("H"));
    FrameData.AddLayer(AddPooledLiteral("I"));
    FrameData.AddLayer(AddPooledLiteral("J"));
    FrameData.AddLayer(AddPooledLiteral("K"));
    FrameData.AddLayer(AddPooledLiteral("L"));
    FrameData.AddLayer(AddPooledLiteral("M"));
    FrameData.AddLayer(AddPooledLiteral("N"));
    FrameData.AddLayer(AddPooledLiteral("O"));
    FrameData.AddLayer(AddPooledLiteral("P"));
    FrameData.AddLayer(AddPooledLiteral("Q"));

    const ent::SceneInst* psi = GetSceneInst();
    mSceneView.UpdateRefreshPolicy(FrameData, psi);

    this->GetClearColorRef() = node._clearColor.xyz();

    this->Clear();
    if (node.mbDrawSource)
      RenderQueuedScene(FrameData);
  };
  pTARG->FBI()->PopScissor();
  pTARG->FBI()->PopViewport();
  ///////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////
  // filth up the pick buffer
  if (miPickDirtyCount > 0) {
    if (mpPickBuffer) {
      mpPickBuffer->SetDirty(true);
      miPickDirtyCount--;
    }
  }
  ///////////////////////////////////////////////////////
  mFramePerfItem.Exit();
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::QueueSceneInstToDb(ent::DrawableBuffer* pDB) // Queue SceneDrawLayerData
{
  AssertOnOpQ2(UpdateSerialOpQ());

  if (GetSceneInst())
    GetSceneInst()->QueueAllDrawablesToBuffer(*pDB);
}

///////////////////////////////////////////////////////////////////////////////

const CompositingGroup* SceneEditorVP::GetCompositingGroup(int igrp) {
  const CompositingGroup* pCG = 0;
  if (ent::CompositingSystem* pCMCI = GetCMCI()) {
    const CompositingSystemData& CCD = pCMCI->systemData();
    auto& Groups = CCD.GetGroups();
    int inumgroups = Groups.size();
    if (inumgroups && igrp >= 0) {
      int idx = igrp % inumgroups;
      ork::Object* pOBJ = Groups.GetItemAtIndex(idx).second;
      if (pOBJ)
        pCG = rtti::autocast(pOBJ);
    }
  }
  return pCG;
}

///////////////////////////////////////////////////////////////////////////////

const ent::SceneInst* SceneEditorVP::GetSceneInst() {
  const ent::SceneInst* psi = 0;
  if (mEditor.mpScene) {
    if (ApplicationStack::Top()) {
      psi = mEditor.GetActiveSceneInst();
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

void SceneEditorVP::RenderQueuedScene(lev2::RenderContextFrameData& FrameData) {
  GL_ERRORCHECK();
  lev2::IRenderTarget* pIRT = FrameData.GetRenderTarget();
  bool IsPickState = FrameData.GetTarget()->FBI()->IsPickState();
  const bool forgepickstate = false;
  if (forgepickstate)
    FrameData.GetTarget()->FBI()->EnterPickState(mpPickBuffer);
  // FrameData.GetTarget()->FBI()->ForceFlush();
  GL_ERRORCHECK();
  ///////////////////////////////////////////////////////////////////////////
  // get the compositor if there is one
  ///////////////////////////////////////////////////////////////////////////

  anyp PassData = FrameData.GetUserProperty("nodes");
  orkstack<ent::CompositingPassData>* cstack = 0;
  cstack = PassData.Get<orkstack<ent::CompositingPassData>*>();
  OrkAssert(cstack != 0);

  CompositingPassData NODE = cstack->top();

  ent::CompositingSystem* pCMCI = GetCMCI();

  ///////////////////////////////////////////////////////////////////////////
  // camera setup
  ///////////////////////////////////////////////////////////////////////////

  CameraData TempCamData, TempCullCamData;
  _editorCamera = 0;

  anyp pvdb = FrameData.GetUserProperty("DB");
  const DrawableBuffer* DB = pvdb.Get<const DrawableBuffer*>();
  if (0 == DB)
    return;

  const CameraData* pcamdata = DB->GetCameraData(miCameraIndex);
  const CameraData* pcullcamdata = DB->GetCameraData(miCullCameraIndex);
  // FrameData.GetTarget()->FBI()->ForceFlush();

  if (nullptr == pcamdata)
    return;

  /////////////////////////////////////////
  // Culling camera ? (for debug)
  /////////////////////////////////////////

  if (pcullcamdata) {
    TempCullCamData = *pcullcamdata;
    TempCullCamData.BindGfxTarget(FrameData.GetTarget());
    TempCullCamData.CalcCameraData(FrameData.GetCameraCalcCtx());
    TempCamData.SetVisibilityCamDat(&TempCullCamData);
  }

  /////////////////////////////////////////
  // try named CameraData from NODE
  /////////////////////////////////////////

  if (NODE.mpCameraName) {
    const CameraData* pcamdataNAMED = DB->GetCameraData(*NODE.mpCameraName);
    if (pcamdataNAMED)
      pcamdata = pcamdataNAMED;
  }

  /////////////////////////////////////////
  // try direct CameraData from NODE
  /////////////////////////////////////////

  if (auto from_node = NODE._impl.TryAs<const CameraData*>()) {
    pcamdata = from_node.value();
    // printf( "from node\n");
  }

  /////////////////////////////////////////
  // generate temporary CamData from input
  //  bind to FrameData target
  /////////////////////////////////////////

  if (pcamdata) {
    TempCamData = *pcamdata;
    TempCamData.BindGfxTarget(FrameData.GetTarget());
    TempCamData.CalcCameraData(FrameData.GetCameraCalcCtx());
  }
  FrameData.SetCameraData(&TempCamData);

  /////////////////////////////////////////
  // editor camera renderupdate
  /////////////////////////////////////////
  if (pcamdata)
    _editorCamera = pcamdata->getEditorCamera();
  if (_editorCamera) {
    _editorCamera->AttachViewport(this);
    _editorCamera->RenderUpdate();
  }
  ManipManager().SetActiveCamera(_editorCamera);
  /////////////////////////////////////////

  if (0 == pcamdata)
    return;

  ///////////////////////////////////////////////////////////////////////////
  lev2::HeadLightManager hlmgr(FrameData);
  SetupLighting(hlmgr, FrameData);
  ///////////////////////////////////////////////////////////////////////////
  auto gfxtarg = FrameData.GetTarget();
  auto MTXI = gfxtarg->MTXI();
  auto FBI = gfxtarg->FBI();
  ///////////////////////////////////////////////////////////////////////////
  FBI->GetThisBuffer()->SetDirty(false);
  ///////////////////////////////////////////////////////////////////////////
  gfxtarg->BindMaterial(lev2::GfxEnv::GetDefault3DMaterial());
  GL_ERRORCHECK();
  /////////////////////////////////////////////////////////////////////////////
  const fmtx4& PMTX = FrameData.GetCameraCalcCtx().mPMatrix;
  const fmtx4& VMTX = FrameData.GetCameraCalcCtx().mVMatrix;
  /////////////////////////////////////////////////////////////////////////////
  // Main Renderer
  {
    static lev2::SRasterState defstate;
    gfxtarg->RSI()->BindRasterState(defstate, true);

    // VMTX.dump("VMTX");

    MTXI->PushPMatrix(PMTX);
    MTXI->PushVMatrix(VMTX);
    MTXI->PushMMatrix(fmtx4::Identity);
    { /////////////////////////////////////////
      // manip
      /////////////////////////////////////////
      if (mEditor.mpScene)
        DrawManip(FrameData, gfxtarg);
      /////////////////////////////////////////
      // grid
      /////////////////////////////////////////
      if (false == IsPickState)
        DrawGrid(FrameData);
      /////////////////////////////////////////
      // RenderQueue
      /////////////////////////////////////////
      std::vector<PoolString> LayerNames;
      if (NODE.mpLayerName) {
        const char* playersstr = NODE.mpLayerName->c_str();
        if (playersstr) {
          char temp_buf[256];
          strncpy(&temp_buf[0], playersstr, sizeof(temp_buf));
          char* tok = strtok(&temp_buf[0], ",");
          while (tok != 0) {
            LayerNames.push_back(AddPooledString(tok));
            tok = strtok(0, ",");
          }
        }
      } else {
        LayerNames.push_back(AddPooledLiteral("All"));
      }
      // printf( "USING LAYERNAME<%s>\n", LayerName.c_str() );
      // const DrawableBuffer& DB = DrawableBuffer::GetLockedReadBuffer(0);
      auto psi = GetSceneInst();
      auto rend = GetRenderer();
      for (const PoolString& layer_name : LayerNames)
        psi->RenderDrawableBuffer(rend, *DB, layer_name);
      rend->DrawQueuedRenderables();
      /////////////////////////////////////////
    }
    MTXI->PopPMatrix(); // back to ortho
    MTXI->PopVMatrix(); // back to ortho
    MTXI->PopMMatrix(); // back to ortho
    /////////////////////////////////////////
    // draw Spinner
    /////////////////////////////////////////
    if (false == IsPickState)
      DrawSpinner(FrameData);
  }
  if (forgepickstate)
    FBI->EnterPickState(mpPickBuffer);
  // FrameData.GetTarget()->SetRenderContextFrameData( 0 );
  FrameData.SetLightManager(0);
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorView::UpdateRefreshPolicy(lev2::RenderContextFrameData& FrameData, const SceneInst* sinst) {
  if (0 == sinst)
    return;

  ///////////////////////////////////////////////////////////
  // refresh control

  lev2::CTXBASE* pctxb = FrameData.GetTarget()->GetCtxBase();

  lev2::GfxTarget* ptarg = FrameData.GetTarget();

  static orkstack<ent::ESceneInstMode> semodestack;

  if (pctxb) {
    ent::ESceneInstMode ecurmode = semodestack.size() ? semodestack.top() : ent::ESCENEMODE_EDIT;
    ent::ESceneInstMode enewmode = sinst->GetSceneInstMode();

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

  const SRect& frame_rect = FrameData.GetDstRect();

  int itx0 = frame_rect.miX;
  int itx1 = frame_rect.miX2;
  int ity0 = frame_rect.miY;
  int ity1 = frame_rect.miY2;

  static Texture* pplaytex = ork::asset::AssetManager<ork::lev2::TextureAsset>::Create("lev2://textures/play_icon")->GetTexture();
  static Texture* ppaustex = ork::asset::AssetManager<ork::lev2::TextureAsset>::Create("lev2://textures/pause_icon")->GetTexture();

  /////////////////////////////////////////////////
  lev2::GfxMaterialUI UiMat(pTARG);
  pTARG->MTXI()->PushPMatrix(fmtx4::Identity);
  pTARG->MTXI()->PushVMatrix(fmtx4::Identity);
  pTARG->MTXI()->PushMMatrix(fmtx4::Identity);

  static SRasterState defstate;
  pTARG->RSI()->BindRasterState(defstate);
  {
    /////////////////////////////////////////////////
    // little spinner so i know which window is active
    pTARG->PushModColor(fcolor4::White());

    pTARG->FXI()->InvalidateStateBlock();
    {
      static float gfspinner = 0.0f;
      float fx = sinf(gfspinner);
      float fy = cosf(gfspinner);

      int ilen = 16;
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
      pTARG->MTXI()->PushUIMatrix();
      pTARG->BindMaterial(&UiMat);
      pTARG->GBI()->DrawPrimitive(vw, lev2::EPRIM_LINES, 2);
      pTARG->BindMaterial(0);
      pTARG->MTXI()->PopUIMatrix();
      gfspinner += (PI2 / 60.0f);
    }
    pTARG->PopModColor();
    pTARG->PushModColor(fcolor4::Yellow());
    {

      float gfspinner = OldSchool::GetRef().GetLoResRelTime() * PI;
      float fx = sinf(gfspinner);
      float fy = cosf(gfspinner);

      int ilen = 16;
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
      pTARG->MTXI()->PushUIMatrix();
      pTARG->BindMaterial(&UiMat);
      pTARG->GBI()->DrawPrimitive(vw, lev2::EPRIM_LINES, 2);
      pTARG->BindMaterial(0);
      pTARG->MTXI()->PopUIMatrix();
    }
    pTARG->PopModColor();
    /////////////////////////////////////////////////
    if (gtoggle_pickbuffer && mEditor.GetActiveSceneInst()) {

      ent::ESceneInstMode emode = mEditor.GetActiveSceneInst()->GetSceneInstMode();

      Texture* ptex = (emode == ent::ESCENEMODE_RUN) ? pplaytex : ppaustex;

      static lev2::GfxMaterialUITextured UiMat(pTARG);

      if (mpPickBuffer) {
        if (mpPickBuffer->mpPickRtGroup) {
					auto mrt = mpPickBuffer->mpPickRtGroup->GetMrt(1);
					auto mtl = mrt->GetMaterial();
          ptex = mrt->GetTexture();
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
          float fZ = 0.0f;
          u32 ucolor = 0xffffffff;
          f32 x0 = 4.0f;
          f32 y0 = f32(ity1 - ksize);
          f32 w0 = f32(ksize + 4);
          f32 h0 = f32(ksize - 18);
          f32 x1 = x0 + w0;
          f32 y1 = y0 + h0;
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
        pTARG->MTXI()->PushUIMatrix();
        pTARG->GBI()->DrawPrimitive(vw, lev2::EPRIM_TRIANGLES, 6);
        pTARG->MTXI()->PopUIMatrix();
      }
      pTARG->PopModColor();
      pTARG->BindMaterial(0);

      if (emode == ent::ESCENEMODE_RUN) {
        ork::lev2::DrawHudEvent drawHudEvent(pTARG, 1);
        if (mEditor.GetActiveSceneInst()->GetApplication())
          mEditor.GetActiveSceneInst()->GetApplication()->Notify(&drawHudEvent);
      }
    }
    pTARG->BindMaterial(0);
    /////////////////////////////////////////////////
    /////////////////////////////////////////////////
    /////////////////////////////////////////////////
    int iy = 4;
    int ix = 4;
    for (auto it : mToolHandlers ) {
      SceneEditorVPToolHandler* phandler = it.second;
      bool bhilite = (phandler == mpCurrentHandler);
      phandler->DrawToolIcon(pTARG, ix, iy, bhilite);
      iy += 36;
    }
    /////////////////////////////////////////////////
    /////////////////////////////////////////////////
    // PerformanceTracker::GetRef().Draw(pTARG);
    /////////////////////////////////////////////////
    /////////////////////////////////////////////////
  }

  pTARG->MTXI()->PopPMatrix(); // back to ortho
  pTARG->MTXI()->PopVMatrix(); // back to ortho
  pTARG->MTXI()->PopMMatrix(); // back to ortho

  if (_editorCamera) {
    _editorCamera->draw(pTARG);
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawGrid(ork::lev2::RenderContextFrameData& fdata) {
  switch (mGridMode) {
    case 0:
      ManipManager().Grid().SetGridMode(lev2::Grid3d::EGRID_XZ);
      ManipManager().Grid().Calc(*fdata.GetCameraData());
      break;
    case 1:
      ManipManager().Grid().SetGridMode(lev2::Grid3d::EGRID_XZ);
      ManipManager().Grid().Calc(*fdata.GetCameraData());
      ManipManager().Grid().Render(fdata);
      break;
    case 2:
      ManipManager().Grid().SetGridMode(lev2::Grid3d::EGRID_XY);
      ManipManager().Grid().Calc(*fdata.GetCameraData());
      ManipManager().Grid().Render(fdata);
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
  auto MTXI = pOutputTarget->MTXI();
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
        fvec3 Pos = MatW.GetTranslation();
        fvec3 UpVector;
        fvec3 RightVector;
        camdat->GetPixelLengthVectors(Pos, VP, UpVector, RightVector);
        float rscale = RightVector.Mag(); // float(100.0f);
        // printf( "OUTERmanip rscale<%f>\n", rscale );

        static float fRSCALE = 1.0f;

        if (fdata.IsPickMode()) {
          ManipManager().SetViewScale(fRSCALE);
        } else {
          float fW = float(pOutputTarget->GetW());
          float fH = float(pOutputTarget->GetH());
          fRSCALE = ManipManager().CalcViewScale(fW, fH, camdat);
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

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::SetupLighting(lev2::HeadLightManager& hlmgr, lev2::RenderContextFrameData& FrameData) {
  ///////////////////////////////////////////////////////////
  // setup headlight
  ///////////////////////////////////////////////////////////
  FrameData.SetLightManager(&hlmgr.mHeadLightManager);
  ///////////////////////////////////////////////////////////
  // override with lightmanager in scene if one exists
  ///////////////////////////////////////////////////////////
  if (mEditor.mpScene) {
    if (mEditor.GetActiveSceneInst()) {
      if (auto lmi = mEditor.GetActiveSceneInst()->findSystem<ent::LightingSystem>()) {
        ork::lev2::LightManager& lightmanager = lmi->GetLightManager();
        const CameraData* cdata = FrameData.GetCameraData();
        lightmanager.EnumerateInFrustum(cdata->GetFrustum());
        if (lightmanager.mLightsInFrustum.size()) {
          FrameData.SetLightManager(&lightmanager);
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawSpinner(lev2::RenderContextFrameData& FrameData) {
  bool bhasfocus = HasKeyboardFocus();
  float fw = FrameData.GetDstRect().miW;
  float fh = FrameData.GetDstRect().miH;
  ork::fmtx4 mtxP = FrameData.GetTarget()->MTXI()->Ortho(0.0f, fw, 0.0f, fh, 0.0f, 1.0f);
  // GfxEnv::SetUIColorMode( ork::lev2::EUICOLOR_MOD );
  GfxMaterialUI matui(FrameData.GetTarget());
  FrameData.GetTarget()->BindMaterial(&matui);
  FrameData.GetTarget()->PushModColor(bhasfocus ? ork::fcolor4::Red() : ork::fcolor4::Black());
  FrameData.GetTarget()->MTXI()->PushPMatrix(mtxP);
  FrameData.GetTarget()->MTXI()->PushVMatrix(ork::fmtx4::Identity);
  FrameData.GetTarget()->MTXI()->PushMMatrix(ork::fmtx4::Identity);
  {
    DynamicVertexBuffer<SVtxV12C4T16>& vb = GfxEnv::GetSharedDynamicVB();

    int ivcount = 16;

    VtxWriter<SVtxV12C4T16> vwriter;
    vwriter.Lock(FrameData.GetTarget(), &vb, ivcount);

    float fx1 = fw - 1.0f;
    float fy1 = fh - 1.0f;

    vwriter.AddVertex(SVtxV12C4T16(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(SVtxV12C4T16(fx1, 0.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(SVtxV12C4T16(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(SVtxV12C4T16(fx1, 1.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));

    vwriter.AddVertex(SVtxV12C4T16(fx1, 0.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(SVtxV12C4T16(fx1 - 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(SVtxV12C4T16(fx1 - 1.0f, fy1, 0.0f, 0.0f, 0.0f, 0xffffffff));

    vwriter.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(SVtxV12C4T16(0.0f, fy1, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(SVtxV12C4T16(fx1, fy1 - 1.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(SVtxV12C4T16(0.0f, fy1 - 1.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));

    vwriter.AddVertex(SVtxV12C4T16(0.0f, fy1, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(SVtxV12C4T16(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(SVtxV12C4T16(0.0f, fy1, 0.0f, 0.0f, 0.0f, 0xffffffff));
    vwriter.AddVertex(SVtxV12C4T16(1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0xffffffff));

    vwriter.UnLock(FrameData.GetTarget());

    FrameData.GetTarget()->GBI()->DrawPrimitive(vwriter, ork::lev2::EPRIM_LINES);
  }
  FrameData.GetTarget()->MTXI()->PopPMatrix(); // back to ortho
  FrameData.GetTarget()->MTXI()->PopVMatrix(); // back to ortho
  FrameData.GetTarget()->MTXI()->PopMMatrix(); // back to ortho
  FrameData.GetTarget()->PopModColor();
  FrameData.GetTarget()->BindMaterial(0);
}

} // namespace ent
} // namespace ork

///////////////////////////////////////////////////////////////////////////
