////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <orktool/qtui/qtdataflow.h>
#include <orktool/qtui/qtmainwin.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/dataflow/dataflow.h>
#include <ork/lev2/lev2_asset.h>
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/reflect/RegisterProperty.h>
#include <pkg/ent/PerfController.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI( ork::tool::DataFlowEditor, "DataFlowEditor" );

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace  ork {
uint32_t PickIdToVertexColor( uint32_t pid );
namespace lev2 {
template<> 
void CPickBuffer<ork::tool::GraphVP>::Draw( void )
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace tool {

void DataFlowEditor::Describe()
{
	reflect::RegisterFunctor("SlotClear", &DataFlowEditor::SlotClear);
}
void DataFlowEditor::SlotClear()
{
	while( mGraphStack.empty() == false ) mGraphStack.pop();
}

DataFlowEditor* gdfloweditor = 0;

class dflowgraphedit : public tool::ged::IOpsDelegate
{
	RttiDeclareConcrete( dflowgraphedit, tool::ged::IOpsDelegate );
	virtual void Execute( ork::Object* ptarget )
	{
		ork::dataflow::graph_inst* pgrf = rtti::autocast(ptarget);
		if( gdfloweditor && pgrf)
		{
			gdfloweditor->Attach( pgrf );
		}
	}
};

void dflowgraphedit::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

ork::dataflow::graph_data* GraphVP::GetTopGraph()
{
	return mDflowEditor.GetTopGraph();
}

GraphVP::GraphVP( DataFlowEditor& dfed, tool::ged::ObjModel& objmdl, const std::string & name )
	: ui::Surface( name, 0, 0, 0, 0, CColor3(0.1f,0.1f,0.1f), 0.0f )
	, mObjectModel(objmdl)
	, mDflowEditor(dfed)
	, mGridMaterial( ork::lev2::GfxEnv::GetRef().GetLoaderTarget() )

{
	dflowgraphedit::GetClassStatic();

	gdfloweditor = & dfed;

	mGrid.SetCenter( CVector2(0.0f,0.0f) );
	mGrid.SetExtent( 100.0f );
	mGrid.SetZoom(1.0f);

	mpArrowTex = ork::asset::AssetManager<ork::lev2::TextureAsset>::Load("lev2://textures/dfarrow")->GetTexture();

}

///////////////////////////////////////////////////////////////////////////////

void GraphVP::draw_connections( ork::lev2::GfxTarget* pTARG )
{
	auto fbi = pTARG->FBI();
	bool is_pick = fbi->IsPickState();

	if( nullptr == GetTopGraph() ) return;
	const auto& modules = GetTopGraph()->Modules();
	auto& VB = lev2::GfxEnv::GetSharedDynamicVB();
	ork::lev2::SVtxV12C4T16 v0,v1,v2,v3;
	CVector2 uv0(0.0f,0.0f);
	CVector2 uv1(1.0f,0.0f);
	CVector2 uv2(1.0f,1.0f);
	CVector2 uv3(0.0f,1.0f);
	float fw(kvppickdimw);
	float fh(kvppickdimw);
	float fwd2=fw*0.5f;
	float fhd2=fh*0.5f;
	float faspect = float(miW)/float(miH);
	pTARG->BindMaterial( & mGridMaterial );
	{	/////////////////////////////////
		// wires
		/////////////////////////////////
		int ivcount = 0;
		// count the number of verts we will use
		for( const auto& item : modules )
		{	ork::dataflow::dgmodule* pmod = rtti::autocast(item.second);
			if( pmod )
			{	if( false == is_pick )
				{	int inuminps = pmod->GetNumInputs();
					for( int ip=0; ip<inuminps; ip++ )
					{	dataflow::inplugbase* pinp = pmod->GetInput(ip);
						const dataflow::outplugbase* poutplug = pinp->GetExternalOutput();
						if( poutplug ) ivcount += 6;
					}
				}
			}
		}
		if( ivcount )
		{	//int ivbbase = vbuf.GetNumVertices();
			lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
			vw.Lock(pTARG,&VB,ivcount);
			for( orklut<ork::PoolString,ork::Object*>::const_iterator it=modules.begin(); it!=modules.end(); it++ )
			{	ork::dataflow::dgmodule* pmod = rtti::autocast(it->second);
				if( pmod )
				{	const CVector2& pos = pmod->GetGVPos();
					U32 ucolor = is_pick ?  (U32)((u64) pmod) : 0xffffffff;
					ucolor = CVector4(ucolor).GetARGBU32();
					if( false == is_pick )
					{	int inuminps = pmod->GetNumInputs();
						for( int ip=0; ip<inuminps; ip++ )
						{	dataflow::inplugbase* pinp = pmod->GetInput(ip);
							const dataflow::outplugbase* poutplug = pinp->GetExternalOutput();
							if( poutplug )
							{	ucolor = is_pick ?  (U32)((u64) pinp) : 0xffffffff;
								ucolor = CVector4(ucolor).GetARGBU32();
								U32 ucolor2 = is_pick ?  (U32)((u64) pinp) : 0x2f2f2f2f;
								ucolor2 = CVector4(ucolor2).GetARGBU32();
								ork::dataflow::dgmodule* pothmod = rtti::autocast( poutplug->GetModule() );		
								const CVector2& othpos = pothmod->GetGVPos();
								CVector3 vdif = (othpos-pos);
								CVector3 vdir = vdif.Normal();
								CVector3 vcross = vdir.Cross(CVector3(0.0f,0.0f,1.0f))*CVector3(1.0f,faspect,0.0f)*5.0f;
								float flength = vdif.Mag();
								CVector2 uvs( flength/16.0f, 1.0f );
								v0 = ork::lev2::SVtxV12C4T16( pos+vcross.GetXY(), uv1*uvs, ucolor2 );
								v1 = ork::lev2::SVtxV12C4T16( othpos+vcross.GetXY(), uv0*uvs, ucolor );
								v2 = ork::lev2::SVtxV12C4T16( othpos-vcross.GetXY(), uv3*uvs, ucolor );
								v3 = ork::lev2::SVtxV12C4T16( pos-vcross.GetXY(), uv2*uvs, ucolor2 );
								vw.AddVertex( v0 );
								vw.AddVertex( v1 );
								vw.AddVertex( v2 );
								vw.AddVertex( v0 );
								vw.AddVertex( v2 );
								vw.AddVertex( v3 );
							}
						}
					}
				}
			}
			vw.UnLock(pTARG);
			if( false==is_pick )
			{	mGridMaterial.SetTexture(mpArrowTex);
				mGridMaterial.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR );
			}
			pTARG->GBI()->DrawPrimitive( vw, ork::lev2::EPRIM_TRIANGLES, ivcount );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

struct regstr
	{
		CVector3 pos;
		int ser;
		int ireg;
	};

void GraphVP::DoInit(lev2::GfxTarget* pt )
{
	auto fbi = pt->FBI();
	auto par = fbi->GetThisBuffer();
	mpPickBuffer = new lev2::CPickBuffer<GraphVP>(	
		par, 
		this,
		0, 0, miW, miH,
		lev2::PickBufferBase::EPICK_FACE_VTX
	);
	
	mpPickBuffer->CreateContext();
	mpPickBuffer->RefClearColor().SetRGBAU32( 0 );
	mpPickBuffer->GetContext()->FBI()->SetClearColor( CColor4(0.0f,0.0f,0.0f,0.0f) );

}
void GraphVP::DoRePaintSurface(ui::DrawEvent& drwev)
{
	auto tgt = drwev.GetTarget();
	auto mtxi = tgt->MTXI();
	auto fbi = tgt->FBI();
	auto fxi = tgt->FXI();
	auto rsi = tgt->RSI();
	auto gbi = tgt->GBI();
	auto& primi = lev2::CGfxPrimitives::GetRef();
	auto defmtl = lev2::GfxEnv::GetDefaultUIMaterial();
	lev2::DynamicVertexBuffer<lev2::SVtxV12C4T16>& VB = lev2::GfxEnv::GetSharedDynamicVB();
	bool has_foc = HasMouseFocus();
	bool is_pick = fbi->IsPickState(); 
	const orklut<ork::PoolString,ork::Object*>& modules = GetTopGraph()->Modules();

	if( nullptr == GetTopGraph() )
	{
		fbi->PushScissor( SRect( 0,0,miW,miH) );
		fbi->PushViewport( SRect( 0,0,miW,miH) );
			fbi->Clear( CVector4::Black(), 1.0f );
		fbi->PopViewport();
		fbi->PopScissor();
		return;
	}

	//SRect tgt_rect = SRect( 0,0, pTARG->GetW(), pTARG->GetH() );
	//ork::lev2::RenderContextFrameData framedata;
	//framedata.SetDstRect(tgt_rect);
	//framedata.SetTarget( pTARG );
	
	//pTARG->SetRenderContextFrameData( & framedata );

	ork::lev2::SVtxV12C4T16 v0,v1,v2,v3;

	CVector2 uv0(0.0f,0.0f);
	CVector2 uv1(1.0f,0.0f);
	CVector2 uv2(1.0f,1.0f);
	CVector2 uv3(0.0f,1.0f);

	//////////////////////////////////////////

	float fmodsizew = 25.0f;
	float fmodsizeh = 25.0f;

	CVector2 of0(-fmodsizew,-fmodsizeh);
	CVector2 of1(+fmodsizew,-fmodsizeh);
	CVector2 of2(+fmodsizew,+fmodsizeh);
	CVector2 of3(-fmodsizew,+fmodsizeh);

	float fw(kvppickdimw);
	float fh(kvppickdimw);
	float fwd2=fw*0.5f;
	float fhd2=fh*0.5f;
	
	mGridMaterial.mRasterState.SetDepthTest( lev2::EDEPTHTEST_OFF );
	mGridMaterial.mRasterState.SetAlphaTest( ork::lev2::EALPHATEST_GREATER, 0.0f );
	mGridMaterial.mRasterState.SetAlphaTest( ork::lev2::EALPHATEST_OFF, 0.0f );

	////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////
	fbi->PushScissor( SRect( 0,0,miW,miH) );
	fbi->PushViewport( SRect( 0,0,miW,miH) );
	{

		vector<regstr> regstrs;

		fbi->Clear( CVector4::Blue(), 1.0f );
		mtxi->PushPMatrix( mGrid.GetOrthoMatrix() );
		mtxi->PushVMatrix( CMatrix4::Identity );
		mtxi->PushMMatrix( CMatrix4::Identity );
		{
			uint32_t pickID = mpPickBuffer->AssignPickId( GetTopGraph() );
			uint32_t uobj = PickIdToVertexColor(pickID);
			U32 ucolor = is_pick ? uobj : 0xffffffff;

			///////////////////////////////////////////////////////
			// draw background
			///////////////////////////////////////////////////////

			float fxa = mGrid.GetTopLeft().GetX();
			float fxb = mGrid.GetBotRight().GetX();
			float fya = mGrid.GetTopLeft().GetY();
			float fyb = mGrid.GetBotRight().GetY();

			v0 = ork::lev2::SVtxV12C4T16( CVector3(fxa,fya,0.0f), uv0, ucolor );
			v1 = ork::lev2::SVtxV12C4T16( CVector3(fxb,fya,0.0f), uv1, ucolor );
			v2 = ork::lev2::SVtxV12C4T16( CVector3(fxb,fyb,0.0f), uv2, ucolor );
			v3 = ork::lev2::SVtxV12C4T16( CVector3(fxa,fyb,0.0f), uv3, ucolor );

			//int ivbbase = vbuf.GetNum();
			{

				lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
				vw.Lock( tgt, &VB, 6 );
				{
					vw.AddVertex( v0 );
					vw.AddVertex( v1 );
					vw.AddVertex( v2 );
					
					vw.AddVertex( v0 );
					vw.AddVertex( v2 );
					vw.AddVertex( v3 );
				}
				vw.UnLock(tgt);

				static const char* assetname = "lev2://textures/dfnodebg2";
				static lev2::TextureAsset* ptexasset = asset::AssetManager<lev2::TextureAsset>::Load(assetname);

				mGridMaterial.SetTexture( ptexasset->GetTexture() );

				mGridMaterial.SetColorMode( is_pick ? lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR 
													: lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR );

				tgt->BindMaterial( & mGridMaterial );

				gbi->DrawPrimitive( vw, ork::lev2::EPRIM_TRIANGLES, 6 );
			}

			///////////////////////////////////////////////////////
			// draw grid
			///////////////////////////////////////////////////////

			if( false == is_pick ) mGrid.Render( tgt, miW, miH );

			///////////////////////////////////////////////////////

			draw_connections( tgt );

			///////////////////////////////////////////////////////
			// draw modules
			///////////////////////////////////////////////////////

			tgt->PushMaterial( & mGridMaterial );

			int inummod = (int) modules.size();

			int imod = 0;
			for( const auto& item : modules )
			{
				ork::dataflow::dgmodule* pmod = rtti::autocast(item.second);

				if( pmod )
				{
					const CVector2& pos = pmod->GetGVPos();

					uint32_t pickID = mpPickBuffer->AssignPickId( pmod );

					uint32_t uobj = PickIdToVertexColor(pickID);

					ucolor = is_pick ? uobj : 0xffffffff;

					if( is_pick )
					{
						printf( "dpick yo uobj<%p>\n", (void*) uobj );
					}

					//int ivbbase = vbuf.GetNum();

					{

						v0 = ork::lev2::SVtxV12C4T16( pos+of0, uv0, ucolor );
						v1 = ork::lev2::SVtxV12C4T16( pos+of1, uv1, ucolor );
						v2 = ork::lev2::SVtxV12C4T16( pos+of2, uv2, ucolor );
						v3 = ork::lev2::SVtxV12C4T16( pos+of3, uv3, ucolor );

						lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
						vw.Lock( tgt, &VB, 6 );
				
						vw.AddVertex( v0 );
						vw.AddVertex( v1 );
						vw.AddVertex( v2 );
						
						vw.AddVertex( v0 );
						vw.AddVertex( v2 );
						vw.AddVertex( v3 );

						vw.UnLock(tgt);

						//////////////////////
						// select texture (using dynamic interface if requested)
						//////////////////////

						lev2::Texture* picon = 0;

						if( false == is_pick )
						{
							any16 iconcbanno = pmod->GetClass()->Description().GetClassAnnotation( "dflowicon" );

							typedef lev2::Texture*(*icon_cb_t)( ork::dataflow::dgmodule* );

							if( iconcbanno.IsA<icon_cb_t>() )
							{
								auto IconCB = 
									iconcbanno.Get<icon_cb_t>();
							
								picon = IconCB( pmod );
							}
						}

						mGridMaterial.SetColorMode( is_pick 
													? lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR 
													: (picon!=0) 
														? lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR
														: lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR );

						mGridMaterial.SetTexture( picon );

						//printf( "imod<%d:%p> icon<%p> pos<%f %f>\n", imod, pmod, picon, pos.GetX(), pos.GetY() );

						//////////////////////
						// draw the dataflow node
						//////////////////////

						fxi->InvalidateStateBlock();

						gbi->DrawPrimitive( vw, ork::lev2::EPRIM_TRIANGLES );

						//////////////////////
						// queue register mapping for draw
						//////////////////////

						if( 1 )
						{
							int ireg = pmod->GetOutput(0)->GetRegister() ? pmod->GetOutput(0)->GetRegister()->mIndex : -1;

							regstr rs;
							rs.pos = pos;
							rs.ser = pmod->Key().mSerial;
							rs.ireg = ireg;
							regstrs.push_back( rs );
						}
					}
				}
				imod++;
			}
			tgt->PopMaterial();
		}
		mtxi->PopPMatrix();
		mtxi->PopVMatrix();
		mtxi->PopMMatrix();

		////////////////////////////////////////////////////////////////

		mtxi->PushUIMatrix(miW,miH);
		if( false == is_pick )
		{
			lev2::CFontMan::BeginTextBlock(tgt);
			tgt->PushModColor( CColor4::Yellow() );
			{
				lev2::CFontMan::DrawText( tgt, 8, 8, "GroupDepth<%d>", mDflowEditor.StackDepth() );
				if( mDflowEditor.GetSelModule() )
				{
					ork::dataflow::dgmodule* pdgmod = mDflowEditor.GetSelModule();
					lev2::CFontMan::DrawText( tgt, 8, 16, "Sel<%s>", pdgmod->GetName().c_str() );
				}

				float fxa = mGrid.GetTopLeft().GetX();
				float fxb = mGrid.GetBotRight().GetX();
				float fya = mGrid.GetTopLeft().GetY();
				float fyb = mGrid.GetBotRight().GetY();
				float fgw = fxb-fxa;
				float fgh = fyb-fya;
				float ftw = miW;
				float fth = miH;
				float fwr = ftw/fgw;
				float fhr = fth/fgh;
				float fzoom = mGrid.GetZoom();

				float fcx = mGrid.GetCenter().GetX();
				float fcy = mGrid.GetCenter().GetY();

				int inumrs = regstrs.size();
				for( int i=0; i<inumrs; i++ )
				{
					const regstr& rs = regstrs[i];

					const CVector2 pos = rs.pos; //(pos-xy0);

					float fxx = (pos.GetX()-fxa)*(ftw/fgw);
					float fyy = (pos.GetY()-fya)*(fth/fgh);

					int imx = int(fxx); 
					int imy = int(fyy); 

					float ioff = fmodsizew*(ftw/fgw);

					if( false == is_pick )
					{
						lev2::CFontMan::DrawText( tgt, imx+ioff, imy+ioff, "%d:%d"
							, rs.ser
							, rs.ireg
							);
					}
				}
			}
			lev2::CFontMan::EndTextBlock(tgt);
			tgt->PopModColor();
		}
		mtxi->PopUIMatrix();

		////////////////////////////////////////////////////////////////
	}
	fbi->PopViewport();
	fbi->PopScissor();
	////////////////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////

void GraphVP::ReCenter()
{
	ork::dataflow::graph_data* pgrf = mDflowEditor.GetTopGraph();
	if( pgrf )
	{	
		CVector2 vmin(+CFloat::TypeMax(),+CFloat::TypeMax());
		CVector2 vmax(-CFloat::TypeMax(),-CFloat::TypeMax());
		const orklut<ork::PoolString,ork::Object*>& modules = pgrf->Modules();
		for( orklut<ork::PoolString,ork::Object*>::const_iterator it=modules.begin(); it!=modules.end(); it++ )
		{	ork::dataflow::dgmodule* pmod = rtti::autocast(it->second);
			if( pmod )
			{	const CVector2& pos = pmod->GetGVPos();
				
				if( pos.GetX() < vmin.GetX() ) vmin.SetX( pos.GetX() );
				if( pos.GetY() < vmin.GetY() ) vmin.SetY( pos.GetY() );
				if( pos.GetX() > vmax.GetX() ) vmax.SetX( pos.GetX() );
				if( pos.GetY() > vmax.GetY() ) vmax.SetY( pos.GetY() );
			}
		}

		CVector2 rng = vmax-vmin;
		CVector2 ctr = (vmin+vmax)*0.5f;
		float fmax = (rng.GetX()>rng.GetY()) ? rng.GetX() : rng.GetY();
		float fz = (mGrid.GetExtent())/(fmax+32.0f);
		mGrid.SetCenter(ctr);
		mGrid.SetZoom(fz);
	}
}

///////////////////////////////////////////////////////////////////////////////

ui::HandlerResult GraphVP::DoOnUiEvent( const ui::Event& EV )
{
	int ix = EV.miX;
	int iy = EV.miY;
	int ilocx, ilocy;
	RootToLocal(ix,iy,ilocx,ilocy);
	float fx = float(ilocx)/float(GetW());
	float fy = float(ilocy)/float(GetH());

	lev2::GetPixelContext ctx;
	ctx.miMrtMask = (1<<0) | (1<<1); // ObjectID and ObjectUVD
	ctx.mUsage[0] = lev2::GetPixelContext::EPU_PTR32;
	ctx.mUsage[1] = lev2::GetPixelContext::EPU_FLOAT;

	QInputEvent* qip = (QInputEvent*) EV.mpBlindEventData;

	bool bisshift = EV.mbSHIFT;
	bool bisalt = EV.mbALT;
	bool bisctrl = EV.mbCTRL;

	static ork::dataflow::dgmodule* gpmodule = 0;

	static CVector2 gbasexym;
	static CVector2 gbasexy;

	switch( EV.miEventCode )
	{	case ui::UIEV_KEY:
		{	if( EV.miKeyCode == 'a' )
			{	
				ReCenter();
				SetDirty();
			}
		}
		case ui::UIEV_MOVE:
		{
			SetDirty();
			break;
		}
		case ui::UIEV_DRAG:
		{
			if( gpmodule )
			{
				float fix = (fx*mGrid.GetBotRight().GetX())+((1.0-fx)*mGrid.GetTopLeft().GetX());
				float fiy = (fy*mGrid.GetBotRight().GetY())+((1.0-fy)*mGrid.GetTopLeft().GetY());
				gpmodule->SetGVPos( mGrid.Snap(CVector2( fix, fiy )) );

			}
			else if( bisalt )
			{
				mGrid.SetCenter( gbasexy-(CVector2(ix,iy)-gbasexym) );
			}
			SetDirty();
			break;
		}
		case ui::UIEV_RELEASE:
		{
			gpmodule = 0;
			break;
		}
		case ui::UIEV_PUSH:
		{
			if( false == bisctrl )
			{
				GetPixel( ilocx, ilocy, ctx );
				ork::Object* pobj = (ork::Object*) ctx.GetObject(mpPickBuffer,0);

				printf( "pobj<%p>\n", pobj );
				if(ork::Object *object = ork::rtti::autocast(pobj))
					mObjectModel.Attach(object);
				gpmodule = rtti::autocast(pobj);
				gbasexym = CVector2( ix,iy );
				gbasexy = mGrid.GetCenter();
				dataflow::dgmodule* dgmod = rtti::autocast(pobj);
				mDflowEditor.SelModule( dgmod );
			}			
			SetDirty();
			break;
		}
		case ui::UIEV_DOUBLECLICK:
		{
			GetPixel( ilocx, ilocy, ctx );
			ork::rtti::ICastable *pobj = ctx.GetObject(mpPickBuffer,0);
			gpmodule = rtti::autocast(pobj);		

			if( bisctrl )
			{
				if( gpmodule && gpmodule->IsGroup() )
				{
					mDflowEditor.Push( gpmodule->GetChildGraph() );
					ReCenter();
				}
				else
				{
					mDflowEditor.Pop();
					ReCenter();
				}
			}
			else
			{
				mDflowEditor.SetProbeModule( gpmodule );
			}
			SetDirty();
			break;
		}
		case ui::UIEV_MOUSEWHEEL:
		{
			QWheelEvent* qem = (QWheelEvent*) qip;
			int iscrollamt = bisshift ? 256 : 32;
			int idelta = qem->delta();
			const float kstep = 1.0f/100.0f;
			float fdelta = (idelta>0) ? kstep : (idelta<0) ? -kstep : 0.0f;
			float fz = mGrid.GetZoom() + fdelta;
			if( fz < 0.1f ) fz = 0.1f;
			mGrid.SetZoom(fz);
			SetDirty();
		}
	}
	return ui::HandlerResult(this);
}

///////////////////////////////////////////////////////////////////////////////

DataFlowEditor::DataFlowEditor()
	: mGraphVP(0)
	, mpSelModule(0)
	, mpProbeModule(0)
{
}
void DataFlowEditor::Attach( ork::dataflow::graph_data* pgrf )
{
	while( mGraphStack.empty() == false ) mGraphStack.pop();
	mGraphStack.push(pgrf);
}
void DataFlowEditor::Push( ork::dataflow::graph_data* pgrf )
{
	mGraphStack.push(pgrf);
}
void DataFlowEditor::Pop()
{
	if( mGraphStack.size()>1 )
	{
		mGraphStack.pop();
	}
}
ork::dataflow::graph_data* DataFlowEditor::GetTopGraph()
{
	return mGraphStack.empty() ? 0 : mGraphStack.top();
}

}}


INSTANTIATE_TRANSPARENT_RTTI(ork::tool::dflowgraphedit,"dflowgraphedit");


