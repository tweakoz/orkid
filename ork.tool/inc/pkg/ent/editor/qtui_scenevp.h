////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/object/Object.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/thread.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/editor/editor.h>
#include <pkg/ent/Lighting.h>
#include <orktool/qtui/qtmainwin.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>
//#include <ork/lev2/gfx/builtin_frameeffects.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <pkg/ent/Lighting.h>
#include <pkg/ent/Compositor.h>
#include <ork/lev2/gfx/pickbuffer.h>
//#include <pkg/ent/FullscreenEffects.h>
///////////////////////////////////////////////////////////////////////////////

class QWidget;

#define _THREADED_RENDERER

///////////////////////////////////////////////////////////////////////////////
namespace ork{ namespace lev2 {

class CGfxMaterialUITextured;
class RenderContextFrameData;
class FrameTechniqueBase;
class Renderer;
class CCamera_persp;
class CCamera_ortho;
class CCamera;

} // namespace lev2

///////////////////////////////////////////////////////////////////////////////
namespace tool { template <typename T> class CPickBuffer; }
///////////////////////////////////////////////////////////////////////////////
namespace ent {
///////////////////////////////////////////////////////////////////////////////
class CompositingGroup;
class CompositingSceneItem;
class CompositingComponentInst;
class SceneEditorVP;
class EditorSceneInst;
class EditorMainWindow;
class CompositingManagerComponentInst;
///////////////////////////////////////////////////////////////////////////////
typedef void (*SceneEditorInitCb)( ork::ent::SceneEditorVP& vped );

class SceneEditorVPToolHandler; // : public SceneEditorVPToolHandlerBase;

///////////////////////////////////////////////////////////////////////////////
#if defined(_THREADED_RENDERER)
class UpdateThread : public ork::Thread
{
public:
	UpdateThread( SceneEditorVP* pVP );
	~UpdateThread();
private:

	void run() override;
	SceneEditorVP* mpVP;
	bool mbOKTOEXIT;
	bool mbEXITING;
};
#endif
///////////////////////////////////////////////////////////////////////////////

class SceneEditorView : public ork::Object
{
	RttiDeclareAbstract( SceneEditorView, Object );

	SceneEditorVP*	mVP;

	//////////////////////////////////////////////////////////

	void SlotObjectSelected( ork::Object* pobj );
	void SlotObjectDeSelected( ork::Object* pobj );
	void SlotModelDirty();

	//////////////////////////////////////////////////////////

public:

	SceneEditorView(SceneEditorVP*vp);

	//////////////////////////////////////////////////////////
	void UpdateRefreshPolicy(lev2::RenderContextFrameData& FrameData, const SceneInst* sinst);
};

///////////////////////////////////////////////////////////////////////////
class SceneEditorVP : public ui::Viewport
{
	RttiDeclareAbstract( SceneEditorVP, ui::Viewport );
	friend class lev2::CPickBuffer<SceneEditorVP>;

public:

	SceneEditorVP( const std::string & name, SceneEditorBase & Editor, EditorMainWindow &MainWin );
	~SceneEditorVP();

	//////////////////////
	ui::HandlerResult DoOnUiEvent( const ui::Event& EV ) override;
	//////////////////////
	void QueueSceneInstToDb(ent::DrawableBuffer*pDB);
	void RenderQueuedScene( ork::lev2::RenderContextFrameData & ContextData );
	//////////////////////
	//lev2::CPickBuffer<SceneEditorVP>* GetPickBuffer() { return (lev2::CPickBuffer<SceneEditorVP>*)mpPickBuffer; }
	void IncPickDirtyCount( int icount );
	void GetPixel( int ix, int iy, lev2::GetPixelContext& ctx );
	ork::Object* GetObject( lev2::GetPixelContext& ctx, int ichan );
	//////////////////////
	ent::CompositingManagerComponentInst* GetCMCI();
	const ent::CompositingGroup* GetCompositingGroup(int igrp);
	ent::CompositingComponentInst* GetCompositingComponentInst( int icidx );
	//////////////////////
	void BindToolHandler( SceneEditorVPToolHandler*handler );
	void BindToolHandler( const std::string& ToolName );
	void RegisterToolHandler( const std::string& ToolName, SceneEditorVPToolHandler*handler );
	//////////////////////
	void SetHeadLightMode( bool bv ) { mbHeadLight=bv; }
	void SaveCubeMap();
	void SetCursor( const CVector3& c ) { mCursor=c; }
	void UpdateScene(ent::DrawableBuffer*pdb);

