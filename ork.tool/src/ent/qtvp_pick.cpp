#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/opq.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/editor/qtui_scenevp.h>
#include <pkg/ent/editor/qtvp_uievh.h>
#include <ork/kernel/future.hpp>
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

DeferredPickOperationContext::DeferredPickOperationContext()
	: miX(0)
	, miY(0)
	, is_left(false)
	, is_mid(false)
	, is_right(false)
	, is_ctrl(false)
	, is_shift(false)
	, mpCastable(nullptr)
	, mOnPick(nullptr)
	, mHandler(nullptr)
	, mViewport(nullptr)

{
	mState = 0;
}

///////////////////////////////////////////////////////////////////////////////
static Opq gPickOPQ(1,"PickOpQ");
void OuterPickOp( DeferredPickOperationContext* pickctx )
{
	assert(pickctx->mViewport!=nullptr);
	if( pickctx->mViewport==nullptr)
		return;

	auto outer_op = [=]()
	{	
		AssertOnOpQ2( gPickOPQ );
		////////////
		// stop updates, and wait for mainthread to acknowledge
		////////////
		gUpdateStatus.SetState(EUPD_STOP);
		UpdateSerialOpQ().sync();
		////////////
		auto op_pick = [=]()
		{	AssertOnOpQ2( MainThreadOpQ() );
			pickctx->mState = 1;
			SceneEditorVP* pVP = pickctx->mViewport;
			assert(pVP!=nullptr);
			lev2::GetPixelContext ctx;
			ctx.miMrtMask = 3;
			ctx.mUsage[0] = lev2::GetPixelContext::EPU_PTR32;
			ctx.mUsage[1] = lev2::GetPixelContext::EPU_FLOAT;
			pVP->GetPixel( pickctx->miX, pickctx->miY, ctx );
			pickctx->mpCastable = ctx.GetObject(pVP->GetPickBuffer(),0);
			//printf( "GOTOBJ<%p>\n", pickctx->mpCastable );
			if( pickctx->mOnPick )
			{	auto on_pick = [=]()
				{ 	pickctx->mOnPick(pickctx);
					pickctx->mState = 2;
					gUpdateStatus.SetState(EUPD_START);
				};
				Op(on_pick).QueueASync(UpdateSerialOpQ());
			}
			else
			{	pickctx->mState = 3;
				gUpdateStatus.SetState(EUPD_START);
			}
			//UpdateSerialOpQ().sync();
		};
		Op(op_pick).QueueASync(MainThreadOpQ());
		////////////
		// restart updates, and wait for mainthread to acknowledge
		////////////
		//gUpdateStatus.SetState(EUPD_START);
		//UpdateSerialOpQ().sync();
		//MainThreadOpQ().sync();
		////////////
	};
	Op(outer_op).QueueASync(gPickOPQ);
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::GetPixel( int ix, int iy, lev2::GetPixelContext& ctx )
{
	if( nullptr == mpPickBuffer ) return;

	float fx = float(ix) / float(miW);
	float fy = float(iy) / float(miH);

	ctx.mRtGroup = mpPickBuffer->mpPickRtGroup;
	ctx.mAsBuffer = mpPickBuffer;

	/////////////////////////////////////////////////////////////
	// force a pick refresh

	mpPickBuffer->Draw();

	/////////////////////////////////////////////////////////////
	int iW = mpPickBuffer->GetContext()->GetW();
	int iH = mpPickBuffer->GetContext()->GetH();
	/////////////////////////////////////////////////////////////
	mpPickBuffer->GetContext()->FBI()->SetViewport( 0,0,iW,iH );
	mpPickBuffer->GetContext()->FBI()->SetScissor( 0,0,iW,iH );
	/////////////////////////////////////////////////////////////

	mpPickBuffer->GetContext()->FBI()->GetPixel( CVector4( fx, fy, 0.0f ), ctx );
}

}} // namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

