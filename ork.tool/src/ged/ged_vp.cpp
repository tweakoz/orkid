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
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/math/basicfilters.h>
#include <ork/kernel/msgrouter.inl>
///////////////////////////////////////////////////////////////////////////////
template class ork::lev2::CPickBuffer<ork::tool::ged::GedVP>;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
template<>
void CPickBuffer<ork::tool::ged::GedVP>::Draw( lev2::GetPixelContext& ctx )
{
    mPickIds.clear();

    auto tgt = GetContext();
	auto mtxi = tgt->MTXI();
	auto fbi = tgt->FBI();
	auto fxi = tgt->FXI();
	auto rsi = tgt->RSI();

	int irtgw = mpPickRtGroup->GetW();
	int irtgh = mpPickRtGroup->GetH();
	int isurfw = mpViewport->GetW();
	int isurfh = mpViewport->GetH();
	if( irtgw!=isurfw || irtgh!=isurfh )
	{
		//printf( "resize ged pickbuf rtgroup<%d %d>\n", isurfw, isurfh);
		this->SetBufferWidth(isurfw);
		this->SetBufferHeight(isurfh);
		tgt->SetSize(0,0,isurfw,isurfh);
		mpPickRtGroup->Resize(isurfw,isurfh);
	}
	fbi->PushRtGroup(mpPickRtGroup);
	fbi->EnterPickState(this);
		ui::DrawEvent drwev(tgt);
		mpViewport->RePaintSurface(drwev);
	fbi->LeavePickState();
	fbi->PopRtGroup();
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
static const int kscrollw = 32;
orkset<GedVP*> GedVP::gAllViewports;
///////////////////////////////////////////////////////////////////////////////
GedVP::GedVP( const std::string & name, ObjModel& model )
	: ui::Surface( name, 0, 0, 0, 0, CColor3::Black(), 0.0f )
	, mModel( model )
	, mWidget( model )
	, mpActiveNode( nullptr )
	, miScrollY( 0 )
	, mpMouseOverNode(0)
{
	mWidget.SetViewport( this );

	gAllViewports.insert( this );

	object::Connect( & model.GetSigRepaint(), & mWidget.GetSlotRepaint() );
	object::Connect( & model.GetSigModelInvalidated(), & mWidget.GetSlotModelInvalidated() );

    _sceneinst_subscriber = msgrouter::channel("SceneInst")->subscribe([=](msgrouter::content_t c){
        mpActiveNode=nullptr;
    });

}
GedVP::~GedVP()
{
	orkset<GedVP*>::iterator it = gAllViewports.find( this );

	if( it != gAllViewports.end() )
	{
		gAllViewports.erase( it );
	}

    if( mpPickBuffer )
        delete mpPickBuffer;
}

///////////////////////////////////////////////////////////////////////////////
void GedVP::DoInit( lev2::GfxTarget* pt )
{
	auto par = pt->FBI()->GetThisBuffer();
	mpPickBuffer = new lev2::CPickBuffer<GedVP>( par,
												 this,
												 0, 0, miW, miH,
												 lev2::PickBufferBase::EPICK_FACE_VTX );

	mpPickBuffer->CreateContext();
	mpPickBuffer->GetContext()->FBI()->SetClearColor( CColor4(0.0f,0.0f,0.0f,0.0f) );
	mpPickBuffer->RefClearColor().SetRGBAU32( 0 );
}
///////////////////////////////////////////////////////////////////////////////
void GedVP::DoSurfaceResize()
{
	mWidget.SetDims( miW, miH );

	if( 0 == mpPickBuffer && (nullptr!=mpTarget) )
	{

	}
	//TODO: mpPickBuffer->Resize()
}
///////////////////////////////////////////////////////////////////////////////
void GedVP::DoRePaintSurface(ui::DrawEvent& drwev)
{
	//printf( "GedVP<%p>::Draw x<%d> y<%d> w<%d> h<%d>\n", this, miX, miY, miW, miH );

	//ork::tool::ged::ObjModel::FlushAllQueues();

	//orkprintf( "GedVP::DoDraw()\n" );

	auto tgt = drwev.GetTarget();
	auto mtxi = tgt->MTXI();
	auto fbi = tgt->FBI();
	//bool bispick = GetFrameData().IsPickMode();

	//////////////////////////////////////////////////
	// Compute Scoll Transform
	//////////////////////////////////////////////////

	ork::CMatrix4 matSCROLL;
	matSCROLL.SetTranslation( 0.0f, float(miScrollY), 0.0f );

	//////////////////////////////////////////////////

	fbi->PushScissor( SRect( 0,0,miW,miH) );
	fbi->PushViewport( SRect( 0,0,miW,miH) );
	mtxi->PushMMatrix( matSCROLL );
	{
		fbi->Clear( GetClearColorRef(), 1.0f );

		auto pobj = mModel.CurrentObject();
		if( pobj )
		{
			GedWidget* pw = mModel.GetGedWidget();

			pw->Draw( tgt, miW, miH, miScrollY );

		}
	}
	mtxi->PopMMatrix();
	fbi->PopViewport();
	fbi->PopScissor();
}

///////////////////////////////////////////////////////////////////////////////

ui::HandlerResult GedVP::DoOnUiEvent( const ui::Event& EV )
{
	ui::HandlerResult ret(this);

	const auto& filtev = EV.mFilteredEvent;

	int ix = EV.miX;
	int iy = EV.miY;
	int ilocx, ilocy;
	RootToLocal(ix,iy,ilocx,ilocy);

	lev2::GetPixelContext ctx;
	ctx.miMrtMask = (1<<0) | (1<<1); // ObjectID and ObjectUVD
	ctx.mUsage[0] = lev2::GetPixelContext::EPU_PTR32;
	ctx.mUsage[1] = lev2::GetPixelContext::EPU_FLOAT;

	bool filt_kpush = (filtev.mAction=="keypush");

	bool filt_leftbutton = filtev.mBut0;
	bool filt_middlebutton = filtev.mBut1;
	bool filt_rightbutton = filtev.mBut2;

	auto qip = (QInputEvent*) EV.mpBlindEventData;

	bool bisshift = EV.mbSHIFT;

	auto locEV = EV;

	locEV.miX = ilocx;
	locEV.miY = ilocy-miScrollY;
    locEV.miRawX = locEV.miX;
    locEV.miRawY = locEV.miY;

	if( mpActiveNode )
		mpActiveNode->OnUiEvent( locEV );

	switch( filtev.miEventCode )
	{
		case ui::UIEV_KEY:
		{
			int mikeyc = filtev.miKeyCode;
			if( mikeyc == '!' )
			{
				mWidget.IncrementSkin();
				mNeedsSurfaceRepaint=true;
			}
			break;
		}
		case ui::UIEV_MOUSEWHEEL:
		{
			int iscrollamt = bisshift ? 32 : 8;

			//if( pobj )
			{
				int idelta = EV.miMWY;

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
				printf( "predelta<%d> miScrollY<%d>\n", idelta, miScrollY );

			}

			mNeedsSurfaceRepaint=true;
			break;
		}
		case ui::UIEV_MOVE:
		{	QMouseEvent* qem = (QMouseEvent*) qip;
			static int gctr = 0;
			if( 0 == gctr%4 )
			{	GetPixel( ilocx, ilocy, ctx );
				rtti::ICastable *pobj = ctx.GetObject(mpPickBuffer,0);
				if( 0 ) //TODO pobj )
				{	GedObject *pnode = rtti::autocast(pobj);
					if( pnode )
					{	//pnode->mouseMoveEvent( & myme );
						mpMouseOverNode = pnode;

						if( pnode != mpActiveNode )
						  pnode->OnUiEvent( locEV );
					}
				}
			}
			gctr++;
			break;
		}
		case ui::UIEV_DRAG:
		{	if( mpActiveNode )
			{
                if( GedItemNode* as_inode = ork::rtti::autocast(mpActiveNode) )
                {
                    locEV.miX -= as_inode->GetX();
                    locEV.miY -= as_inode->GetY();
                }
                mpActiveNode->OnUiEvent( locEV );
				mNeedsSurfaceRepaint=true;
			}
			else
			{
				//pnode->mouseMoveEvent( & myme );
			}
			break;
		}
		case ui::UIEV_PUSH:
		case ui::UIEV_RELEASE:
		case ui::UIEV_DOUBLECLICK:
		{

			QMouseEvent* qem = (QMouseEvent*) qip;


			GetPixel( ilocx, ilocy, ctx );
			float fx = float(ilocx)/float(GetW());
			float fy = float(ilocy)/float(GetH());
			ork::rtti::ICastable* pobj = ctx.GetObject(mpPickBuffer,0);

			bool is_in_set = IsObjInSet(pobj);

			orkprintf( "Object<%p> is_in_set<%d> ilocx<%d> ilocy<%d> fx<%f> fy<%f>\n", pobj, int(is_in_set), ilocx, ilocy, fx, fy );

			/////////////////////////////////////
			// test object against known set
			if( false == is_in_set ) pobj = 0;
			/////////////////////////////////////

			if(GedObject *pnode = ork::rtti::autocast(pobj))
			{
                if( GedItemNode* as_inode = ork::rtti::autocast(pobj) )
                {
                    locEV.miX -= as_inode->GetX();
                    locEV.miY -= as_inode->GetY();
                }

				switch( filtev.miEventCode )
				{
					case ui::UIEV_PUSH:
						mpActiveNode = pnode;
						if( pnode )
						  pnode->OnUiEvent( locEV );
						break;
					case ui::UIEV_RELEASE:
						if( pnode )
						  pnode->OnUiEvent( locEV );
						mpActiveNode = 0;
						break;
					case ui::UIEV_DOUBLECLICK:
						if( pnode )
						  pnode->OnUiEvent( locEV );
						break;
				}
			}

			mNeedsSurfaceRepaint=true;
			break;
		}
		default:
			break;
	}
	return ret;
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
