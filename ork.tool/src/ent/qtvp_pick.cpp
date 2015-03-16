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
	SceneEditorVP* viewport = pickctx->mViewport;

	assert(pickctx->mViewport!=nullptr);
	if( pickctx->mViewport==nullptr)
		return;

	const ent::SceneInst* psi = viewport->GetSceneInst();
	const ent::SceneData* pscene = viewport->SceneEditor().mpScene;

	if( nullptr == pscene ) return;
	if( nullptr == psi ) return;
	if( false == viewport->IsSceneDisplayEnabled() ) return;

	auto outer_op = [=]()
	{	
		AssertOnOpQ2( gPickOPQ );
		////////////
		// stop updates, and wait for mainthread to acknowledge
		////////////
		gUpdateStatus.SetState(EUPD_STOP);
		UpdateSerialOpQ().sync();
		////////////
		static auto d_buf = new ork::ent::DrawableBuffer(4);
		ork::lev2::RenderContextFrameData framedata; //

		anyp db_var;
		db_var.Set<const DrawableBuffer*>( d_buf );

		framedata.SetUserProperty( "DB", db_var );

		auto lamb = [&]()
		{	
			AssertOnOpQ2( UpdateSerialOpQ() );

			//viewport->SceneEditorView().UpdateRefreshPolicy(framedata,psi);
			d_buf->miBufferIndex = 0;
			//psi->Update();
			viewport->QueueSceneInstToDb(d_buf);
			////////////
			MainThreadOpQ().sync();
			////////////
			auto op_pick = [&]()
			{	AssertOnOpQ2( MainThreadOpQ() );
				pickctx->mState = 1;
				lev2::GetPixelContext ctx;
				ctx.miMrtMask = 3;
				ctx.mUsage[0] = lev2::GetPixelContext::EPU_PTR32;
				ctx.mUsage[1] = lev2::GetPixelContext::EPU_FLOAT;
				ctx.mUserData.Set<ork::lev2::RenderContextFrameData*>( & framedata );

				viewport->GetPixel( pickctx->miX, pickctx->miY, ctx ); // HERE<<<<<<
				pickctx->mpCastable = ctx.GetObject(viewport->GetPickBuffer(),0);
				printf( "GOTOBJ<%p>\n", pickctx->mpCastable );
				if( pickctx->mOnPick )
				{	
					auto on_pick = [=]()
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
			Op(op_pick).QueueSync(MainThreadOpQ());

		};
		Op(lamb).QueueSync(UpdateSerialOpQ()); // HERE<<<<<<

		//delete d_buf;

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

	mpPickBuffer->Draw(ctx);

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
void ork::lev2::CPickBuffer<ork::ent::SceneEditorVP>::Draw( lev2::GetPixelContext& ctx )
{
	AssertOnOpQ2( MainThreadOpQ() );

	const ent::SceneInst* psi = mpViewport->GetSceneInst();
	ent::SceneData* pscene = mpViewport->mEditor.mpScene;

	if( nullptr == pscene ) return;
	if( nullptr == psi ) return;
	if( false == mpViewport->mbSceneDisplayEnable ) return;


	if( false == ctx.mUserData.IsA<ork::lev2::RenderContextFrameData*>() )
		return;

	auto frame_data = ctx.mUserData.Get<ork::lev2::RenderContextFrameData*>();

	///////////////////////////////////////////////////////////////////////////

	mPickIds.clear();
    
	ork::recursive_mutex& glock = lev2::GfxEnv::GetRef().GetGlobalLock();
	glock.Lock(0x777);
	PickFrameTechnique pktek;
	mpViewport->PushFrameTechnique( & pktek );
	GfxTarget *pTEXTARG = GetContext();
	GfxTarget* pPARENTTARG = GetParent()->GetContext();
	pTEXTARG->SetRenderContextFrameData( frame_data );
	frame_data->SetRenderingMode( ork::lev2::RenderContextFrameData::ERENDMODE_STANDARD );
	frame_data->SetTarget( pTEXTARG );
	SRect tgt_rect( 0, 0, mpViewport->GetW(), mpViewport->GetH() );
	frame_data->SetDstRect( tgt_rect );
	pTEXTARG->SetRenderContextFrameData( frame_data );
	///////////////////////////////////////////////////////////////////////////
	mpViewport->GetRenderer()->SetTarget( pTEXTARG );
	frame_data->SetLightManager(nullptr);
	///////////////////////////////////////////////////////////////////////////
	// use source viewport's W/H for camera matrix computation
	///////////////////////////////////////////////////////////////////////////
	frame_data->AddLayer( AddPooledLiteral("All") );
	///////////////////////////////////////////////////////////////////////////
	anyp PassData;
	PassData.Set<orkstack<ent::CompositingPassData>*>( & mpViewport->mCompositingGroupStack );
	frame_data->SetUserProperty( "nodes", PassData );
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
	frame_data->GetCameraCalcCtx().mfAspectRatio = fW/fH;
	///////////////////////////////////////////////////////////////////////////
	lev2::UiViewportRenderTarget rt( mpViewport );
	frame_data->PushRenderTarget( & rt );
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
		{	
			mpViewport->RenderQueuedScene( * frame_data );
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