template<>
void ork::lev2::CPickBuffer<ork::ent::SceneEditorVP>::Draw( void )
{
	AssertOnOpQ2( MainThreadOpQ() );

	ent::SceneInst* psi = mpViewport->GetSceneInst();
	ent::SceneData* pscene = mpViewport->mEditor.mpScene;

	if( nullptr == pscene ) return;
	if( nullptr == psi ) return;
	if( false == mpViewport->mbSceneDisplayEnable ) return;

	///////////////////////////////////////////////////////////////////////////

	mPickIds.clear();
    
	ork::recursive_mutex& glock = lev2::GfxEnv::GetRef().GetGlobalLock();
	glock.Lock(0x777);
	PickFrameTechnique pktek;
	mpViewport->PushFrameTechnique( & pktek );
	GfxTarget *pTEXTARG = GetContext();
	GfxTarget* pPARENTTARG = GetParent()->GetContext();
	ork::lev2::RenderContextFrameData framedata; //
	pTEXTARG->SetRenderContextFrameData( & framedata );
	framedata.SetRenderingMode( ork::lev2::RenderContextFrameData::ERENDMODE_STANDARD );
	framedata.SetTarget( pTEXTARG );
	SRect tgt_rect( 0, 0, mpViewport->GetW(), mpViewport->GetH() );
	framedata.SetDstRect( tgt_rect );
	pTEXTARG->SetRenderContextFrameData( & framedata );
	///////////////////////////////////////////////////////////////////////////
	mpViewport->GetRenderer()->SetTarget( pTEXTARG );
	framedata.SetLightManager(0);
	///////////////////////////////////////////////////////////////////////////
	// use source viewport's W/H for camera matrix computation
	///////////////////////////////////////////////////////////////////////////
	framedata.AddLayer( AddPooledLiteral("All") );
	///////////////////////////////////////////////////////////////////////////
	anyp PassData;
	PassData.Set<orkstack<ent::CompositingPassData>*>( & mpViewport->mCompositingGroupStack );
	framedata.SetUserProperty( "nodes", PassData );
	ent::CompositingPassData compositor_node;
	///////////////////////////////////////////////////////////////////////////
	int itx0 = GetContextX();
	int itx1 = GetContextX()+GetContextW();
	int ity0 = GetContextY();
	int ity1 = GetContextY()+GetContextH();
	///////////////////////////////////////////////////////////////////////////
	// force aspect ratio to that of the parent visible viewport
	//  as opposed to the pickbuffer size
	///////////////////////////////////////////////////////////////////////////
	float fW = mpViewport->GetW();
	float fH = mpViewport->GetH();
	framedata.GetCameraCalcCtx().mfAspectRatio = fW/fH;
	///////////////////////////////////////////////////////////////////////////
	lev2::UiViewportRenderTarget rt( mpViewport );
	framedata.PushRenderTarget( & rt );
	BeginFrame();
	{
		SRect VPRect( itx0, ity0, itx1, ity1 );
		//printf( "itx0<%d> ity0<%d> itx1<%d> ity1<%d> fW<%f> fH<%f>\n", itx0, ity0, itx1, ity1, fW, fH );
		///////////////////////////////////////////////////////////////////////////
		pTEXTARG->FBI()->PushRtGroup( mpPickRtGroup );	// Enable Mrt
		pTEXTARG->FBI()->EnterPickState(this);
		//pTEXTARG->RSI()->SetOverrideBlending(true);
		//pTEXTARG->RSI()->GetOverrideRasterState().muBlending = EBLENDING_OFF;
		//pTEXTARG->RSI()->SetOverrideAlphaTest(true);
		//EXTARG->RSI()->GetOverrideRasterState().muAlphaTest = EALPHATEST_OFF;
		pTEXTARG->FBI()->PushViewport( VPRect );
		pTEXTARG->BindMaterial( GfxEnv::GetDefault3DMaterial() );
		pTEXTARG->PushModColor( CColor4::Yellow() );
		mpViewport->mCompositingGroupStack.push(compositor_node);
		{	////////////////////////////////////////////////
			// update on update q
			////////////////////////////////////////////////
			ork::ent::DrawableBuffer draw_buffer(4);
			auto lamb = [&]()
			{	mpViewport->mSceneView.UpdateRefreshPolicy(framedata,psi);
				draw_buffer.miBufferIndex = 0;
				psi->Update();
				mpViewport->QueueSDLD(&draw_buffer);
			};
			Op(lamb).QueueSync(UpdateSerialOpQ());
			////////////////////////////////////////////////
			const ent::DrawableBuffer* DB = & draw_buffer;
			framedata.SetUserProperty( "DB", anyp(DB) );
			////////////////////////////////////////////////
			mpViewport->RenderSDLD( framedata );
			////////////////////////////////////////////////
		}
		mpViewport->mCompositingGroupStack.pop();
		pTEXTARG->PopModColor();
		pTEXTARG->FBI()->PopRtGroup();
		pTEXTARG->FBI()->PopViewport();
		//pTEXTARG->RSI()->ClearOverrides();
		pTEXTARG->FBI()->LeavePickState();
	}
	EndFrame();
	///////////////////////////////////////////////////////////////////////////
	SetDirty( false );
	pTEXTARG->SetRenderContextFrameData( 0 );
	mpViewport->PopFrameTechnique();
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();

	///////////////////////////////////////////////////////////////////////////
}

