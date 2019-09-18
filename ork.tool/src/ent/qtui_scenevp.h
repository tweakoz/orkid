////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/thread.h>
#include <ork/kernel/timer.h>
#include <ork/object/Object.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/drawable.h>
#include <orktool/qtui/qtmainwin.h>
#include <pkg/ent/Lighting.h>
#include <pkg/ent/editor/editor.h>
#include <pkg/ent/scene.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>
#include <pkg/ent/CompositingSystem.h>
#include <pkg/ent/Lighting.h>
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
class Camera;
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

  //////////////////////////////////////////////////////////
  void UpdateRefreshPolicy(lev2::RenderContextFrameData& FrameData, const Simulation* sinst);
};

///////////////////////////////////////////////////////////////////////////
class SceneEditorVP : public ui::Viewport {
  RttiDeclareAbstract(SceneEditorVP, ui::Viewport);
  friend class lev2::PickBuffer<SceneEditorVP>;

public:
  SceneEditorVP(const std::string& name, SceneEditorBase& Editor, EditorMainWindow& MainWin);
  ~SceneEditorVP();

  //////////////////////
  ui::HandlerResult DoOnUiEvent(const ui::Event& EV) override;
  //////////////////////
  void enqueueSimulationDrawables(lev2::DrawableBuffer* pDB);
  void RenderQueuedScene(ork::lev2::RenderContextFrameData& ContextData);
  //////////////////////
  // lev2::PickBuffer<SceneEditorVP>* GetPickBuffer() { return (lev2::PickBuffer<SceneEditorVP>*)mpPickBuffer; }
  void IncPickDirtyCount(int icount);
  void GetPixel(int ix, int iy, lev2::GetPixelContext& ctx);
  ork::Object* GetObject(lev2::GetPixelContext& ctx, int ichan);
  //////////////////////
  ent::CompositingSystem* compositingSystem();
  const lev2::CompositingGroup* GetCompositingGroup(int igrp);
  //////////////////////
  void bindToolHandler(SceneEditorVPToolHandler* handler);
  void bindToolHandler(const std::string& ToolName);
  void RegisterToolHandler(const std::string& ToolName, SceneEditorVPToolHandler* handler);
  //////////////////////
  void SetHeadLightMode(bool bv) { mbHeadLight = bv; }
  void SaveCubeMap();
  void SetCursor(const fvec3& c) { mCursor = c; }
  void UpdateScene(lev2::DrawableBuffer* pdb);

  ///////////////////////////////////////////////////
  void SetupLighting(lev2::HeadLightManager& hlmgr, lev2::RenderContextFrameData& fdata);
  ///////////////////////////////////////////////////
  void DrawManip(lev2::RenderContextFrameData& fdata, lev2::GfxTarget* pProxyTarg);
  void DrawGrid(lev2::RenderContextFrameData& fdata);
  void Draw3dContent(lev2::RenderContextFrameData& FrameData);
  void DrawHUD(lev2::RenderContextFrameData& FrameData);
  void DrawSpinner(lev2::RenderContextFrameData& FrameData);
  void Init();
  ///////////////////////////////////////////////////

  const ent::Simulation* simulation();
  SceneEditorBase& SceneEditor() { return mEditor; }
  ork::ent::SceneEditorView& SceneEditorView() { return mSceneView; }
  EditorMainWindow& MainWindow() const { return mMainWindow; }
  ork::lev2::IRenderer* GetRenderer() const { return _renderer; }
  lev2::ManipManager& ManipManager() { return mEditor.ManipManager(); }
  lev2::Camera* getActiveCamera() const { return _editorCamera; }

  ///////////////////////////////////////////////////
  bool isCompositorEnabled();
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

  // lev2::PickBuffer<SceneEditorVP>*				mpPickBuffer;
  int miPickDirtyCount;
  bool mbHeadLight;
  SceneEditorBase& mEditor;

  lev2::BasicFrameTechnique* mpBasicFrameTek;
  EditorMainWindow& mMainWindow;

  orkmap<std::string, SceneEditorVPToolHandler*> mToolHandlers;
  SceneEditorVPToolHandler* mpCurrentHandler;
  SceneEditorVPToolHandler* mpDefaultHandler;
  lev2::Texture* mpCurrentToolIcon;

  int mGridMode;
  ork::ent::SceneEditorView mSceneView;
  lev2::IRenderer* _renderer;
  lev2::Camera* _editorCamera;
  fvec3 mCursor;
  PerformanceItem mFramePerfItem;
  int miCameraIndex;
  int miCullCameraIndex;
  int mCompositorSceneIndex;
  int mCompositorSceneItemIndex;
  orkstack<lev2::CompositingPassData> mCompositingGroupStack;
  bool mbSceneDisplayEnable;

private:
  UpdateThread* _updateThread;

  void DisableSceneDisplay();
  void EnableSceneDisplay();

  ////////////////////////////////////////////

  void DoInit(ork::lev2::GfxTarget* pTARG) override;
  bool DoNotify(const ork::event::Event* pev) override;

  static orkset<SceneEditorInitCb> mInitCallbacks;
};

///////////////////////////////////////////////////////////////////////////

} // namespace ent
} // namespace ork
