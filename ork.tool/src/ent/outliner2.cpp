#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/qtui/qtui.hpp>
#include <ork/reflect/Functor.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <pkg/ent/editor/editor.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/dbgfontman.h>

#include "outliner2.h"
#include <orktool/ged/ged.h>

INSTANTIATE_TRANSPARENT_RTTI( ork::ent::Outliner2Model, "Outliner2Model" );

namespace ork {
uint32_t PickIdToVertexColor( uint32_t pid );
}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void Outliner2Model::Describe()
{
	RegisterAutoSlot( Outliner2Model, SceneTopoChanged );
	RegisterAutoSlot( Outliner2Model, ObjectSelected );
	RegisterAutoSlot( Outliner2Model, ObjectDeSelected );
	RegisterAutoSlot( Outliner2Model, ClearSelection );

	RegisterAutoSignal( Outliner2Model, ModelChanged );
}

Outliner2Model::Outliner2Model(SceneEditorBase&ed,Outliner2View&v)
	: mEditor(ed)
	, ConstructAutoSlot(SceneTopoChanged)
	, ConstructAutoSlot(ObjectSelected)
	, ConstructAutoSlot(ObjectDeSelected)
	, ConstructAutoSlot(ClearSelection)
	, mVP(v)
	, mLastSelection(-1)
	, mShowArchs(true)
	, mShowEnts(true)
	, mShowComps(true)
{
	SetupSignalsAndSlots();
}
Outliner2Model::~Outliner2Model()
{

}
void Outliner2Model::IncSel()
{
	auto& selmgr = mEditor.SelectionManager();

	ork::Object* pobj = nullptr;

	mLastSelection++;
	if(mLastSelection>=mItems.size())
		mLastSelection = 0;

	if( mLastSelection<mItems.size() )
	{
		pobj = mItems[mLastSelection].mObject;
		selmgr.ClearSelection();
		selmgr.AddObjectToSelection(pobj);
	}
}
void Outliner2Model::DecSel()
{
	auto& selmgr = mEditor.SelectionManager();

	ork::Object* pobj = nullptr;

	mLastSelection--;
	if(mLastSelection<0)
		mLastSelection = mItems.size()-1;

	if( mLastSelection>=0 )
	{
		pobj = mItems[mLastSelection].mObject;
		selmgr.ClearSelection();
		selmgr.AddObjectToSelection(pobj);
	}

}
void Outliner2Model::ToggleEnts()
{
	mShowEnts=!mShowEnts;
	UpdateModel();
}
void Outliner2Model::ToggleArchs()
{
	mShowArchs=!mShowArchs;
	UpdateModel();
}
void Outliner2Model::ToggleComps()
{
	mShowComps=!mShowComps;
	UpdateModel();
}

void Outliner2Model::UpdateModel()
{
	printf( "Outliner2Model<%p>::SlotSceneTopoChanged\n", this );

	auto scene_data = mEditor.GetSceneData();

	if( scene_data )
	{
		orkmap<PoolString, SceneObject*>& objs = scene_data->GetSceneObjects();
		size_t numobjs = objs.size();

		int iy = 0;

		bool alt = false;

		mItems.clear();
		int index = 0;

		mLastSelection = -1;

		for( const auto& item : objs )
		{
			const PoolString& name = item.first;
			SceneObject* pobj = item.second;

			Archetype* as_arch = rtti::autocast(pobj);
			EntData* as_ent = rtti::autocast(pobj);

			if( as_ent && false==mShowEnts ) pobj=nullptr;
			if( as_arch && false==mShowArchs ) pobj=nullptr;

			if( pobj )
			{
				FixedString<256> decnam;
				if( as_arch )
					decnam.format("(a) %s", item.first.c_str() );
				if( as_ent )
					decnam.format("(e) %s", item.first.c_str() );

				Outliner2Item  o2i;

				o2i.mName = decnam.c_str();
				o2i.mObject = pobj;

				bool is_sel = mSelected.find((ork::Object*)pobj)!=mSelected.end();

				if( is_sel )
					mLastSelection = index;

				o2i.mSelected = is_sel;
				mItems.push_back(o2i);
				index++;

				if( as_arch && mShowComps )
				{
					const auto& comps = as_arch->GetComponentDataTable().GetComponents();
					for( const auto& citem : comps )
					{
						auto c = citem.second;
						is_sel = mSelected.find((ork::Object*)c)!=mSelected.end();
						if( is_sel )
							mLastSelection = index;

						auto clazz = c->GetClass();
						auto class_name = clazz->Name().c_str();
						
						decnam.format("(c) %s", class_name );
						Outliner2Item  o2ic;
						o2ic.mName = decnam.c_str();
						o2ic.mObject = c;
						o2ic.mSelected = is_sel;
						o2ic.mIndent = 1;
						mItems.push_back(o2ic);
						index++;
					}

				}
			}
		}
	}

	mVP.SetDirty();

	SigModelChanged();


}
void Outliner2Model::SlotSceneTopoChanged()
{
	UpdateModel();
}
void Outliner2Model::SigModelChanged()
{
	//mSignalModelChanged(&Outliner2Model::SigModelChanged);
}

