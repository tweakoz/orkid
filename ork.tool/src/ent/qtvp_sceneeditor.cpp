////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <ork/application/application.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/timer.h>

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/input/input.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/kernel/timer.h>

#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/drawable.h>
#include <orktool/qtui/qtvp_edrenderer.h>
#include <ork/reflect/RegisterProperty.h>

#include <orktool/toolcore/dataflow.h>

///////////////////////////////////////////////////////////////////////////////

#include <pkg/ent/editor/qtui_scenevp.h>
#include <pkg/ent/editor/qtvp_uievh.h>
#include <pkg/ent/editor/edmainwin.h>
#include <pkg/ent/Compositor.h>

#include <ork/gfx/camera.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/kernel/future.hpp>

#include <pkg/ent/Lighting.h>

///////////////////////////////////////////////////////////////////////////////

extern bool gtoggle_hud;
static const bool draw_pickbuffer = false;

using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////

template class ork::lev2::CPickBuffer<ork::ent::SceneEditorVP>;

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SceneEditorView, "SceneEditorView" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SceneEditorVP, "SceneEditorVP" );

namespace ork {
namespace lev2 {
	int GetGlError( void );

}
void SetCurrentThreadName( const char*pname );
namespace ent {

#if defined(NDEBUG)
#error
#endif


///////////////////////////////////////////////////////////////////////////////

void SceneEditorView::Describe()
{
	reflect::RegisterFunctor("SlotModelDirty", &SceneEditorView::SlotModelDirty);
	reflect::RegisterFunctor("SlotObjectSelected", &SceneEditorView::SlotObjectSelected);
	reflect::RegisterFunctor("SlotObjectDeSelected", &SceneEditorView::SlotObjectDeSelected);
}

///////////////////////////////////////////////////////////////////////////////

orkset<SceneEditorInitCb> SceneEditorVP::mInitCallbacks;

///////////////////////////////////////////////////////////////////////////////

UpdateThread* gupdatethread = 0;

void SceneEditorVP::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DisableSceneDisplay()
{
	ork::AssertOnOpQ2( ork::MainThreadOpQ() );
	mbSceneDisplayEnable = false;
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::EnableSceneDisplay()
{
	ork::AssertOnOpQ2( ork::MainThreadOpQ() );
	mbSceneDisplayEnable = true;
}

SceneEditorVP::SceneEditorVP( const std::string & name, SceneEditorBase & Editor, EditorMainWindow &MainWin )
	: ui::Viewport( name, 1, 1, 1, 1, CColor3( 0.0f, 0.0f, 0.0f ), 1.0f )
	, mMainWindow( MainWin )
	, miPickDirtyCount( 0 )
	, mbHeadLight( true )
	, mEditor( Editor )
	, mpBasicFrameTek( 0 )
	, mpCurrentHandler( 0 )
	, mpCurrentToolIcon(0)
	, mGridMode( 0 )
	, mRenderer( new ork::tool::Renderer )
	, mSceneView( this )
	, mActiveCamera( 0 )
	, mFramePerfItem( "SceneEditorVP::Draw()" )
	, miCullCameraIndex(-1)
	, miCameraIndex(0)
	, mCompositorSceneIndex(0)
	, mCompositorSceneItemIndex(0)
	, mbSceneDisplayEnable(false)
	, mUpdateThread(nullptr)
{
	mRenderLock = 0;
	ork::event::Broadcaster& bcaster = ork::event::Broadcaster::GetRef();
	bcaster.AddListenerOnChannel( this, ork::ent::SceneInst::EventChannel() );

	///////////////////////////////////////////////////////////

	ent::DrawableBuffer::ClearAndSync();

	///////////////////////////////////////////////////////////

	mpBasicFrameTek = new BasicFrameTechnique( );

	PushFrameTechnique( mpBasicFrameTek );

#if defined(_THREADED_RENDERER)
	mUpdateThread = new UpdateThread(this);
	mUpdateThread->start();
#endif
}

///////////////////////////////////////////////////////////////////////////

SceneEditorVP::~SceneEditorVP()
{
	delete mUpdateThread;

	ork::event::Broadcaster& bcaster = ork::event::Broadcaster::GetRef();
	bcaster.RemoveListenerOnChannel( this, ork::ent::SceneInst::EventChannel() );
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::Init()
{
	((ork::tool::Renderer*)(mRenderer))->Init();

	///////////////////////////////////////////////////////////
	// INIT BUILT IN TOOL HANDLERS

	ork::msleep(500);
	RegisterToolHandler( "0Default", new TestVPDefaultHandler( mEditor ) );
	RegisterToolHandler( "1ManipTrans", new ManipTransHandler( mEditor ) );
	RegisterToolHandler( "2ManipRot", new ManipRotHandler( mEditor ) );
	//RegisterToolHandler( "3Collision", new PaintCollisionHandler( this, mEditor ) );
	//RegisterToolHandler( "4Bsp", new BspToolHandler( this, mEditor ) );
	
	mpDefaultHandler = mToolHandlers[ "0Default" ];
	BindToolHandler( "0Default" );

	///////////////////////////////////////////////////////////
	// INIT MODULAR TOOL HANDLERS

	for( orkset<SceneEditorInitCb>::iterator it =mInitCallbacks.begin();
			it!=mInitCallbacks.end();
			it++ )
	{
		(*it)( *this );
	}

}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::RegisterInitCallback(ork::ent::SceneEditorInitCb icb)
{
	mInitCallbacks.insert( icb );
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorView::SlotObjectSelected( ork::Object* pobj )
{
	EntData* pdata = rtti::autocast(pobj);
	if( pdata )
	{
		CVector3 pos = pdata->GetDagNode().GetTransformNode().GetTransform()->GetPosition();
		//if( mVP->GetCamera() )
		//	mVP->GetCamera()->CamFocus = pos;
		//mVP->mpPickBuffer->SetDirty(true);
	}
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorView::SlotObjectDeSelected( ork::Object* pobj )
{

}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::IncPickDirtyCount( int icount )
{
	mpPickBuffer->SetDirty(true);
}

void SceneEditorView::SlotModelDirty()
{
	mVP->IncPickDirtyCount(1);
}

///////////////////////////////////////////////////////////////////////////

SceneEditorView::SceneEditorView(SceneEditorVP*vp)
	: mVP(vp)
{

}
///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DoInit( ork::lev2::GfxTarget* pTARG )
{
	mpPickBuffer = new lev2::CPickBuffer<SceneEditorVP>(	pTARG->FBI()->GetThisBuffer(), 
															this,
															0, 0, 1024, 1024,
															lev2::PickBufferBase::EPICK_FACE_VTX
													   );
	mpPickBuffer->RefClearColor().SetRGBAU32( 0 );
	mpPickBuffer->CreateContext();
	mpPickBuffer->GetContext()->FBI()->SetClearColor( CColor4(0.0f,0.0f,0.0f,0.0f) );
	int iw = pTARG->GetW();
	int ih = pTARG->GetH();
	
	pTARG->FBI()->SetClearColor( CColor4(0.0f,0.0f,0.0f,0.0f) );

	orkprintf( "PickBuffer<%p>\n", mpPickBuffer );

}

///////////////////////////////////////////////////////////////////////////
// Draw INTO the onscreen target
///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DoDraw(ui::DrawEvent& drwev)
{
	if( gUpdateStatus.GetState()!=EUPD_RUNNING) return;

	mRenderLock = 1;

	const SRect tgtrect = SRect( 0, 0, mpTarget->GetW(), mpTarget->GetH() );
	FrameRenderer the_renderer( this );
	lev2::UiViewportRenderTarget rt( this );
	the_renderer.GetFrameData().SetDstRect( tgtrect );
	mRenderer->SetTarget( mpTarget );
	the_renderer.GetFrameData().SetTarget( mpTarget );
		
	/////////////////////////////////////////////////////////////////////////////////
	
	bool bFX = false;
	auto pCMCI = GetCMCI();
	if( GetSceneInst() )
	{	ent::ESceneInstMode emode = GetSceneInst()->GetSceneInstMode();
		if( pCMCI ) switch( emode )
		{
			case ent::ESCENEMODE_RUN:
			case ent::ESCENEMODE_SINGLESTEP:
			{
				bFX = pCMCI->IsEnabled();
				break;
			}
			default:
				break;
		}
	}
	
	/////////////////////////////////////////////////////////////////////////////////

	the_renderer.GetFrameData().PushRenderTarget( & rt );
	{
		/////////////////////////////////
		if( bFX )
		/////////////////////////////////
		{	float frame_rate = pCMCI ? pCMCI->GetCurrentFrameRate() : 0.0f;
			bool externally_fixed_rate = (frame_rate!=0.0f);
			ent::SceneInst* psi = this->GetSceneInst();

			RenderSyncToken syntok;
			/////////////////////////////
			bool have_token = false;
			if( externally_fixed_rate )
			{	
				Timer totim;
				totim.Start();
				while(false==have_token && (totim.SecsSinceStart()<2.0f))
				{	have_token = ent::DrawableBuffer::mOfflineRenderSynchro.try_pop(syntok);
					usleep(1000);
				}
			}
			/////////////////////////////
			// render it
			/////////////////////////////

			ent::CMCIdrawdata compositorDrawData( the_renderer );
			pCMCI->Draw( compositorDrawData );

			////////////////////////////////////////////
			// FrameTechnique FinalMRT + HUD -> screen
			////////////////////////////////////////////
			the_renderer.GetFrameData().PushRenderTarget( & rt );
			if( externally_fixed_rate && have_token )
			{
				bool use_offscreen_buffer = false;

				////////////////////////////////////////
				// setup destination buffer as offscreen buffer
				//  (for write to disk)
				////////////////////////////////////////

				int itw = mpTarget->GetW();
				int ith = mpTarget->GetH();

				if( use_offscreen_buffer )
				{
					auto poutbuf = new GfxBuffer(0,0,0,1280,720);
					mpTarget->InitializeContext(poutbuf);
					auto rtg = new RtGroup(mpTarget,1280,720);
					auto mrt = new RtBuffer(	
						rtg,
						lev2::ETGTTYPE_MRT0,
						lev2::EBUFFMT_RGBA32,
						1280,720 );
					//mpTarget->InitializeContext(mrt);

					rtg->SetMrt( 0, mrt );

					const SRect tgtrect2 = SRect( 0, 0, 1280, 720 );

					the_renderer.GetFrameData().SetTarget( mpTarget );
					mRenderer->SetTarget( mpTarget );
					the_renderer.GetFrameData().SetDstRect( tgtrect2 );
					mpTarget->FBI()->SetAutoClear(true);
					mpTarget->FBI()->SetViewport( 0,0, itw, ith );
					mpTarget->FBI()->SetScissor( 0,0, itw, ith );
					mpTarget->BeginFrame();
					mpTarget->FBI()->PushRtGroup(rtg);
					{	
						pCMCI->ComposeToScreen( mpTarget );
						////////////////////////////////////////
						// write to disk
						////////////////////////////////////////
						file::Path::NameType fnamesyn;
						fnamesyn.format("outputframes/frame%04d.tga",syntok.mFrameIndex);			
						mpTarget->FBI()->Capture( *poutbuf, file::Path(fnamesyn.c_str()) );
						////////////////////////////////////////
					}
					mpTarget->FBI()->PopRtGroup();
					mpTarget->EndFrame();// the_renderer );
				}
				////////////////////////////////////////
				// setup destination buffer as onscreen buffer
				////////////////////////////////////////
				else
				{	the_renderer.GetFrameData().SetTarget( mpTarget );
					mRenderer->SetTarget( mpTarget );
					the_renderer.GetFrameData().SetDstRect( tgtrect );
					mpTarget->FBI()->SetAutoClear(true);
					//mpTarget->FBI()->SetViewport( 0,0, itw, ith );
					//mpTarget->FBI()->SetScissor( 0,0, itw, ith );
					mpTarget->BeginFrame();
						pCMCI->ComposeToScreen( mpTarget );
					mpTarget->EndFrame();// the_renderer );
						////////////////////////////////////////
						// write to disk
						////////////////////////////////////////
						auto buf = mpTarget->FBI()->GetThisBuffer();
						file::Path::NameType fnamesyn;
						fnamesyn.format("outputframes/frame%04d.tga",syntok.mFrameIndex);			
						mpTarget->FBI()->Capture( *buf, file::Path(fnamesyn.c_str()) );
						////////////////////////////////////////
				}

				////////////////////////////////////////
				// return the token
				////////////////////////////////////////
				
				DrawableBuffer::mOfflineUpdateSynchro.push(syntok);

			}
			else
			{
				the_renderer.GetFrameData().SetTarget( mpTarget );
				mRenderer->SetTarget( mpTarget );
				the_renderer.GetFrameData().SetDstRect( tgtrect );
				mpTarget->FBI()->SetAutoClear(true);
				
				mpTarget->FBI()->SetViewport( 0,0, mpTarget->GetW(), mpTarget->GetH() );
				mpTarget->FBI()->SetScissor( 0,0, mpTarget->GetW(), mpTarget->GetH() );
				mpTarget->BeginFrame();
				pCMCI->ComposeToScreen( mpTarget );
				/////////////////////////////////////////////////////////////////////
				// HUD
				/////////////////////////////////////////////////////////////////////
				
				if( gtoggle_hud )
				{
					DrawHUD(the_renderer.GetFrameData());
					DrawChildren(drwev);
				}

				/////////////////////////////////////////////////////////////////////				
				mpTarget->EndFrame();// the_renderer );
			}		
			the_renderer.GetFrameData().PopRenderTarget();
		}
		/////////////////////////////////
		else
		/////////////////////////////////
		{
			const ent::DrawableBuffer* DB = DrawableBuffer::LockReadBuffer(7);//mDbLock.Aquire(7);
			the_renderer.GetFrameData().SetUserProperty("DB",anyp(DB) );
			
			if( DB )
			{
	
				anyp PassData;
				PassData.Set<orkstack<ent::CompositingPassData>*>( & mCompositingGroupStack );
				the_renderer.GetFrameData().SetUserProperty( "nodes", PassData );
				the_renderer.GetFrameData().PushRenderTarget( & rt );
				the_renderer.GetFrameData().SetTarget( mpTarget );
				mRenderer->SetTarget( mpTarget );
				the_renderer.GetFrameData().SetDstRect( tgtrect );
				mpTarget->FBI()->SetAutoClear(true);
				mpTarget->FBI()->SetViewport( 0,0, mpTarget->GetW(), mpTarget->GetH() );
				mpTarget->FBI()->SetScissor( 0,0, mpTarget->GetW(), mpTarget->GetH() );
				mpTarget->BeginFrame();
				{
					CompositingPassData node;
					node.mpGroup = 0;
					node.mpFrameTek = 0;//mpBasicFrameTek;
					mCompositingGroupStack.push(node);
					mpBasicFrameTek->mbDoBeginEndFrame = false;
					mpBasicFrameTek->Render( the_renderer );
					mCompositingGroupStack.pop();

					if( gtoggle_hud )
					{
						DrawHUD(the_renderer.GetFrameData());
						DrawChildren(drwev);
					}
				}
				mpTarget->EndFrame();// the_renderer );
				the_renderer.GetFrameData().PopRenderTarget();
			
			}
			DrawableBuffer::UnLockReadBuffer(DB);
		}
	}
	the_renderer.GetFrameData().PopRenderTarget();
	
	the_renderer.GetFrameData().SetDstRect( tgtrect );
	mRenderLock = 0;
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::FrameRenderer::Render()
{
	switch( GetFrameData().GetRenderingMode() )
	{
		case ork::lev2::RenderContextFrameData::ERENDMODE_STANDARD:
		case ork::lev2::RenderContextFrameData::ERENDMODE_LIGHTPREPASS:
			mpViewport->Draw3dContent( GetFrameData() );
			break;
		default:
			break;
	}
}

///////////////////////////////////////////////////////////////////////////

ent::CompositingManagerComponentInst* SceneEditorVP::GetCMCI()
{	auto psi = mEditor.GetActiveSceneInst();
	return (psi!=nullptr) ? psi->GetCMCI() : nullptr;
}

///////////////////////////////////////////////////////////////////////////
// Draw 3d content for the editor
// this will always eventually go to the onscreen target
//  though it MAY go thru an intermediate rendertarget first
///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::Draw3dContent( lev2::RenderContextFrameData& FrameData )
{
	if( false == mbSceneDisplayEnable ) return;

	lev2::GfxTarget* pTARG = FrameData.GetTarget();
	///////////////////////////////////////////////////////////////////////////	
	mFramePerfItem.Enter();
	///////////////////////////////////////////////////////////////////////////
	lev2::IRenderTarget* pIT = FrameData.GetRenderTarget();
	///////////////////////////////////////////////////////////////////////////
	const SRect & frame_rect = FrameData.GetDstRect();
	int ix = pTARG->GetX();
	int iy = pTARG->GetY();
	int iw = pTARG->GetW();
	int ih = pTARG->GetH();
	SetRect(ix,iy,iw,ih);
	///////////////////////////////////////////////////////////////////////////
	float fW = float(pTARG->GetW());
	float fH = float(pTARG->GetH());
	FrameData.GetCameraCalcCtx().mfAspectRatio = fW/fH;
	///////////////////////////////////////////////////////////////////////////
	ent::SceneData *pscene = mEditor.mpScene;

	anyp PassData = FrameData.GetUserProperty( "nodes" );
	orkstack<ent::CompositingPassData>* cstack = 0;
	cstack = PassData.Get<orkstack<ent::CompositingPassData>*>();
	OrkAssert(cstack!=0);
	
	CompositingPassData node = cstack->top();
	const ent::CompositingGroup* pCG = node.mpGroup;
	lev2::BuiltinFrameTechniques* pFTEK = node.mpFrameTek;
	///////////////////////////////////////////////////////////////////////////
	const char* EffectName = "none";
	float fFxAmt =0.0f;
	float fFbAmt =0.0f;
	float fFinResMult = 0.5f;
	float fFxResMult = 0.5f;
    lev2::Texture* pFbUvMap = 0; 
	bool bpostfxfb = false;
	if( pFTEK )
	{
		if( pCG )
		{	const CompositingGroupEffect&	effect = pCG->GetEffect();
			EffectName = effect.GetEffectName();
			fFxAmt = effect.GetEffectAmount();
			fFbAmt = effect.GetFeedbackAmount();
            pFbUvMap = effect.GetFbUvMap();
			bpostfxfb = effect.IsPostFxFeedback();
			fFinResMult = effect.GetFinalRezScale();
			fFxResMult = effect.GetFxRezScale();
		}
		pFTEK->SetEffect( EffectName, fFxAmt, fFbAmt );
        pFTEK->SetFbUvMap( pFbUvMap );
		pFTEK->SetPostFxFb( bpostfxfb );

		////////////////////////////////////////
		// set buffer sizes
		////////////////////////////////////////

		int ifxW = int(fFxResMult * fW);
		int ifxH = int(fFxResMult * fH);
		int ifinalW = int(fFinResMult * fW);
		int ifinalH = int(fFinResMult * fH);
		pFTEK->ResizeFinalBuffer( ifinalW, ifinalH );
		pFTEK->ResizeFxBuffer( ifxW, ifxH );

		////////////////////////////////////////
	}
	///////////////////////////////////////////////////////////////////////////
	SRect VPRect( 0, 0, pIT->GetW(), pIT->GetH() );
	pTARG->FBI()->PushViewport( VPRect );
	pTARG->FBI()->PushScissor( VPRect );
	{	mRenderer->SetTarget( pTARG );
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
		ent::SceneInst* psi = GetSceneInst();
		mSceneView.UpdateRefreshPolicy(FrameData,psi);
	
		Clear(); 
		if( node.mbDrawSource ) 
			RenderSDLD( FrameData );
	};
	pTARG->FBI()->PopScissor();
	pTARG->FBI()->PopViewport();
	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////
	// filth up the pick buffer
	if( miPickDirtyCount>0 )
	{	if( mpPickBuffer )
		{	mpPickBuffer->SetDirty(true);
			miPickDirtyCount--;
		}
	}
	///////////////////////////////////////////////////////
	mFramePerfItem.Exit();
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::QueueSDLD(ent::DrawableBuffer*pDB) // Queue SceneDrawLayerData
{
	pDB->mSDLD.Queue(GetSceneInst(), pDB );
}

///////////////////////////////////////////////////////////////////////////////

ent::CompositingComponentInst* SceneEditorVP::GetCompositingComponentInst( int icidx )
{
	ent::CompositingComponentInst* rval = 0;
	ent::CompositingManagerComponentInst* pCMCI = GetCMCI();
	if( pCMCI )
	{	rval = pCMCI->GetCompositingComponentInst(icidx);
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

const CompositingGroup* SceneEditorVP::GetCompositingGroup(int igrp)
{
	ent::CompositingManagerComponentInst* pCMCI = GetCMCI();
	const ent::CompositingComponentInst* pCCI = GetCompositingComponentInst(igrp);
	const CompositingGroup* pCG = 0;
	if( pCCI )
	{
		const CompositingComponentData& CCD = pCCI->GetCompositingData();
		const orklut<PoolString,ork::Object*>& Groups = CCD.GetGroups();
		int inumgroups = Groups.size();
		if( inumgroups && igrp>= 0 )
		{
			int idx = igrp%inumgroups;
			ork::Object* pOBJ = Groups.GetItemAtIndex(idx).second;
			if( pOBJ )
				pCG = rtti::autocast(pOBJ);		
		}
	}
	return pCG;
}

///////////////////////////////////////////////////////////////////////////////

ent::SceneInst* SceneEditorVP::GetSceneInst()
{	ent::SceneInst* psi = 0;
	if( mEditor.mpScene )
	{	if(ApplicationStack::Top())
		{	psi = mEditor.GetActiveSceneInst();
		}
	}
	return psi;
}

///////////////////////////////////////////////////////////////////////////////
// Queue and Render Scene into active target
//  this may go to the onscreen or pickbuffer targets
///////////////////////////////////////////////////////////////////////////////

#define GL_ERRORCHECK() { int iErr = GetGlError(); OrkAssert( iErr==0 ); }

void SceneEditorVP::RenderSDLD( lev2::RenderContextFrameData & FrameData )
{
GL_ERRORCHECK();
	lev2::IRenderTarget* pIRT = FrameData.GetRenderTarget();
	bool IsPickState = FrameData.GetTarget()->FBI()->IsPickState();
	const bool forgepickstate = false;
	if( forgepickstate ) FrameData.GetTarget()->FBI()->EnterPickState(mpPickBuffer);
	//FrameData.GetTarget()->FBI()->ForceFlush();
GL_ERRORCHECK();
	///////////////////////////////////////////////////////////////////////////
	// get the compositor if there is one
	///////////////////////////////////////////////////////////////////////////

	anyp PassData = FrameData.GetUserProperty( "nodes" );
	orkstack<ent::CompositingPassData>* cstack = 0;
	cstack = PassData.Get<orkstack<ent::CompositingPassData>*>();
	OrkAssert(cstack!=0);

	CompositingPassData NODE = cstack->top();
	
	ent::CompositingManagerComponentInst* pCMCI = GetCMCI();
	const PoolString* pCAMNAME = NODE.mpCameraName;
	const PoolString* pLAYERSN = NODE.mpLayerName;

	///////////////////////////////////////////////////////////////////////////
	// camera setup
	///////////////////////////////////////////////////////////////////////////

	CCameraData TempCamData, TempCullCamData;
	mActiveCamera = 0;

	anyp pvdb = FrameData.GetUserProperty("DB");
	const DrawableBuffer* DB = pvdb.Get<const DrawableBuffer*>();
	if( 0 == DB ) return;

	const CCameraData* pcamdata = DB->mSDLD.GetCameraData(miCameraIndex);
	const CCameraData* pcullcamdata = DB->mSDLD.GetCameraData(miCullCameraIndex);
	//FrameData.GetTarget()->FBI()->ForceFlush();
	
	if( pcamdata )
	{
		mActiveCamera = pcamdata->GetLev2Camera();
	}

	if( pCAMNAME )
	{
		const CCameraData* pcamdataNAMED = DB->mSDLD.GetCameraData(*pCAMNAME);
		if( pcamdataNAMED )
			pcamdata = pcamdataNAMED;
		//printf( "Using CameraName<%s> CamData<%p>\n", pCAMNAME->c_str(), pcamdataNAMED );
	}
	GL_ERRORCHECK();
	//FrameData.GetTarget()->FBI()->ForceFlush();
	if( pcullcamdata )
	{
		TempCullCamData = *pcullcamdata;
		//FrameData.GetTarget()->FBI()->ForceFlush();
		TempCullCamData.BindGfxTarget(FrameData.GetTarget());
		//FrameData.GetTarget()->FBI()->ForceFlush();
		TempCullCamData.CalcCameraData(FrameData.GetCameraCalcCtx());
		//FrameData.GetTarget()->FBI()->ForceFlush();
		TempCamData.SetVisibilityCamDat(&TempCullCamData);
	}
	GL_ERRORCHECK();
	//FrameData.GetTarget()->FBI()->ForceFlush();
	if( pcamdata )
	{
		TempCamData = *pcamdata;
		//FrameData.GetTarget()->FBI()->ForceFlush();
		TempCamData.BindGfxTarget(FrameData.GetTarget());
	GL_ERRORCHECK();
		//FrameData.GetTarget()->FBI()->ForceFlush();
		TempCamData.CalcCameraData(FrameData.GetCameraCalcCtx());
	}
	GL_ERRORCHECK();
	//FrameData.GetTarget()->FBI()->ForceFlush();
	FrameData.SetCameraData( & TempCamData );
	if( mActiveCamera )
	{
		mActiveCamera->AttachViewport(this);
		mActiveCamera->RenderUpdate();
	}
	ManipManager().SetActiveCamera(mActiveCamera);
	
	//printf( "RENDER0...\n" );
	if( 0 == pcamdata ) return;
GL_ERRORCHECK();
	//FrameData.GetTarget()->FBI()->ForceFlush();
	///////////////////////////////////////////////////////////////////////////
	// dbuffer -> rendererqueue
	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	//OrkAssert( pcameraLUT!=0 );
	///////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////
	lev2::HeadLightManager hlmgr( FrameData );
	SetupLighting( hlmgr, FrameData );
GL_ERRORCHECK();
	///////////////////////////////////////////////////////////////////////////
	FrameData.GetTarget()->FBI()->GetThisBuffer()->SetDirty( false );
	//FrameData.GetTarget()->FBI()->ForceFlush();
	///////////////////////////////////////////////////////////////////////////
	FrameData.GetTarget()->BindMaterial( lev2::GfxEnv::GetDefault3DMaterial() );
	CMatrix4 IdentityMatrix;
GL_ERRORCHECK();

	/////////////////////////////////////////////////////////////////////////////
	const CMatrix4& PMTX = FrameData.GetCameraCalcCtx().mPMatrix;
	const CMatrix4& VMTX = FrameData.GetCameraCalcCtx().mVMatrix;
	/////////////////////////////////////////////////////////////////////////////
	// Main Renderer
	//printf( "RENDER...\n" );
	{	
		static lev2::SRasterState defstate;
		FrameData.GetTarget()->RSI()->BindRasterState( defstate, true );
		
		FrameData.GetTarget()->MTXI()->PushPMatrix( PMTX );
		FrameData.GetTarget()->MTXI()->PushVMatrix( VMTX );
		FrameData.GetTarget()->MTXI()->PushMMatrix( CMatrix4::Identity );
		{	/////////////////////////////////////////
			// manip
			/////////////////////////////////////////
			if( mEditor.mpScene ) DrawManip( FrameData, FrameData.GetTarget() );
			/////////////////////////////////////////
			// grid
			/////////////////////////////////////////
			if( false == IsPickState ) DrawGrid( FrameData );

			/////////////////////////////////////////
			// RenderQueue
			/////////////////////////////////////////
			std::vector<PoolString> LayerNames;
			if( pLAYERSN )
			{
				const char* playersstr = pLAYERSN->c_str();
				if( playersstr )
				{
					char temp_buf[256];
					strncpy(&temp_buf[0],playersstr,sizeof(temp_buf));
					char *tok = strtok( &temp_buf[0], "," );

					while( tok != 0 )
					{
						//orkprintf( "LAYER %s\n", tok );
						LayerNames.push_back( AddPooledString(tok) );
						tok = strtok( 0, "," );
					}
				}
				//LayerName = *pLAYERSN;
			}
			else
			{
				LayerNames.push_back( AddPooledLiteral( "All" ) );
			}
			//printf( "USING LAYERNAME<%s>\n", LayerName.c_str() );
			//const DrawableBuffer& DB = DrawableBuffer::GetLockedReadBuffer(0);
			for( std::vector<PoolString>::const_iterator itl=LayerNames.begin(); itl!=LayerNames.end(); itl++ )
			{
				const PoolString& layer_name = *itl;
				GetSceneInst()->RenderDrawableBuffer(GetRenderer(),*DB,layer_name);
			}
			GetRenderer()->DrawQueuedRenderables();
			/////////////////////////////////////////
		}
		FrameData.GetTarget()->MTXI()->PopPMatrix(); // back to ortho
		FrameData.GetTarget()->MTXI()->PopVMatrix(); // back to ortho
		FrameData.GetTarget()->MTXI()->PopMMatrix(); // back to ortho
		/////////////////////////////////////////
		// draw Spinner
		/////////////////////////////////////////
		if( false == IsPickState ) DrawSpinner( FrameData );
	}
	if( forgepickstate ) FrameData.GetTarget()->FBI()->EnterPickState(mpPickBuffer);
	//FrameData.GetTarget()->SetRenderContextFrameData( 0 );
	FrameData.SetLightManager(0);
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorView::UpdateRefreshPolicy( lev2::RenderContextFrameData& FrameData, const SceneInst* sinst )
{
	if( 0 == sinst ) return;
	
	//ork::tool::ged::ObjModel::FlushAllQueues();
	///////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////
	// refresh control

	lev2::CTXBASE* pctxb = FrameData.GetTarget()->GetCtxBase();

	lev2::GfxTarget* ptarg = FrameData.GetTarget();

	static orkstack<ent::ESceneInstMode> semodestack;

	if( pctxb )
	{
		ent::ESceneInstMode ecurmode = semodestack.size() ? semodestack.top() : ent::ESCENEMODE_EDIT;
		ent::ESceneInstMode enewmode = sinst->GetSceneInstMode();

		//if( enewmode != ecurmode )
		{
			switch( enewmode )
			{
				case ent::ESCENEMODE_EDIT:
					pctxb->SetRefreshPolicy( lev2::CTXBASE::EREFRESH_WHENDIRTY );
					break;
				case ent::ESCENEMODE_RUN:
				case ent::ESCENEMODE_SINGLESTEP:
				case ent::ESCENEMODE_PAUSE:
					pctxb->SetRefreshPolicy( lev2::CTXBASE::EREFRESH_FASTEST );
					break;
				default:
					break;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawHUD( lev2::RenderContextFrameData& FrameData )
{
	lev2::GfxTarget* pTARG = FrameData.GetTarget();

	const SRect & frame_rect = FrameData.GetDstRect();

	int itx0 = frame_rect.miX;
	int itx1 = frame_rect.miX2;
	int ity0 = frame_rect.miY;
	int ity1 = frame_rect.miY2;

	static Texture *pplaytex = ork::asset::AssetManager<ork::lev2::TextureAsset>::Create( "lev2://textures/play_icon" )->GetTexture();
	static Texture *ppaustex = ork::asset::AssetManager<ork::lev2::TextureAsset>::Create( "lev2://textures/pause_icon" )->GetTexture();

	/////////////////////////////////////////////////
	lev2::GfxMaterialUI UiMat(pTARG);
	pTARG->MTXI()->PushPMatrix( CMatrix4::Identity );
	pTARG->MTXI()->PushVMatrix( CMatrix4::Identity );
	pTARG->MTXI()->PushMMatrix( CMatrix4::Identity );
	//pTARG->MTXI()->PushUIMatrix();
	static SRasterState defstate;
	pTARG->RSI()->BindRasterState( defstate );
	{
		/////////////////////////////////////////////////
		// little spinner so i know which window is active
		pTARG->PushModColor( CColor4::White() );
				//pTARG->BindMaterial( GfxEnv::GetDefaultUIMaterial() );
		pTARG->FXI()->InvalidateStateBlock();
		{
			static float gfspinner = 0.0f;
			float fx = sinf( gfspinner );
			float fy = cosf( gfspinner );

			int ilen = 16;
			int ibaseX = itx1-ilen;
			int ibaseY = ity1-ilen;
			lev2::VtxWriter<SVtxV12C4T16> vw;

			vw.Lock( pTARG, & GfxEnv::GetSharedDynamicVB(), 2 );
			{
				u32 ucolor = 0xffffffff;
				ork::CVector2 uvZ( 0.0f,0.0f );
				float fZ = 0.0f;
				
				lev2::SVtxV12C4T16 v0( CVector3(float(ibaseX-int(fx*ilen)),float(ibaseY-int(fy*ilen)), fZ), uvZ, ucolor );
				lev2::SVtxV12C4T16 v1( CVector3(float(ibaseX+int(fx*ilen)),float(ibaseY+int(fy*ilen)), fZ), uvZ, ucolor );
				
				vw.AddVertex( v0 );
				vw.AddVertex( v1 );
			}
			vw.UnLock( pTARG );
			pTARG->MTXI()->PushUIMatrix();
			pTARG->BindMaterial( & UiMat );
			pTARG->GBI()->DrawPrimitive(vw,lev2::EPRIM_LINES,2);
			pTARG->BindMaterial( 0 );
			pTARG->MTXI()->PopUIMatrix();
			gfspinner += (PI2/60.0f);

		}
		pTARG->PopModColor();
		pTARG->PushModColor( CColor4::Yellow() );
		{

			float gfspinner = CSystem::GetRef().GetLoResRelTime()*PI;
			float fx = sinf( gfspinner );
			float fy = cosf( gfspinner );

			int ilen = 16;
			int ibaseX = itx1-ilen;
			int ibaseY = ity1-ilen;

			lev2::VtxWriter<SVtxV12C4T16> vw;

			vw.Lock( pTARG, & GfxEnv::GetSharedDynamicVB(), 2 );
			{
				u32 ucolor = 0xffff00ff;
				ork::CVector2 uvZ( 0.0f,0.0f );
				float fZ = 0.0f;
				//pTARG->IMI()->DrawLine( ibaseX-int(fx*ilen),ibaseY-int(fy*ilen), ibaseX+int(fx*ilen),ibaseY+int(fy*ilen) );
				lev2::SVtxV12C4T16 v0( CVector3(float(ibaseX-int(fx*ilen)),float(ibaseY-int(fy*ilen)), fZ), uvZ, ucolor );
				lev2::SVtxV12C4T16 v1( CVector3(float(ibaseX+int(fx*ilen)),float(ibaseY+int(fy*ilen)), fZ), uvZ, ucolor );
				
				vw.AddVertex( v0 );
				vw.AddVertex( v1 );
			}
			vw.UnLock( pTARG );
			pTARG->MTXI()->PushUIMatrix();
			pTARG->BindMaterial( & UiMat );
			pTARG->GBI()->DrawPrimitive(vw,lev2::EPRIM_LINES,2);
			pTARG->BindMaterial( 0 );
			pTARG->MTXI()->PopUIMatrix();

		}
		pTARG->PopModColor();
		/////////////////////////////////////////////////
		if( draw_pickbuffer && mEditor.GetActiveSceneInst() )
		{
			//pTARG->IMI()->QueFlush( false );
			ent::ESceneInstMode emode = mEditor.GetActiveSceneInst()->GetSceneInstMode();
	
			Texture *ptex	= (emode == ent::ESCENEMODE_RUN )
							? pplaytex
							: ppaustex;


			static lev2::GfxMaterialUITextured UiMat(pTARG);

			if( mpPickBuffer )
			{
				if( mpPickBuffer->mpPickRtGroup )
				{
					ptex = mpPickBuffer->mpPickRtGroup->GetMrt(0)->GetTexture();
					if( mpPickBuffer->mpPickRtGroup->GetMrt(0)->GetMaterial() )
					{
						pTARG->BindMaterial( mpPickBuffer->mpPickRtGroup->GetMrt(0)->GetMaterial() );
					}
				}
				UiMat.SetTexture( lev2::ETEXDEST_DIFFUSE, ptex );
			}
			else
			{
				pTARG->BindMaterial( & UiMat );
			}
			UiMat.mRasterState.SetBlending( lev2::EBLENDING_OFF );
			pTARG->PushModColor( CColor4::White() );
			{
				const int ksize = 256;
				lev2::VtxWriter<SVtxV12C4T16> vw;

				vw.Lock( pTARG, & GfxEnv::GetSharedDynamicVB(), 6 );
				{
					float fZ = 0.0f;
					u32 ucolor = 0xffffffff;
					f32 x0 = 4.0f;
					f32 y0 = f32(ity1-ksize);
					f32 w0 = f32(ksize+4);
					f32 h0 = f32(ksize-18);
					f32 x1 = x0+w0;
					f32 y1 = y0+h0;
					ork::CVector2 uv0( 0.0f,1.0f );
					ork::CVector2 uv1( 1.0f,1.0f );
					ork::CVector2 uv2( 1.0f,0.0f );
					ork::CVector2 uv3( 0.0f,0.0f );
					ork::CVector3 vv0( x0,y0,fZ );
					ork::CVector3 vv1( x1,y0,fZ );
					ork::CVector3 vv2( x1,y1,fZ );
					ork::CVector3 vv3( x0,y1,fZ );
					
					lev2::SVtxV12C4T16 v0( vv0, uv0, ucolor );
					lev2::SVtxV12C4T16 v1( vv1, uv1, ucolor );
					lev2::SVtxV12C4T16 v2( vv2, uv2, ucolor );
					lev2::SVtxV12C4T16 v3( vv3, uv3, ucolor );
					
					vw.AddVertex( v0 );
					vw.AddVertex( v1 );
					vw.AddVertex( v2 );

					vw.AddVertex( v2 );
					vw.AddVertex( v3 );
					vw.AddVertex( v0 );
				}
				vw.UnLock( pTARG );
				pTARG->MTXI()->PushUIMatrix();
				pTARG->GBI()->DrawPrimitive(vw,lev2::EPRIM_TRIANGLES,6);
				pTARG->MTXI()->PopUIMatrix();
				//pTARG->IMI()->QueFlush( false );
			}
			pTARG->PopModColor();
			pTARG->BindMaterial( 0 );

			if( emode == ent::ESCENEMODE_RUN )
			{
				ork::lev2::DrawHudEvent drawHudEvent(pTARG, 1);
				if( mEditor.GetActiveSceneInst()->GetApplication() ) mEditor.GetActiveSceneInst()->GetApplication()->Notify(&drawHudEvent);
			}
		}
		pTARG->BindMaterial( 0 );
		/////////////////////////////////////////////////
		/////////////////////////////////////////////////
		/////////////////////////////////////////////////
		int iy = 4;
		int ix = 4;
		for( orkmap<std::string,SceneEditorVPToolHandler*>::const_iterator it=mToolHandlers.begin(); it!=mToolHandlers.end(); it++ )
		{
			SceneEditorVPToolHandler* phandler = (*it).second;
			bool bhilite = (phandler == mpCurrentHandler);
			phandler->DrawToolIcon( pTARG, ix, iy, bhilite );
			iy += 36;
		}	
		/////////////////////////////////////////////////
		/////////////////////////////////////////////////
		//CPerformanceTracker::GetRef().Draw(pTARG);
		/////////////////////////////////////////////////
		/////////////////////////////////////////////////
	}
	//pTARG->MTXI()->PopUIMatrix();

	pTARG->MTXI()->PopPMatrix(); // back to ortho
	pTARG->MTXI()->PopVMatrix(); // back to ortho
	pTARG->MTXI()->PopMMatrix(); // back to ortho

	if( mActiveCamera )
	{
		mActiveCamera->draw( pTARG );
	}
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawGrid( ork::lev2::RenderContextFrameData& fdata )
{	switch( mGridMode )
	{	case 0:
			ManipManager().Grid().SetGridMode( lev2::Grid3d::EGRID_XZ );
			ManipManager().Grid().Calc( *fdata.GetCameraData() );
			break;
		case 1:
			ManipManager().Grid().SetGridMode( lev2::Grid3d::EGRID_XZ );
			ManipManager().Grid().Calc( *fdata.GetCameraData() );
			ManipManager().Grid().Render( fdata );
			break;
		case 2:
			ManipManager().Grid().SetGridMode( lev2::Grid3d::EGRID_XY );
			ManipManager().Grid().Calc( *fdata.GetCameraData() );
			ManipManager().Grid().Render( fdata );
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawManip(ork::lev2::RenderContextFrameData& fdata, ork::lev2::GfxTarget* pProxyTarg )
{
	const CCameraData* pcamdata = fdata.GetCameraData();
	if( 0 == pcamdata ) return;
	const CameraCalcContext& ccctx = fdata.GetCameraCalcCtx();

	ork::lev2::GfxTarget* pOutputTarget = fdata.GetTarget();
	pOutputTarget->MTXI()->PushPMatrix( ccctx.mPMatrix );
	pOutputTarget->MTXI()->PushVMatrix( ccctx.mVMatrix );
	pOutputTarget->MTXI()->PushMMatrix( CMatrix4::Identity );
	{	switch( ManipManager().GetUIMode() )
		{	case CManipManager::EUIMODE_MANIP_WORLD_TRANSLATE:
			case CManipManager::EUIMODE_MANIP_LOCAL_ROTATE:
			{
				GetRenderer()->SetTarget( pProxyTarg );
				fdata.SetTarget( pProxyTarg );

				///////////////////////////////////////
				const CVector4 & ScreenXNorm = pProxyTarg->MTXI()->GetScreenRightNormal();

				CMatrix4 MatW;
				ManipManager().GetCurTransform().GetMatrix(MatW);
				const CVector4 V0 = MatW.GetTranslation();
				const CVector4 V1 = V0 + ScreenXNorm*CReal(30.0f);
				CVector2 VP( CReal(pProxyTarg->GetW()), CReal(pProxyTarg->GetH()) );
				const CCameraData *camdat = pProxyTarg->GetRenderContextFrameData()->GetCameraData();
				CVector3 Pos = MatW.GetTranslation();
				CVector3 UpVector;
				CVector3 RightVector;
				camdat->GetPixelLengthVectors( Pos, VP, UpVector, RightVector );
				CReal rscale = RightVector.Mag();//CReal(100.0f);
				//printf( "OUTERmanip rscale<%f>\n", rscale );

				static float fRSCALE = 1.0f;
				
				if( fdata.IsPickMode() )
				{
					ManipManager().SetViewScale( fRSCALE );
				}
				else
				{
					float fW = float(pOutputTarget->GetW());
					float fH = float(pOutputTarget->GetH());
					fRSCALE = ManipManager().CalcViewScale( fW, fH, camdat );
					ManipManager().SetViewScale( fRSCALE );
				}


				///////////////////////////////////////

				ManipManager().Setup( GetRenderer() );
				ManipManager().Queue( GetRenderer() );
				GetRenderer()->SetTarget( pOutputTarget );
				break;
			}
		}
	}
	pOutputTarget->MTXI()->PopPMatrix();
	pOutputTarget->MTXI()->PopVMatrix();
	pOutputTarget->MTXI()->PopMMatrix();
}

///////////////////////////////////////////////////////////////////////////////

void SceneEditorVP::SetupLighting( lev2::HeadLightManager& hlmgr, lev2::RenderContextFrameData& FrameData )
{	///////////////////////////////////////////////////////////
	// setup headlight
	///////////////////////////////////////////////////////////
	FrameData.SetLightManager(&hlmgr.mHeadLightManager);
	///////////////////////////////////////////////////////////
	// override with lightmanager in scene if one exists
	///////////////////////////////////////////////////////////
	if( mEditor.mpScene )
	{	if( mEditor.GetActiveSceneInst() )
		{	
			ork::ent::LightingManagerComponentInst* lmi = mEditor.GetActiveSceneInst()->FindTypedSceneComponent<ent::LightingManagerComponentInst>();

			if( lmi )
			{
				ork::lev2::LightManager& lightmanager = lmi->GetLightManager();
			
				const CCameraData* cdata = FrameData.GetCameraData();
				lightmanager.EnumerateInFrustum( cdata->GetFrustum() );
				if( lightmanager.mLightsInFrustum.size() )
				{
					FrameData.SetLightManager( & lightmanager );
				}
			}
		}
	}
	///////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::DrawSpinner(lev2::RenderContextFrameData & FrameData)
{
	bool bhasfocus = HasKeyboardFocus();
	float fw = FrameData.GetDstRect().miW;
	float fh = FrameData.GetDstRect().miH;
	ork::CMatrix4 mtxP = FrameData.GetTarget()->MTXI()->Ortho( 0.0f, fw, 0.0f, fh, 0.0f, 1.0f );
	//GfxEnv::SetUIColorMode( ork::lev2::EUICOLOR_MOD );
	GfxMaterialUI matui( FrameData.GetTarget() );
	FrameData.GetTarget()->BindMaterial( & matui );
	FrameData.GetTarget()->PushModColor( bhasfocus ? ork::CColor4::Red() : ork::CColor4::Black() );
	FrameData.GetTarget()->MTXI()->PushPMatrix( mtxP ); 
	FrameData.GetTarget()->MTXI()->PushVMatrix( ork::CMatrix4::Identity ); 
	FrameData.GetTarget()->MTXI()->PushMMatrix( ork::CMatrix4::Identity ); 
	{
		DynamicVertexBuffer<SVtxV12C4T16>& vb = GfxEnv::GetSharedDynamicVB();

		int ivcount = 16;

		VtxWriter<SVtxV12C4T16> vwriter;
		vwriter.Lock( FrameData.GetTarget(), &vb, ivcount );

		float fx1 = fw-1.0f;
		float fy1 = fh-1.0f;

		vwriter.AddVertex( SVtxV12C4T16(0.0f,0.0f,0.0f,  0.0f, 0.0f, 0xffffffff ) );
		vwriter.AddVertex( SVtxV12C4T16(fx1,0.0f,0.0f,  0.0f, 0.0f, 0xffffffff ) );
		vwriter.AddVertex( SVtxV12C4T16(0.0f,1.0f,0.0f,  0.0f, 0.0f, 0xffffffff ) );
		vwriter.AddVertex( SVtxV12C4T16(fx1,1.0f,0.0f,  0.0f, 0.0f, 0xffffffff ) );

		vwriter.AddVertex( SVtxV12C4T16(fx1,0.0f,0.0f,  0.0f, 0.0f, 0xffffffff ) );
		vwriter.AddVertex( SVtxV12C4T16(fx1,fy1,0.0f,  0.0f, 0.0f, 0xffffffff ) );
		vwriter.AddVertex( SVtxV12C4T16(fx1-1.0f,0.0f,0.0f,  0.0f, 0.0f, 0xffffffff ) );
		vwriter.AddVertex( SVtxV12C4T16(fx1-1.0f,fy1,0.0f,  0.0f, 0.0f, 0xffffffff ) );

		vwriter.AddVertex( SVtxV12C4T16(fx1,fy1,0.0f,  0.0f, 0.0f, 0xffffffff ) );
		vwriter.AddVertex( SVtxV12C4T16(0.0f,fy1,0.0f,  0.0f, 0.0f, 0xffffffff ) );
		vwriter.AddVertex( SVtxV12C4T16(fx1,fy1-1.0f,0.0f,  0.0f, 0.0f, 0xffffffff ) );
		vwriter.AddVertex( SVtxV12C4T16(0.0f,fy1-1.0f,0.0f,  0.0f, 0.0f, 0xffffffff ) );

		vwriter.AddVertex( SVtxV12C4T16(0.0f,fy1,0.0f,  0.0f, 0.0f, 0xffffffff ) );
		vwriter.AddVertex( SVtxV12C4T16(0.0f,0.0f,0.0f,  0.0f, 0.0f, 0xffffffff ) );
		vwriter.AddVertex( SVtxV12C4T16(0.0f,fy1,0.0f,  0.0f, 0.0f, 0xffffffff ) );
		vwriter.AddVertex( SVtxV12C4T16(1.0f,1.0f,0.0f,  0.0f, 0.0f, 0xffffffff ) );

		vwriter.UnLock(FrameData.GetTarget());

		FrameData.GetTarget()->GBI()->DrawPrimitive( vwriter, ork::lev2::EPRIM_LINES );

	}
	FrameData.GetTarget()->MTXI()->PopPMatrix(); // back to ortho
	FrameData.GetTarget()->MTXI()->PopVMatrix(); // back to ortho
	FrameData.GetTarget()->MTXI()->PopMMatrix(); // back to ortho
	FrameData.GetTarget()->PopModColor(  );
	FrameData.GetTarget()->BindMaterial( 0 );
}

} // namespace tool
} // namespace ork

///////////////////////////////////////////////////////////////////////////

