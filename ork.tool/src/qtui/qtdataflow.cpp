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
#include <orktool/qtui/gfxbuffer.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/dataflow/dataflow.h>
#include <ork/lev2/lev2_asset.h>
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/reflect/RegisterProperty.h>
#include <pkg/ent/PerfController.h>
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
	GfxTarget *pTEXTARG = GetContext();
	GfxTarget* pPARENTTARG = GetParent()->GetContext();
	///////////////////////////////////////////////////////////////////////////
	int itx0 = 0;
	int itx1 = mpViewport->GetW();
	int ity0 = 0;
	int ity1 = mpViewport->GetH();
	///////////////////////////////////////////////////////////////////////////
	mpViewport->BeginFrame(pTEXTARG);
	pTEXTARG->FBI()->SetRtGroup( mpPickRtGroup );	// Enable Mrt
	pTEXTARG->FBI()->EnterPickState(this);
	///////////////////////////////////////////////////////////////////////////
	SRect VPRect( itx0, ity0, itx1, ity1 );
	pTEXTARG->FBI()->PushViewport( VPRect );
	{
		pTEXTARG->MTXI()->PushPMatrix( CMatrix4::Identity );
		pTEXTARG->MTXI()->PushVMatrix( CMatrix4::Identity );
		pTEXTARG->MTXI()->PushMMatrix( CMatrix4::Identity );
		{
			mpViewport->ExtDraw( pTEXTARG );
		}
		pTEXTARG->MTXI()->PopPMatrix();
		pTEXTARG->MTXI()->PopVMatrix();
		pTEXTARG->MTXI()->PopMMatrix();
	}
	pTEXTARG->FBI()->PopViewport();
	///////////////////////////////////////////////////////////////////////////
	pTEXTARG->FBI()->LeavePickState();
	pTEXTARG->FBI()->SetRtGroup(0);
	mpViewport->EndFrame(pTEXTARG);
	///////////////////////////////////////////////////////////////////////////
	SetDirty( false );
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
		ork::dataflow::graph* pgrf = rtti::autocast(ptarget);
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

ork::dataflow::graph* GraphVP::GetTopGraph()
{
	return mDflowEditor.GetTopGraph();
}

