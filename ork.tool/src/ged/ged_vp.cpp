////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/IObjectPropertyObject.h>
#include <orktool/qtui/gfxbuffer.h>
///////////////////////////////////////////////////////////////////////////////
template class ork::lev2::CPickBuffer<ork::tool::ged::GedVP>;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
template<> 
void CPickBuffer<ork::tool::ged::GedVP>::Draw( void )
{	
    mPickIds.clear();
    lev2::PickFrameTechnique pktek;
	mpViewport->PushFrameTechnique( & pktek );
	ork::lev2::GfxTarget *pTEXTARG = GetContext();
	///////////////////////////////////////////////////////////////////////////
	int itx0 = GetContextX();	int itx1 = GetContextX()+GetContextW();
	int ity0 = GetContextY();	int ity1 = GetContextY()+GetContextH();
	///////////////////////////////////////////////////////////////////////////
	ork::lev2::RenderContextFrameData framedata; //
	pTEXTARG->SetRenderContextFrameData( & framedata );
	framedata.SetRenderingMode( ork::lev2::RenderContextFrameData::ERENDMODE_STANDARD );
	framedata.SetTarget( pTEXTARG );
	SRect tgt_rect( 0, 0, pTEXTARG->GetW(), pTEXTARG->GetH() );
	framedata.SetDstRect( tgt_rect );
	pTEXTARG->SetRenderContextFrameData( & framedata );
	///////////////////////////////////////////////////////////////////////////
	SRect VPRect( itx0, ity0, itx1, ity1 );
	pTEXTARG->FBI()->PushViewport( VPRect );
	pTEXTARG->FBI()->PushScissor( VPRect );
	BeginFrame();
	{
		pTEXTARG->FBI()->SetRtGroup( mpPickRtGroup );	// Enable Mrt
		pTEXTARG->FBI()->EnterPickState(this);
		///////////////////////////////////////////////////////////////////////////
		SRect VPRect( itx0, ity0, itx1, ity1 );
		pTEXTARG->FBI()->PushViewport( VPRect );
			pTEXTARG->MTXI()->PushPMatrix( CMatrix4::Identity );
			pTEXTARG->MTXI()->PushVMatrix( CMatrix4::Identity );
			pTEXTARG->MTXI()->PushMMatrix( CMatrix4::Identity );
				//printf( "DRAWPICK\n" );
				mpViewport->ExtDraw( pTEXTARG );
			pTEXTARG->MTXI()->PopPMatrix();
			pTEXTARG->MTXI()->PopVMatrix();
			pTEXTARG->MTXI()->PopMMatrix();
		pTEXTARG->FBI()->PopViewport();
		///////////////////////////////////////////////////////////////////////////
		pTEXTARG->FBI()->LeavePickState();
		pTEXTARG->FBI()->SetRtGroup(0);
	}
	EndFrame();
	pTEXTARG->FBI()->PopViewport();
	pTEXTARG->FBI()->PopScissor();
	pTEXTARG->SetRenderContextFrameData( 0 );
	///////////////////////////////////////////////////////////////////////////
	SetDirty( false );
	mpViewport->PopFrameTechnique( );
}
}}
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { 

///////////////////////////////////////////////////////////////////////////////
uint32_t ged::GedVP::AssignPickId(GedObject*pobj)
{   return mpPickBuffer->AssignPickId(pobj);
}
///////////////////////////////////////////////////////////////////////////////
namespace ged {
///////////////////////////////////////////////////////////////////////////////
static const int kvppickdimw = 1024;
static const int kvppickdimh = 1024;
static const int kscrollw = 32;
orkset<GedVP*> GedVP::gAllViewports;
///////////////////////////////////////////////////////////////////////////////
GedVP::GedVP( const std::string & name, ObjModel& model )
	: CUIViewport( name, 0, 0, 0, 0, CColor3::Black(), 0.0f )
	, mModel( model )
	, mWidget( model )
	, mpPickBuffer( 0 )
	, mpActiveNode( 0 )
	, miScrollY( 0 )
	, mpMouseOverNode(0)
	, mpBasicFrameTek( 0 )
{
	mWidget.SetViewport( this );

	mpBasicFrameTek = new lev2::BasicFrameTechnique;
	PushFrameTechnique( mpBasicFrameTek );

	gAllViewports.insert( this );

	object::Connect( & model.GetSigRepaint(), & mWidget.GetSlotRepaint() );
	object::Connect( & model.GetSigModelInvalidated(), & mWidget.GetSlotModelInvalidated() );
}
GedVP::~GedVP()
{
	orkset<GedVP*>::iterator it = gAllViewports.find( this );

	if( it != gAllViewports.end() )
	{
		gAllViewports.erase( it );
	}
}
///////////////////////////////////////////////////////////////////////////////
void GedVP::DoDraw(  )
{
	//orkprintf( "GedVP::DoDraw()\n" );
	
	if( 0 == mpPickBuffer )
	{
		mpPickBuffer = new lev2::CPickBuffer<GedVP>(	mpTarget->FBI()->GetThisBuffer(), 
														this,
														0, 0, kvppickdimw, kvppickdimh,
														lev2::PickBufferBase::EPICK_FACE_VTX
												   );
		mpPickBuffer->RefClearColor().SetRGBAU32( 0 );
		mpPickBuffer->CreateContext();
		mpPickBuffer->GetContext()->FBI()->SetClearColor( CColor4(0.0f,0.0f,0.0f,0.0f) );
	}

	if( GetFrameTechnique() )
	{
		//orkprintf( "BEG: GedVP::DoDraw::2()\n" );
		static const bool bfakepik = false;
		if( bfakepik )
		{
			lev2::PickFrameTechnique pktek;
			this->PushFrameTechnique( & pktek );
			ork::lev2::GfxTarget *pTEXTARG = mpTarget;
			///////////////////////////////////////////////////////////////////////////
			int itx0 = GetX();	int itx1 = GetX()+GetW();
			int ity0 = GetY();	int ity1 = GetY()+GetH();
			///////////////////////////////////////////////////////////////////////////
			ork::lev2::RenderContextFrameData framedata; //
			pTEXTARG->SetRenderContextFrameData( & framedata );
			//framedata.SetRenderingMode( ork::lev2::RenderContextFrameData::ERENDMODE_PICK );
			framedata.SetTarget( pTEXTARG );
			SRect tgt_rect( 0, 0, pTEXTARG->GetW(), pTEXTARG->GetH() );
			framedata.SetDstRect( tgt_rect );
			pTEXTARG->SetRenderContextFrameData( & framedata );
			///////////////////////////////////////////////////////////////////////////
			//pTEXTARG->FBI()->SetRtGroup( mpPickRtGroup );	// Enable Mrt
			this->BeginFrame(pTEXTARG);
			pTEXTARG->FBI()->EnterPickState(mpPickBuffer);
			///////////////////////////////////////////////////////////////////////////
			SRect VPRect( itx0, ity0, itx1, ity1 );
			pTEXTARG->FBI()->PushViewport( VPRect );
				pTEXTARG->MTXI()->PushPMatrix( CMatrix4::Identity );
				pTEXTARG->MTXI()->PushVMatrix( CMatrix4::Identity );
				pTEXTARG->MTXI()->PushMMatrix( CMatrix4::Identity );
					//this->ExtDraw( pTEXTARG );
				{
					FrameRenderer the_renderer( this );
					the_renderer.GetFrameData().SetTarget( mpTarget );
					lev2::UiViewportRenderTarget rt( this );
					the_renderer.GetFrameData().PushRenderTarget( & rt );
					//the_renderer.GetFrameData().SetRenderingMode( RenderContextFrameData::ERENDMODE_PICK );
					GetFrameTechnique()->Render( the_renderer );
					the_renderer.GetFrameData().PopRenderTarget();
				}
				pTEXTARG->MTXI()->PopPMatrix();
				pTEXTARG->MTXI()->PopVMatrix();
				pTEXTARG->MTXI()->PopMMatrix();
			pTEXTARG->FBI()->PopViewport();
			///////////////////////////////////////////////////////////////////////////
			pTEXTARG->FBI()->LeavePickState();
			EndFrame(pTEXTARG);
			//pTEXTARG->FBI()->SetRtGroup(0);
			pTEXTARG->SetRenderContextFrameData( 0 );
			///////////////////////////////////////////////////////////////////////////
			//SetDirty( false );
			this->PopFrameTechnique( );

		}	
		else
		{
			FrameRenderer the_renderer( this );
			lev2::UiViewportRenderTarget rt( this );
			the_renderer.GetFrameData().PushRenderTarget( & rt );
			the_renderer.GetFrameData().SetTarget( mpTarget );
			//printf( "GED::FTEKRENDER\n" );
			GetFrameTechnique()->Render( the_renderer );
			the_renderer.GetFrameData().PopRenderTarget();
		}
		//orkprintf( "END: GedVP::DoDraw::2()\n" );
	}
}

void GedVP::FrameRenderer::Render()
{
	//OrkAssert(false);
	//orkprintf( "BEG: GedVP::FrameRenderer::Render()\n" );

	ork::tool::ged::ObjModel::FlushAllQueues();
	bool bval = true;

	ork::lev2::GfxTarget* pTARG = GetFrameData().GetTarget();
	bool bispick = GetFrameData().IsPickMode(); 


	//////////////////////////////////////////////////
	// Compute Scoll Transform
	//////////////////////////////////////////////////

	ork::CMatrix4 matV;
	matV.SetTranslation( 0.0f, float(mpViewport->miScrollY), 0.0f );

	//////////////////////////////////////////////////

	const ork::rtti::ICastable* pobj = mpViewport->mModel.CurrentObject();

	int iw = pTARG->GetRenderContextFrameData()->GetDstRect().miW;
	int ih = pTARG->GetRenderContextFrameData()->GetDstRect().miH;

	//pTARG->FBI()->PushScissor( SRect( 0, 0, pTARG->GetW(), GetH() ) );
	pTARG->FBI()->PushViewport( pTARG->GetRenderContextFrameData()->GetDstRect() );
	
	mpViewport->Clear(); // FrameData.GetTarget() );

	pTARG->MTXI()->PushMMatrix( matV );
	{
		int itx = pTARG->GetRenderContextFrameData()->GetDstRect().miX;
		int itw = pTARG->GetRenderContextFrameData()->GetDstRect().miW;
		int ity = pTARG->GetRenderContextFrameData()->GetDstRect().miY;
		int ith = pTARG->GetRenderContextFrameData()->GetDstRect().miH;
		////////////////////////////////////////////////////////
		// draw some 2d crap
		////////////////////////////////////////////////////////

		float aspect = float(iw)/float(ih);

		if( false == bispick )
		{
			//uimat.SetUIColorMode( ork::lev2::EUICOLOR_MODVTX );
			//ork::lev2::NUI::EBoxStyle eBoxStyle( ork::lev2::NUI::EBOXSTYLE_FILLED_GREY );
			//ork::lev2::NUI::DrawStyledBox( pTARG, itx+8, ity+8, itx+(itw-8), ity+(ith-8), eBoxStyle );
		}
		//pTARG->IMI()->QueFlush();

		if( pobj )
		{
			GedWidget* pw = mpViewport->mModel.GetGedWidget();

			pw->Draw( pTARG, mpViewport->miScrollY );

		}
		else if( false == bispick )
		{
			//pTARG->IMI()->DrawLine( s16(itx), s16(ity), s16(itx+itw), s16(ity+ith), 0x00ffffff );
			//pTARG->IMI()->DrawLine( s16(itx+itw), s16(ity), s16(itx), s16(ity+ith), 0x00ffffff );
			//pTARG->IMI()->QueFlush();
		}
	}
	pTARG->MTXI()->PopMMatrix();
	pTARG->FBI()->PopViewport();
	//orkprintf( "END: GedVP::FrameRenderer::Render()\n" );
}
///////////////////////////////////////////////////////////////////////////////
lev2::EUIHandled GedVP::UIEventHandler( lev2::CUIEvent *pEV )
{
	ork::tool::ged::ObjModel::FlushAllQueues();
	int ix = pEV->miX;
	int iy = pEV->miY;
	float fx = float(ix) / float(GetW());
	float fy = float(iy) / float(GetH());

	lev2::GetPixelContext ctx;
	ctx.miMrtMask = (1<<0) | (1<<1); // ObjectID and ObjectUVD
	ctx.mUsage[0] = lev2::GetPixelContext::EPU_PTR32;
	ctx.mUsage[1] = lev2::GetPixelContext::EPU_FLOAT;

	QInputEvent* qip = (QInputEvent*) pEV->mpBlindEventData;

	bool bisshift = pEV->mbSHIFT;

	switch( pEV->miEventCode )
	{
	case lev2::UIEV_KEY:
		{
			int mikeyc = pEV->miKeyCode;
			if( mikeyc == '!' )
			{
				mWidget.IncrementSkin();
			}
			break;
		}
		case lev2::UIEV_MOUSEWHEEL:
		{
			QWheelEvent* qem = (QWheelEvent*) qip;

			//GetPixel( ix, iy, ctx );
			//ork::Object *pobj = ctx.GetObject(0);			

			int iscrollamt = bisshift ? 256 : 32;

			//if( pobj )
			{
				int idelta = qem->delta();

				if( idelta > 0 )
				{
					miScrollY += iscrollamt;
					if( miScrollY > 0 )
						miScrollY = 0;
				}
				else if( idelta < 0 )
				{
					////////////////////////////////////
					// iwh = 500
					// irh = 200
					// ism = 300 // 0 
					////////////////////////////////////

					////////////////////////////////////
					// iwh = 300
					// irh = 500
					// ism = -200
					////////////////////////////////////

					int iwh = GetH();					// 500
					int irh = mWidget.GetRootHeight();	// 200
					int iscrollmin = (iwh-irh);			// 300

					if( iscrollmin > 0 )
					{
						iscrollmin = 0;
					}

					miScrollY -= iscrollamt;
					if( miScrollY < iscrollmin )
					{
						miScrollY = iscrollmin;
					}
					
				}
			}
			break;
		}
		case lev2::UIEV_MOVE:
		{	QMouseEvent* qem = (QMouseEvent*) qip;
			QPoint mypos = qem->pos();
			mypos.setY( mypos.y() - miScrollY );
			QMouseEvent myme( qem->type(), mypos, qem->button(), qem->buttons(), qem->modifiers() );
			static int gctr = 0;
			if( 0 == gctr%4 ) 
			{	GetPixel( ix, iy, ctx );
				rtti::ICastable *pobj = ctx.GetObject(mpPickBuffer,0);
				if( 0 ) //TODO pobj )
				{	GedObject *pnode = rtti::autocast(pobj);
					if( pnode )
					{	//pnode->mouseMoveEvent( & myme );
						mpMouseOverNode = pnode;
					}
				}
			}
			gctr++;
			break;
		}
		case lev2::UIEV_DRAG:
		{	QMouseEvent* qem = (QMouseEvent*) qip;
			QPoint mypos = qem->pos();
			mypos.setY( mypos.y() - miScrollY );
			QMouseEvent myme( qem->type(), mypos, qem->button(), qem->buttons(), qem->modifiers() );
			//GetPixel( ix, iy, ctx );
			//ork::Object *pobj = ctx.GetObject(0);
			//if( pobj )
			{	//GedItemNode *pnode = rtti::autocast(pobj);
				//OrkAssert(pnode);
				if( mpActiveNode )
				{	mpActiveNode->mouseMoveEvent( & myme );
				}
				else
				{
					//pnode->mouseMoveEvent( & myme );
				}
				break;
			}
			break;
		}
		case lev2::UIEV_PUSH:
		case lev2::UIEV_RELEASE:
		case lev2::UIEV_DOUBLECLICK:
		{
			QMouseEvent* qem = (QMouseEvent*) qip;

			QPoint mypos = qem->pos();
			mypos.setY( mypos.y() - miScrollY );

			QMouseEvent myme( qem->type(), mypos, qem->button(), qem->buttons(), qem->modifiers() );

			GetPixel( ix, iy, ctx );

			ork::rtti::ICastable* pobj = ctx.GetObject(mpPickBuffer,0);
			orkprintf( "Object<%p>\n", pobj );

			/////////////////////////////////////
			// test object against known set
			if( false == IsObjInSet(pobj) ) pobj = 0;
			/////////////////////////////////////

			if( pobj )
			if(GedObject *pnode = ork::rtti::autocast(pobj))
			{
				switch( pEV->miEventCode )
				{
					case lev2::UIEV_PUSH:
						mpActiveNode = pnode;
						pnode->mousePressEvent( & myme );
						break;
					case lev2::UIEV_RELEASE:
						if( mpActiveNode ) mpActiveNode->mouseReleaseEvent( & myme );
						mpActiveNode = 0;
						break;
					case lev2::UIEV_DOUBLECLICK:
						pnode->mouseDoubleClickEvent( & myme );
						break;
				}
			}
		}
		default:
			break;
	}
	return lev2::EUI_HANDLED;
}
///////////////////////////////////////////////////////////////////////////////
void GedVP::GetPixel( int ix, int iy, lev2::GetPixelContext& ctx )
{
	float fx = float(ix) / float(kvppickdimw);
	float fy = float(iy) / float(kvppickdimh);
	/////////////////////////////////////////////////////////////
	if( mpPickBuffer )
	{	ctx.mRtGroup = mpPickBuffer->mpPickRtGroup;
		ctx.mAsBuffer = mpPickBuffer;
		/////////////////////////////////////////////////////////////
		mpPickBuffer->Draw();
		/////////////////////////////////////////////////////////////
		int iW = mpPickBuffer->GetContext()->GetW();
		int iH = mpPickBuffer->GetContext()->GetH();
		/////////////////////////////////////////////////////////////
		mpPickBuffer->GetContext()->FBI()->SetViewport( 0,0,iW,iH );
		mpPickBuffer->GetContext()->FBI()->SetScissor( 0,0,iW,iH );
		/////////////////////////////////////////////////////////////
		mpPickBuffer->GetContext()->FBI()->GetPixel( CVector4( fx, fy, 0.0f ), ctx );
		/////////////////////////////////////////////////////////////
	}
	/////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////////
static std::set<void*>& GetObjSet()
{
	static std::set<void*> gObjSet;
	return gObjSet;
}
void ClearObjSet()
{
	GetObjSet().clear();
}
void AddToObjSet(void*pobj)
{
	GetObjSet().insert(pobj);
}
bool IsObjInSet(void*pobj)
{
	bool rval = false;
	rval = (GetObjSet().find(pobj)!=GetObjSet().end());
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