///////////////////////////////////////////////////////////////////////////////
void Outliner2Model::SlotObjectSelected( ork::Object* pobj )
{
	printf( "Outliner2Model<%p>::SlotObjectSelected obj<%p>\n", this, pobj );
	mSelected.insert(pobj);
	UpdateModel();

}
///////////////////////////////////////////////////////////////////////////////
void Outliner2Model::SlotObjectDeSelected( ork::Object* pobj )
{
	printf( "Outliner2Model<%p>::SlotObjectDeSelected obj<%p>\n", this, pobj );
	auto it = mSelected.find(pobj);
	if( it != mSelected.end() )
	{
		mSelected.erase(it);
	}
	UpdateModel();
}	
///////////////////////////////////////////////////////////////////////////////
void Outliner2Model::SlotClearSelection()
{
	mSelected.clear();
	UpdateModel();
}
///////////////////////////////////////////////////////////////////////////////

Outliner2View::Outliner2View(SceneEditorBase&ed)
	: ui::Surface( "outl2", 0, 0, 0, 0, CColor3::Black(), 0.0f )
	, mOutlinerModel(ed,*this)
	, mFont(nullptr)
	, mCtxBase(nullptr)
	, miScrollY(0)
	, mContentH(0)
	, ConstructAutoSlot(ModelChanged)
	, mDark(true)
{

	object::Connect(	& ed.GetSigSceneTopoChanged(),
						& mOutlinerModel.GetSlotSceneTopoChanged()
						 );

	object::Connect(	& mOutlinerModel.GetSigModelChanged(),
						& this->GetSlotModelChanged()
						 );

	object::Connect(	& ed.SelectionManager().GetSigObjectSelected(),
						& mOutlinerModel.GetSlotObjectSelected()
						 );

	object::Connect(	& ed.SelectionManager().GetSigObjectDeSelected(),
						& mOutlinerModel.GetSlotObjectDeSelected()
						 );
}

void Outliner2View::SlotObjectSelected( ork::Object* pobj )
{

}
void Outliner2View::SlotObjectDeSelected( ork::Object* pobj )
{

}	