GraphVP::GraphVP( DataFlowEditor& dfed, tool::ged::ObjModel& objmdl, const std::string & name )
	: CUIViewport( name, 0, 0, 0, 0, CColor3(0.1f,0.1f,0.1f), 0.0f )
	, mpPickBuffer( 0 )
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
	if( 0 == GetTopGraph() ) return;
	const orklut<ork::PoolString,ork::Object*>& modules = GetTopGraph()->Modules();
	ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16>& vbuf = lev2::GfxEnv::GetSharedDynamicVB();
	ork::lev2::SVtxV12C4T16 v0,v1,v2,v3;
	CVector2 uv0(0.0f,0.0f);
	CVector2 uv1(1.0f,0.0f);
	CVector2 uv2(1.0f,1.0f);
	CVector2 uv3(0.0f,1.0f);
	float fw(kvppickdimw);
	float fh(kvppickdimw);
	float fwd2=fw*0.5f;
	float fhd2=fh*0.5f;
	float faspect = float(pTARG->GetW())/float(pTARG->GetH());
	pTARG->BindMaterial( & mGridMaterial );
	{	/////////////////////////////////
		// wires
		/////////////////////////////////
		int ivcount = 0;
		// count the number of verts we will use
		for( orklut<ork::PoolString,ork::Object*>::const_iterator it=modules.begin(); it!=modules.end(); it++ )
		{	ork::dataflow::dgmodule* pmod = rtti::autocast(it->second);
			if( pmod )
			{	if( false == pTARG->FBI()->IsPickState() )
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
			vw.Lock(pTARG,&vbuf,ivcount);
			for( orklut<ork::PoolString,ork::Object*>::const_iterator it=modules.begin(); it!=modules.end(); it++ )
			{	ork::dataflow::dgmodule* pmod = rtti::autocast(it->second);
				if( pmod )
				{	const CVector2& pos = pmod->GetGVPos();
					U32 ucolor = pTARG->FBI()->IsPickState() ?  (U32)((u64) pmod) : 0xffffffff;
					ucolor = CVector4(ucolor).GetARGBU32();
					if( false == pTARG->FBI()->IsPickState() )
					{	int inuminps = pmod->GetNumInputs();
						for( int ip=0; ip<inuminps; ip++ )
						{	dataflow::inplugbase* pinp = pmod->GetInput(ip);
							const dataflow::outplugbase* poutplug = pinp->GetExternalOutput();
							if( poutplug )
							{	ucolor = pTARG->FBI()->IsPickState() ?  (U32)((u64) pinp) : 0xffffffff;
								ucolor = CVector4(ucolor).GetARGBU32();
								U32 ucolor2 = pTARG->FBI()->IsPickState() ?  (U32)((u64) pinp) : 0x2f2f2f2f;
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
			if( false==pTARG->FBI()->IsPickState() )
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


void GraphVP::DoDraw(  )
{
	ork::lev2::GfxTarget* pTARG = GetTarget();

	if( 0 == mpPickBuffer )
	{
		mpPickBuffer = new lev2::CPickBuffer<GraphVP>(	pTARG->FBI()->GetThisBuffer(), 
															this,
															0, 0, kvppickdimw, kvppickdimw,
															lev2::PickBufferBase::EPICK_FACE_VTX
														);
		mpPickBuffer->RefClearColor().SetRGBAU32( 0 );
		mpPickBuffer->CreateContext();
		mpPickBuffer->GetContext()->FBI()->SetClearColor( CColor4(0.0f,0.0f,0.0f,0.0f) );
	}

	if( 0 == GetTopGraph() ) return;

	SRect tgt_rect = SRect( 0,0, pTARG->GetW(), pTARG->GetH() );
	ork::lev2::RenderContextFrameData framedata;
	framedata.SetDstRect(tgt_rect);
	framedata.SetTarget( pTARG );
	
	pTARG->SetRenderContextFrameData( & framedata );


	bool bpickstate = pTARG->FBI()->IsPickState();

	static const char* assetname = "lev2://textures/dfnodebg2";
	static lev2::TextureAsset* ptexasset = asset::AssetManager<lev2::TextureAsset>::Load(assetname);

	const orklut<ork::PoolString,ork::Object*>& modules = GetTopGraph()->Modules();
	ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16>& vbuf = lev2::GfxEnv::GetSharedDynamicVB();
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

	vector<regstr> regstrs;

	////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////
	pTARG->BeginFrame();
	{
		pTARG->MTXI()->PushPMatrix( mGrid.GetOrthoMatrix() );
		pTARG->MTXI()->PushVMatrix( CMatrix4::Identity );
		pTARG->MTXI()->PushMMatrix( CMatrix4::Identity );
		{
			uint32_t pickID = mpPickBuffer->AssignPickId( GetTopGraph() );
			uint32_t uobj = PickIdToVertexColor(pickID);
			U32 ucolor = bpickstate ? uobj : 0xffffffff;

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

			lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
			vw.Lock( pTARG, &vbuf, 6 );
			{
				vw.AddVertex( v0 );
				vw.AddVertex( v1 );
				vw.AddVertex( v2 );
				
				vw.AddVertex( v0 );
				vw.AddVertex( v2 );
				vw.AddVertex( v3 );
			}
			vw.UnLock(pTARG);

			mGridMaterial.SetTexture( ptexasset->GetTexture() );

			mGridMaterial.SetColorMode( bpickstate ? lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR : lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR );
			pTARG->BindMaterial( & mGridMaterial );

			pTARG->GBI()->DrawPrimitive( vw, ork::lev2::EPRIM_TRIANGLES, 6 );

			///////////////////////////////////////////////////////
			
			if( false == bpickstate ) mGrid.Render( pTARG );


			draw_connections( pTARG );

			///////////////////////////////////////////////////////
		
			pTARG->BindMaterial( & mGridMaterial );

			int inummod = (int) modules.size();

			for( orklut<ork::PoolString,ork::Object*>::const_iterator it=modules.begin(); it!=modules.end(); it++ )
			{
				ork::dataflow::dgmodule* pmod = rtti::autocast(it->second);

				if( pmod )
				{
					const CVector2& pos = pmod->GetGVPos();

					uint32_t pickID = mpPickBuffer->AssignPickId( pmod );

					uint32_t uobj = PickIdToVertexColor(pickID);

					ucolor = bpickstate ? uobj : 0xffffffff;

					if( bpickstate )
					{
						printf( "dpick yo uobj<%p>\n", (void*) uobj );
					}


					v0 = ork::lev2::SVtxV12C4T16( pos+of0, uv0, ucolor );
					v1 = ork::lev2::SVtxV12C4T16( pos+of1, uv1, ucolor );
					v2 = ork::lev2::SVtxV12C4T16( pos+of2, uv2, ucolor );
					v3 = ork::lev2::SVtxV12C4T16( pos+of3, uv3, ucolor );

					//int ivbbase = vbuf.GetNum();

					vw.Lock( pTARG, &vbuf, 6 );
					//pTARG->GBI()->LockVB( vbuf, ivbbase, 6 );
			
					vw.AddVertex( v0 );
					vw.AddVertex( v1 );
					vw.AddVertex( v2 );
					
					vw.AddVertex( v0 );
					vw.AddVertex( v2 );
					vw.AddVertex( v3 );

					vw.UnLock(pTARG);

					lev2::Texture* picon = 0;

					if( false == bpickstate )
					{
						any16 iconcbanno = pmod->GetClass()->Description().GetClassAnnotation( "dflowicon" );

						if( iconcbanno.IsA<lev2::Texture*(*)( ork::dataflow::dgmodule* )>() )
						{
							lev2::Texture* (*IconCB)( ork::dataflow::dgmodule* ) = 
								iconcbanno.Get<lev2::Texture*(*)( ork::dataflow::dgmodule* )>();
						
							picon = IconCB( pmod );
						}
					}

					mGridMaterial.SetColorMode( pTARG->FBI()->IsPickState() 
												? lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR 
												: (picon!=0) 
													? lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR
													: lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR );

					mGridMaterial.SetTexture( picon );

					if( 1 ) // draw register allocations
					{
						int ireg = pmod->GetOutput(0)->GetRegister() ? pmod->GetOutput(0)->GetRegister()->mIndex : -1;

						regstr rs;
						rs.pos = pos;
						rs.ser = pmod->Key().mSerial;
						rs.ireg = ireg;
						regstrs.push_back( rs );
					}
					pTARG->GBI()->DrawPrimitive( vw, ork::lev2::EPRIM_TRIANGLES, 6 );
				}
			}
		}
		pTARG->MTXI()->PopPMatrix();
		pTARG->MTXI()->PopVMatrix();
		pTARG->MTXI()->PopMMatrix();

		////////////////////////////////////////////////////////////////

		pTARG->MTXI()->PushUIMatrix();
		if( false == pTARG->FBI()->IsPickState() )
		{
			lev2::CFontMan::BeginTextBlock(pTARG);
			pTARG->PushModColor( CColor4::Yellow() );
			{
				lev2::CFontMan::DrawText( pTARG, 8, 8, "GroupDepth<%d>", mDflowEditor.StackDepth() );
				if( mDflowEditor.GetSelModule() )
				{
					ork::dataflow::dgmodule* pdgmod = mDflowEditor.GetSelModule();
					lev2::CFontMan::DrawText( pTARG, 8, 16, "Sel<%s>", pdgmod->GetName().c_str() );
				}

				float fxa = mGrid.GetTopLeft().GetX();
				float fxb = mGrid.GetBotRight().GetX();
				float fya = mGrid.GetTopLeft().GetY();
				float fyb = mGrid.GetBotRight().GetY();
				float fgw = fxb-fxa;
				float fgh = fyb-fya;
				float ftw = pTARG->GetW();
				float fth = pTARG->GetH();
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

					if( false == pTARG->FBI()->IsPickState() )
					{
						lev2::CFontMan::DrawText( pTARG, imx+ioff, imy+ioff, "%d:%d"
							, rs.ser
							, rs.ireg
							);
					}
				}
			}
			lev2::CFontMan::EndTextBlock(pTARG);
			pTARG->PopModColor();
		}
		pTARG->MTXI()->PopUIMatrix();

		////////////////////////////////////////////////////////////////
	}
	 pTARG->EndFrame();
	////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////


	pTARG->SetRenderContextFrameData( 0 );

}

///////////////////////////////////////////////////////////////////////////////

void GraphVP::GetPixel( int ix, int iy, lev2::GetPixelContext& ctx )
{
	float fx = float(ix) / float(GetW());
	float fy = float(iy) / float(GetH());
	/////////////////////////////////////////////////////////////
	if( mpPickBuffer )
	{	ctx.mRtGroup = mpPickBuffer->mpPickRtGroup;
		ctx.mAsBuffer = mpPickBuffer;
		/////////////////////////////////////////////////////////////
		mpPickBuffer->Draw();
		/////////////////////////////////////////////////////////////
		mpPickBuffer->GetContext()->FBI()->GetPixel( CVector4( fx, fy, 0.0f ), ctx );
	}
	/////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void GraphVP::ReCenter()
{
	ork::dataflow::graph* pgrf = mDflowEditor.GetTopGraph();
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

lev2::EUIHandled GraphVP::UIEventHandler( lev2::CUIEvent *pEV )
{
	int ix = pEV->miX;
	int iy = pEV->miY;
	float fx = float(ix) / float(GetW());
	float fy = float(iy) / float(GetH());

	float faspect = float(GetW())/float(GetH());

	lev2::GetPixelContext ctx;
	ctx.miMrtMask = (1<<0) | (1<<1); // ObjectID and ObjectUVD
	ctx.mUsage[0] = lev2::GetPixelContext::EPU_PTR32;
	ctx.mUsage[1] = lev2::GetPixelContext::EPU_FLOAT;

	QInputEvent* qip = (QInputEvent*) pEV->mpBlindEventData;

	bool bisshift = pEV->mbSHIFT;
	bool bisalt = pEV->mbALT;
	bool bisctrl = pEV->mbCTRL;

	static ork::dataflow::dgmodule* gpmodule = 0;

	static CVector2 gbasexym;
	static CVector2 gbasexy;

	switch( pEV->miEventCode )
	{	case lev2::UIEV_KEY:
		{	if( pEV->miKeyCode == 'a' )
			{	
				ReCenter();
			}
		}
		case lev2::UIEV_DRAG:
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
			break;
		}
		case lev2::UIEV_RELEASE:
		{
			gpmodule = 0;
			break;
		}
		case lev2::UIEV_PUSH:
		{
			if( false == bisctrl )
			{
				GetPixel( ix, iy, ctx );
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
			break;
		}
		case lev2::UIEV_DOUBLECLICK:
		{
			GetPixel( ix, iy, ctx );
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
			break;
		}
		case lev2::UIEV_MOUSEWHEEL:
		{
			QWheelEvent* qem = (QWheelEvent*) qip;
			int iscrollamt = bisshift ? 256 : 32;
			int idelta = qem->delta();
			float fdelta = (idelta>0) ? 0.0333f : (idelta<0) ? -0.0333f : 0.0f;
			float fz = mGrid.GetZoom() + fdelta;
			if( fz < 0.1f ) fz = 0.1f;
			mGrid.SetZoom(fz);
		}
	}
	return lev2::EUI_HANDLED;
}

///////////////////////////////////////////////////////////////////////////////

DataFlowEditor::DataFlowEditor()
	: mGraphVP(0)
	, mpSelModule(0)
	, mpProbeModule(0)
{
}
void DataFlowEditor::Attach( ork::dataflow::graph* pgrf )
{
	while( mGraphStack.empty() == false ) mGraphStack.pop();
	mGraphStack.push(pgrf);
}
void DataFlowEditor::Push( ork::dataflow::graph* pgrf )
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
ork::dataflow::graph* DataFlowEditor::GetTopGraph()
{
	return mGraphStack.empty() ? 0 : mGraphStack.top();
}

}}


INSTANTIATE_TRANSPARENT_RTTI(ork::tool::dflowgraphedit,"dflowgraphedit");