	///////////////////////////////////////////////////
	void SetupLighting( lev2::HeadLightManager& hlmgr, lev2::RenderContextFrameData& fdata );
	///////////////////////////////////////////////////
	void DrawManip( lev2::RenderContextFrameData& fdata, lev2::GfxTarget* pProxyTarg );
	void DrawGrid( lev2::RenderContextFrameData& fdata );
	void Draw3dContent( lev2::RenderContextFrameData& FrameData );
	void DrawHUD( lev2::RenderContextFrameData& FrameData );
	void DrawSpinner(lev2::RenderContextFrameData & FrameData);
	void Init();
	///////////////////////////////////////////////////

	const ent::SceneInst* GetSceneInst();
	SceneEditorBase& SceneEditor() { return mEditor; }
	ork::ent::SceneEditorView& SceneEditorView() { return mSceneView; }
	EditorMainWindow& MainWindow() const { return mMainWindow; }
	ork::lev2::Renderer* GetRenderer() const { return mRenderer; }
	lev2::CManipManager& ManipManager() { return mEditor.ManipManager(); }
	const lev2::CCamera* GetActiveCamera() const { return _editorCamera; }

	///////////////////////////////////////////////////

	static void RegisterInitCallback( SceneEditorInitCb icb );

	void NotInDrawSync() const { while(int(mRenderLock)) usleep(1000); }

	bool IsSceneDisplayEnabled() const { return mbSceneDisplayEnable; }

protected:

    void DoDraw( ui::DrawEvent& drwev ) final; //virtual

	ork::atomic<int>								mRenderLock;

	//lev2::CPickBuffer<SceneEditorVP>*				mpPickBuffer;
	int												miPickDirtyCount;
	bool											mbHeadLight;
	SceneEditorBase&								mEditor;

	lev2::BasicFrameTechnique*						mpBasicFrameTek;
	EditorMainWindow&								mMainWindow;

	orkmap<std::string,SceneEditorVPToolHandler*>	mToolHandlers;
	SceneEditorVPToolHandler*						mpCurrentHandler;
	SceneEditorVPToolHandler*						mpDefaultHandler;
	lev2::Texture*									mpCurrentToolIcon;

	int												mGridMode;
	ork::ent::SceneEditorView						mSceneView;
	lev2::Renderer*									mRenderer;
	lev2::CCamera*									_editorCamera;
	CVector3										mCursor;
	CPerformanceItem								mFramePerfItem;
	int												miCameraIndex;
	int												miCullCameraIndex;
	int												mCompositorSceneIndex;
	int												mCompositorSceneItemIndex;
	orkstack<ent::CompositingPassData>				mCompositingGroupStack;
	//DrawableBufferLock								mDbLock;
	bool											mbSceneDisplayEnable;

private:

	UpdateThread* 									mUpdateThread;

	void DisableSceneDisplay();
	void EnableSceneDisplay();

	////////////////////////////////////////////
	class FrameRenderer : public ork::lev2::FrameRenderer
	{
		SceneEditorVP*	mpViewport;
		virtual void Render();
		public:
		FrameRenderer( SceneEditorVP* pvp ) : mpViewport(pvp) {}
	};
	////////////////////////////////////////////

	void DoInit( ork::lev2::GfxTarget* pTARG ) override;
	bool DoNotify(const ork::event::Event* pev) override;

	static orkset<SceneEditorInitCb>	mInitCallbacks;

};

///////////////////////////////////////////////////////////////////////////

}
}