void Outliner2View::SlotModelChanged()
{
	assert(false);
}
///////////////////////////////////////////////////////////////////////////////
int Outliner2View::kitemh() const
{
	return mCharH+4;
}
///////////////////////////////////////////////////////////////////////////////
void Outliner2View::DoInit( lev2::GfxTarget* pt )
{
	auto par = pt->FBI()->GetThisBuffer();
	mpPickBuffer = new lev2::CPickBuffer<Outliner2View>(
		par, 
		this,
		0, 0, miW, miH,
		lev2::PickBufferBase::EPICK_FACE_VTX );

	mpPickBuffer->CreateContext();
	mpPickBuffer->GetContext()->FBI()->SetClearColor( CColor4(0.0f,0.0f,0.0f,0.0f) );
	mpPickBuffer->RefClearColor().SetRGBAU32( 0 );

	mFont = lev2::CFontMan::GetFont("i13");
	auto& fontdesc = mFont->GetFontDesc();

	mCharW = fontdesc.miAdvanceWidth;
	mCharH = fontdesc.miAdvanceHeight;

	mCtxBase  = pt->GetCtxBase();

}
///////////////////////////////////////////////////////////////////////////////
void Outliner2View::DoRePaintSurface(ui::DrawEvent& drwev)
{
	auto tgt = drwev.GetTarget();
	auto mtxi = tgt->MTXI();
	auto fbi = tgt->FBI();
	auto fxi = tgt->FXI();
	auto rsi = tgt->RSI();
	auto& primi = lev2::CGfxPrimitives::GetRef();
	auto defmtl = lev2::GfxEnv::GetDefaultUIMaterial();
	lev2::DynamicVertexBuffer<lev2::SVtxV12C4T16>& VB = lev2::GfxEnv::GetSharedDynamicVB();
	SceneEditorBase& ed = mOutlinerModel.Editor();
	auto scene_data = ed.GetSceneData();
	bool has_foc = HasMouseFocus();
	bool is_pick = fbi->IsPickState(); 

	//////////////////////////////////////////////////
	// Compute Scoll Transform
	//////////////////////////////////////////////////

	ork::CMatrix4 matSCROLL;
	matSCROLL.SetTranslation( 0.0f, float(miScrollY), 0.0f );
	lev2::SRasterState defstate;

	//////////////////////////////////////////////////

	fbi->PushScissor( SRect( 0,0,miW,miH) );
	fbi->PushViewport( SRect( 0,0,miW,miH) );

	{
		fbi->Clear( CVector4::Blue(), 1.0f );
	
		rsi->BindRasterState( defstate );
		fxi->InvalidateStateBlock();

		const std::vector<Outliner2Item>& items = mOutlinerModel.Items();

		mContentH = items.size()*kitemh();

		CVector4 c1(0.7f,0.7f,0.8f);
		CVector4 c2(0.8f,0.8f,0.8f);
		CVector4 c3(0.8f,0.0f,0.0f);

		if( mDark )
		{
			c1 = CVector4( 0.3f,0.3f,0.4f );
			c2 = CVector4( 0.2f,0.2f,0.3f );
			c3 = CVector4( 0.5f,0.0f,0.0f );
		}

		const int kheaderH = miScrollY;

		tgt->PushMaterial( defmtl );
		mtxi->PushUIMatrix(miW,miH);
		{

			int iy = kheaderH;
			bool alt = false;

			//////////////////////////////////////

			for( const auto& item : items )
			{	const std::string& name = item.mName;
				auto pobj = item.mObject;
		        uint32_t pickID = mpPickBuffer->AssignPickId(pobj);
				uint32_t uobj = PickIdToVertexColor( pickID );
				CVector4 pick_color(pickID);
				bool is_sel = item.mSelected;

				if( is_pick )
					tgt->PushModColor( pick_color );
				else
					tgt->PushModColor( is_sel ? c3 : (alt ? c1 : c2) );

				primi.RenderQuadAtZ(
					tgt,
					0, miW, 		// x0, x1
					iy, iy+kitemh(), 	// y0, y1
					0.0f,			// z
					0.0f, 1.0f,		// u0, u1
					0.0f, 1.0f		// v0, v1
					);

				tgt->PopModColor();
				iy += kitemh();
				alt = ! alt;
			}

			//////////////////////////////////////

			if( false == is_pick )
			{
				lev2::GfxMaterialUI uimat(tgt);
				uimat.SetUIColorMode( ork::lev2::EUICOLOR_MOD );

				lev2::CFontMan::PushFont(mFont);
				tgt->PushMaterial( & uimat );
				tgt->PushModColor( mDark ? CColor4(0.7f,0.7f,0.8f) : CColor4::Black() );
				lev2::CFontMan::BeginTextBlock( tgt );
				iy = kheaderH+5;
				for( const auto& item : items )
				{	const std::string& name = item.mName;
					auto pobj = item.mObject;
					int indent = item.mIndent;
			        
					lev2::CFontMan::DrawText( tgt, (indent+1)*16, iy, name.c_str() );
			        iy += kitemh();
					alt = ! alt;
				}
				lev2::CFontMan::EndTextBlock( tgt );
				lev2::CFontMan::PopFont();
				tgt->PopMaterial();
				tgt->PopModColor();
			}
		}
		mtxi->PopUIMatrix();
		tgt->PopMaterial();
	}
	fbi->PopViewport();
	fbi->PopScissor();
}
///////////////////////////////////////////////////////////////////////////////
void Outliner2View::SetNameOfSelectedItem()
{
	int ilastsel = mOutlinerModel.GetLastSelection();

	int irx, iry;
	LocalToRoot(0,(ilastsel*kitemh())+miScrollY,irx,iry);

	const std::vector<Outliner2Item>& items = mOutlinerModel.Items();
	const Outliner2Item& item = items[ilastsel];

	//int irootx = mParent->miX;
	//int ipary = mParent->miY;

	auto g = mCtxBase->MapCoordToGlobal(CVector2(irx,iry));

	//QString qstr = tool::ged::GedInputDialog::getText ( &qev, & mParent, ptsg.c_str(), 2, 2, mParent.width()-3, iheight );
	tool::ged::GedInputDialog dialog;
	dialog.setModal( true );

	dialog.setGeometry( g.GetX(), g.GetY(), miW, kitemh() );
	dialog.clear();
	dialog.mTextEdit.setGeometry( 0, 0, miW, kitemh() );
	dialog.mTextEdit.SetText( item.mName.c_str() );

	int iv = dialog.exec();

	QString res("");

	if( 0 == iv && dialog.WasChanged() )
	{
		auto result = dialog.GetResult();
		const char* rescstr = result.toAscii().constData();

		auto& ed = mOutlinerModel.Editor();
		auto& sm = ed.SelectionManager();


		ed.EditorRenameSceneObject((ent::SceneObject*)item.mObject,rescstr);
		sm.ClearSelection();
		sm.AddObjectToSelection((ork::Object*)item.mObject);

	}

}
///////////////////////////////////////////////////////////////////////////////
ui::HandlerResult Outliner2View::DoOnUiEvent( const ui::Event& EV )
{
	ui::HandlerResult ret(this);

	auto& ed = mOutlinerModel.Editor();
	auto& sm = ed.SelectionManager();
	const auto& filtev = EV.mFilteredEvent;

	//ork::tool::ged::ObjModel::FlushAllQueues();
	int ix = EV.miX;
	int iy = EV.miY;
	int ilocx, ilocy;
	RootToLocal(ix,iy,ilocx,ilocy);

	lev2::GetPixelContext ctx;
	ctx.miMrtMask = (1<<0) | (1<<1); // ObjectID and ObjectUVD
	ctx.mUsage[0] = lev2::GetPixelContext::EPU_PTR32;
	ctx.mUsage[1] = lev2::GetPixelContext::EPU_FLOAT;

	QInputEvent* qip = (QInputEvent*) EV.mpBlindEventData;

	bool bisshift = EV.mbSHIFT;

	switch( filtev.miEventCode )
	{
		case ui::UIEV_KEY:
		{
			int ikeyc = filtev.miKeyCode;
			printf( "ikeyc<%d>\n", ikeyc );
			
			switch( ikeyc )
			{
				case 'a': 
				{
					if( false==mOutlinerModel.AreArchsEnabled() )
						mOutlinerModel.ToggleArchs();
					else if( mOutlinerModel.AreArchsEnabled() && false==mOutlinerModel.AreCompsEnabled() )
						mOutlinerModel.ToggleComps();
					else if( mOutlinerModel.AreArchsEnabled() && mOutlinerModel.AreCompsEnabled() )
					{
						mOutlinerModel.ToggleArchs();
						mOutlinerModel.ToggleComps();
					}
					break;
				}
				case '!':
				{
					mDark = !mDark;
					SetDirty();
					break;
				}
				case 'e': 
				{
					mOutlinerModel.ToggleEnts();
					break;
				}
				case '\n': 
				{	
					SetNameOfSelectedItem();
					break; 
				}
				case 16777219: // delete
				{
					int ilastsel = mOutlinerModel.GetLastSelection();
					if( ilastsel>=0 )
					{
						const std::vector<Outliner2Item>& items = mOutlinerModel.Items();
						const Outliner2Item& item = items[ilastsel];
						ed.EditorDeleteObject(item.mObject);
					}
					break;
				}
				case 16777235: // cursup
					mOutlinerModel.DecSel();
					break;
				case 16777237: // cursdn
					mOutlinerModel.IncSel();
					break;
				default:
					break;
			}
			break;
		}
		case ui::UIEV_PUSH:
		case ui::UIEV_RELEASE:
		{
			QMouseEvent* qem = (QMouseEvent*) qip;

			GetPixel( ilocx, ilocy, ctx );
			float fx = float(ilocx)/float(GetW());
			float fy = float(ilocy)/float(GetH());
			ork::rtti::ICastable* pobj = ctx.GetObject(mpPickBuffer,0);

			bool is_in_set = true; //IsObjInSet(pobj);

			orkprintf( "Object<%p> is_in_set<%d> ilocx<%d> ilocy<%d> fx<%f> fy<%f>\n", pobj, int(is_in_set), ilocx, ilocy, fx, fy );
			mNeedsSurfaceRepaint=true;

			if( pobj )
			{
				sm.ClearSelection();
				sm.AddObjectToSelection((ork::Object*)pobj);
			}
			break;
		}
		case ui::UIEV_DOUBLECLICK:
		{
			SetNameOfSelectedItem();
			break;
		}
		case ui::UIEV_MOUSEWHEEL:
		{
			//QWheelEvent* qem = (QWheelEvent*) qip;
			int idelta = EV.miMWY;
			miScrollY += idelta;

			int scrollb = -(mContentH-miH);
			printf( "miScrollY<%d> mContentH<%d> scrollb<%d>\n", miScrollY, mContentH, scrollb );
			if( miScrollY<scrollb )
				miScrollY = scrollb;
			if( miScrollY>0 )
				miScrollY = 0;
			mNeedsSurfaceRepaint=true;

			break;
		}
		default:
			break;
	}
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
}} //namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
template class ork::lev2::CPickBuffer<ork::ent::Outliner2View>;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
template<> 
void CPickBuffer<ork::ent::Outliner2View>::Draw( void )
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
		printf( "resize ged pickbuf rtgroup<%d %d>\n", isurfw, isurfh);
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