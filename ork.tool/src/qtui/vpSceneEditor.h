////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/thread.h>
#include <ork/kernel/timer.h>
#include <ork/object/Object.h>
#include <ork/kernel/msgrouter.inl>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/editor/editor.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/CompositingSystem.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/qtui/qtmainwin.h>
///////////////////////////////////////////////////////////////////////////////

class QWidget;

///////////////////////////////////////////////////////////////////////////////
namespace ork {
namespace lev2 {

class GfxMaterialUITextured;
class RenderContextFrameData;
class FrameTechniqueBase;
class IRenderer;
class EzUiCam;
class Camera_ortho;
class UiCamera;
class CompositingGroup;
class CompositingSceneItem;

} // namespace lev2

///////////////////////////////////////////////////////////////////////////////
namespace tool {
template <typename T> class PickBuffer;
}
///////////////////////////////////////////////////////////////////////////////
namespace ent {
///////////////////////////////////////////////////////////////////////////////
class SceneEditorVP;
class EditorSimulation;
class EditorMainWindow;
class CompositingSystem;
///////////////////////////////////////////////////////////////////////////////
typedef void (*SceneEditorInitCb)(ork::ent::SceneEditorVP& vped);

class SceneEditorVPToolHandler; // : public SceneEditorVPToolHandlerBase;

///////////////////////////////////////////////////////////////////////////////
class UpdateThread : public ork::Thread {
public:
  UpdateThread(SceneEditorVP* pVP);
  ~UpdateThread();

private:
  void run() override;
  SceneEditorVP* mpVP;
  bool mbOKTOEXIT;
  bool mbEXITING;
  ork::opq::opq_ptr_t _updq;
};
///////////////////////////////////////////////////////////////////////////////

class SceneEditorView : public ork::Object {
  RttiDeclareAbstract(SceneEditorView, Object);

  SceneEditorVP* mVP;

  //////////////////////////////////////////////////////////

  void SlotModelDirty();

  //////////////////////////////////////////////////////////

public:
  SceneEditorView(SceneEditorVP* vp);
};

///////////////////////////////////////////////////////////////////////////
struct ScenePickBuffer : public ork::lev2::PickBuffer {
  ScenePickBuffer(SceneEditorVP* vp, ork::lev2::Context* ctx);
  void Draw(lev2::PixelFetchContext& ctx) final;
  SceneEditorVP* _scenevp;
};
///////////////////////////////////////////////////////////////////////////
class SceneEditorVP : public ui::Viewport {
  RttiDeclareAbstract(SceneEditorVP, ui::Viewport);
  friend class ScenePickBuffer;
public:
  SceneEditorVP(const std::string& name, SceneEditorBase& Editor, EditorMainWindow& MainWin);
  ~SceneEditorVP();
  void UpdateRefreshPolicy();

  //////////////////////
  ui::HandlerResult DoOnUiEvent(const ui::Event& EV) override;
  //////////////////////
  void renderMisc(ork::lev2::RenderContextFrameData& RCFD);
  //////////////////////
  void IncPickDirtyCount(int icount);
  void GetPixel(int ix, int iy, lev2::PixelFetchContext& ctx);
  ork::Object* GetObject(lev2::PixelFetchContext& ctx, int ichan);
  //////////////////////
  ent::CompositingSystem* compositingSystem();
  const lev2::CompositingGroup* GetCompositingGroup(int igrp);
  //////////////////////
  void bindToolHandler(SceneEditorVPToolHandler* handler);
  void bindToolHandler(const std::string& ToolName);
  void RegisterToolHandler(const std::string& ToolName, SceneEditorVPToolHandler* handler);
  //////////////////////
  void SaveCubeMap();
  void SetCursor(const fvec3& c) { mCursor = c; }
  void UpdateScene(lev2::DrawableBuffer* pdb);

  ///////////////////////////////////////////////////
  void DrawManip(lev2::CompositorDrawData& CDD, lev2::Context* pProxyTarg);
  void DrawGrid(lev2::RenderContextFrameData& fdata);
  void DrawHUD(lev2::RenderContextFrameData& FrameData);
  void DrawBorder(lev2::RenderContextFrameData& FrameData);
  void Init();
  ///////////////////////////////////////////////////

  const ent::Simulation* simulation();
  SceneEditorBase& SceneEditor() { return mEditor; }
  ork::ent::SceneEditorView& SceneEditorView() { return mSceneView; }
  EditorMainWindow& MainWindow() const { return mMainWindow; }
  ork::lev2::IRenderer* GetRenderer() const { return _renderer; }
  lev2::ManipManager& ManipManager() { return mEditor.ManipManager(); }
  lev2::UiCamera* getActiveCamera() const { return _editorCamera; }

  ///////////////////////////////////////////////////
  bool isCompositorEnabled();
  ///////////////////////////////////////////////////
  void drawProgressFrame(opq::progressdata_ptr_t data);
  ///////////////////////////////////////////////////

  static void RegisterInitCallback(SceneEditorInitCb icb);

  void NotInDrawSync() const {
    while (int(mRenderLock))
      usleep(1000);
  }

  bool IsSceneDisplayEnabled() const { return mbSceneDisplayEnable; }

protected:
  void DoDraw(ui::DrawEvent& drwev) final; // virtual

  ork::atomic<int> mRenderLock;

  msgrouter::subscriber_t _simchannelsubscriber;
  int miPickDirtyCount;
  SceneEditorBase& mEditor;

  lev2::BasicFrameTechnique* mpBasicFrameTek;
  EditorMainWindow& mMainWindow;

  orkmap<std::string, SceneEditorVPToolHandler*> mToolHandlers;
  SceneEditorVPToolHandler* mpCurrentHandler;
  SceneEditorVPToolHandler* mpDefaultHandler;

  int mGridMode;
  ork::ent::SceneEditorView mSceneView;
  lev2::IRenderer* _renderer;
  lev2::UiCamera* _editorCamera;
  fvec3 mCursor;
  int miCameraIndex;
  int miCullCameraIndex;
  int mCompositorSceneIndex;
  int mCompositorSceneItemIndex;
  bool mbSceneDisplayEnable;
  orkstack<lev2::CompositingPassData> _compositingGroupStack;
  lev2::CTXBASE* _ctxbase = nullptr;

private:
  UpdateThread* _updateThread;
  std::shared_ptr<ork::lev2::CameraMatrices> _overlayCamMatrices;

  void DisableSceneDisplay();
  void EnableSceneDisplay();

  ////////////////////////////////////////////

  void DoInit(ork::lev2::Context* pTARG) override;

  static orkset<SceneEditorInitCb> mInitCallbacks;
};

///////////////////////////////////////////////////////////////////////////

} // namespace ent
} // namespace ork
