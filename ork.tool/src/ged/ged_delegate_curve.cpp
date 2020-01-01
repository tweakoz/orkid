////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/IObjectPropertyObject.h>
#include <ork/reflect/IObjectPropertyType.h>
#include "ged_delegate.hpp"
#include <ork/math/multicurve.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/kernel/orkpool.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////

static const int kpntsize = 5;
static const int kh = 128;

class GedCurveEditPoint : public GedObject
{
	RttiDeclareAbstract( GedCurveEditPoint, GedObject );
	MultiCurve1D*											mCurveObject;
	GedItemNode*											mParent;
	int														miPoint;

public:
	void SetPoint( int idx ) { miPoint=idx; }
	GedCurveEditPoint() : mCurveObject(0), mParent(0), miPoint(-1) {}
	void SetCurveObject( MultiCurve1D* pgrad ) { mCurveObject=pgrad; }
	void SetParent( GedItemNode* ppar ) { mParent=ppar; }
	void OnMouseDoubleClicked(const ork::ui::Event& ev) final
	{	if( mParent && mCurveObject )
		{	orklut<float,float> & data = mCurveObject->GetVertices();
			orklut<float,float>::iterator it = data.begin()+miPoint;
			if( ev.IsButton0DownF() )
			{	bool bok = false;
			}
			else if( ev.IsButton2DownF() )
			{	if( it->first != 0.0f && it->first != 1.0f )
				{	mCurveObject->MergeSegment(miPoint);
					mParent->SigInvalidateProperty();
				}
			}
		}
	}
    void OnUiEvent( const ork::ui::Event& ev ) final
    {
        const auto& filtev = ev.mFilteredEvent;

        switch( filtev.miEventCode )
        {
            case ui::UIEV_DRAG:
            {   
        		if( mParent && mCurveObject )
        		{	orklut<float,float> & data = mCurveObject->GetVertices();
        			const int knumpoints = (int) data.size();
        			const int ksegs = knumpoints-1;
        			if( miPoint>=0 && miPoint<knumpoints )
        			{	
                        int mouseposX = ev.miX-mParent->GetX();
                        int mouseposY = ev.miY-mParent->GetY();
    					float fx = float(mouseposX)/float(mParent->width());
    					float fy = 1.0f-float(mouseposY)/float(kh);
    					if( miPoint==0 ) fx=0.0f;
    					if( miPoint==(knumpoints-1) ) fx=1.0f;
    					if( fy<0.0f ) fy=0.0f;
    					if( fy>1.0f ) fy=1.0f;

    					orklut<float,float>::iterator it = data.begin()+miPoint;

    					if( miPoint != 0 && miPoint != (knumpoints-1) )
    					{
    						orklut<float,float>::iterator itp = it-1;
    						orklut<float,float>::iterator itn = it+1;
    						const float kfbound = float(kpntsize)/mParent->width();
    						if(itp!=data.end())
    						{	if( fx < (itp->first+kfbound) )
    							{	fx = (itp->first+kfbound);
    							}
    						}
    						if(itn!=data.end())
    						{	if( fx > (itn->first-kfbound) )
    							{	fx = (itn->first-kfbound);
    							}
    						}
    					}
    					data.RemoveItem( it );
    					data.AddSorted( fx, fy );
    					mParent->SigInvalidateProperty();
        			}
        		}
                break;
            }
            default:
                break;
        }
	}
};


void GedCurveEditPoint::Describe()
{
}

class GedCurveEditSeg : public GedObject
{
	RttiDeclareAbstract( GedCurveEditSeg, GedObject );

	MultiCurve1D*											mCurveObject;
	GedItemNode*											mParent;
	int														miSeg;

public:

	void OnMouseDoubleClicked(const ork::ui::Event& ev) final
	{
		if( mParent && mCurveObject )
		{
			if( ev.IsButton0DownF() )
			{	mCurveObject->SplitSegment(miSeg);
				mParent->SigInvalidateProperty();
			}
			else if(ev.IsButton2DownF())
			{
				QMenu *pMenu = new QMenu(0);
				QAction *pchildmenu0 = pMenu->addAction( "Seg:Lin" );
				QAction *pchildmenu1 = pMenu->addAction( "Seg:Box" );
				QAction *pchildmenu2 = pMenu->addAction( "Seg:Log" );
				QAction *pchildmenu3 = pMenu->addAction( "Seg:Exp" );

				pchildmenu0->setData( QVariant( "lin" ) );
				pchildmenu1->setData( QVariant( "box" ) );
				pchildmenu2->setData( QVariant( "log" ) );
				pchildmenu3->setData( QVariant( "exp" ) );

				QAction* pact = pMenu->exec(QCursor::pos());

				if( pact )
				{
					QVariant UserData = pact->data();
					QString UserName = UserData.toString();
					std::string sval = UserName.toStdString();
					if( sval == "lin" ) mCurveObject->SetSegmentType(miSeg,EMCST_LINEAR);
					if( sval == "box" ) mCurveObject->SetSegmentType(miSeg,EMCST_BOX);
					if( sval == "log" )	mCurveObject->SetSegmentType(miSeg,EMCST_LOG);
					if( sval == "exp" )	mCurveObject->SetSegmentType(miSeg,EMCST_EXP);
					mParent->SigInvalidateProperty();
				}

			}
		}
	}

	void SetSeg( int idx ) { miSeg=idx; }

	GedCurveEditSeg() : mCurveObject(0), mParent(0), miSeg(-1) {}

	void SetCurveObject( MultiCurve1D* pgrad ) { mCurveObject=pgrad; }
	void SetParent( GedItemNode* ppar ) { mParent=ppar; }
};

void GedCurveEditSeg::Describe()
{
}


///////////////////////////////////////////////////////////////////////////////

class GedCurveV4Widget : public GedItemNode
{
	static const int										kpoolsize = 32;
	ork::pool<GedCurveEditPoint>							mEditPoints;
	ork::pool<GedCurveEditSeg>								mEditSegs;
	MultiCurve1D*											mCurveObject;
	//ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16>	mVertexBuffer;

	bool DoDrawDefault() const { return false; } // virtual

	void DoDraw( lev2::Context* pTARG ) // virtual
	{
		const orklut<float,float> & data = mCurveObject->GetVertices();

		GetSkin()->DrawBgBox( this, miX, miY+2, miW, kh-3, GedSkin::ESTYLE_BACKGROUND_1 );
		GedSkin::GedPrim prim;
		prim.mDrawCB = CurveCustomPrim;
		prim.mpNode = this;
		prim.meType = ork::lev2::EPRIM_MULTI;
		prim.iy1 = miY;
		prim.iy2 = miY+kh;
		GetSkin()->AddPrim( prim );

		////////////////////////////////////

		const int knumpoints = (int) data.size();
		const int ksegs = knumpoints-1;

		////////////////////////////////////
		// draw segments

		if( pTARG->FBI()->IsPickState() )
		{
			mEditSegs.clear();

			for( int i=0; i<ksegs; i++ )
			{
				std::pair<float,float> pointa = data.GetItemAtIndex(i);
				std::pair<float,float> pointb = data.GetItemAtIndex(i+1);

				GedCurveEditSeg* editseg = mEditSegs.allocate();
				editseg->SetCurveObject( mCurveObject );
				editseg->SetParent(this);
				editseg->SetSeg( i );

				float fi0 = pointa.first;
				float fi1 = pointb.first;
				float fw = (fi1-fi0);

				int fx0 = miX+int(fi0*float(miW));
				int fw0 = int(fw*float(miW));

				GetSkin()->DrawBgBox( editseg, fx0, miY, fw0, kh, GedSkin::ESTYLE_DEFAULT_CHECKBOX, 1 );
				GetSkin()->DrawOutlineBox( editseg, fx0, miY, fw0, kh, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT, 1 );

			}		
		}		

		////////////////////////////////////
		// draw points

		mEditPoints.clear();

		for( int i=0; i<knumpoints; i++ )
		{
			std::pair<float,float> point = data.GetItemAtIndex(i);

			GedCurveEditPoint* editpoint = mEditPoints.allocate();
			editpoint->SetCurveObject( mCurveObject );
			editpoint->SetParent(this);
			editpoint->SetPoint( i );

			int fxc = int(point.first*float(miW));
			int fyc = int(point.second*float(kh));
			int fx0 = miX+(fxc-kpntsize);
			int fy0 = miY+(kh-(kpntsize))-fyc;

			GedSkin::ESTYLE pntstyl = IsObjectHilighted(editpoint) ? GedSkin::ESTYLE_DEFAULT_HIGHLIGHT : GedSkin::ESTYLE_DEFAULT_CHECKBOX;
			if( pTARG->FBI()->IsPickState() )
			{
				GetSkin()->DrawBgBox( editpoint, fx0, fy0, kpntsize*2, kpntsize*2, pntstyl, 2 );
			}
			else
			{
				GetSkin()->DrawOutlineBox( editpoint, fx0+1, fy0+1, kpntsize*2-2, kpntsize*2-2, pntstyl, 2 );
			}
			GetSkin()->DrawOutlineBox( editpoint, fx0, fy0, kpntsize*2, kpntsize*2, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT, 2 );

		}		

		////////////////////////////////////

	}
	
	static void CurveCustomPrim( GedSkin*pskin,GedObject*pnode,ork::lev2::Context* pTARG ) 
	{
		GedCurveV4Widget* pthis = rtti::autocast(pnode);
		const orklut<float,float> & data = pthis->mCurveObject->GetVertices();
		const int knumpoints = (int) data.size();
		const int ksegs = knumpoints-1;

		if( 0 == ksegs ) return;
	
		if( pTARG->FBI()->IsPickState() )
		{
		}
		else if(pthis->mCurveObject )
		{
			lev2::DynamicVertexBuffer<lev2::SVtxV12C4T16>& VB = lev2::GfxEnv::GetSharedDynamicVB();
			lev2::GfxMaterial3DSolid gridmat( pTARG );
			gridmat.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR );
			gridmat._rasterstate.SetAlphaTest( ork::lev2::EALPHATEST_OFF );
			gridmat._rasterstate.SetCullTest( ork::lev2::ECULLTEST_OFF );
			gridmat._rasterstate.SetBlending( ork::lev2::EBLENDING_OFF );
			gridmat._rasterstate.SetDepthTest( ork::lev2::EDEPTHTEST_ALWAYS );
			gridmat._rasterstate.SetShadeModel( ork::lev2::ESHADEMODEL_SMOOTH );

			//pthis->mVertexBuffer.Reset();

			static const int kexplogsegs = 16;
			int inuml = 0;
			for( int i=0; i<ksegs; i++ )
			{
				switch( pthis->mCurveObject->GetSegmentType(i) )
				{	case EMCST_LINEAR: inuml+=2; break;
					case EMCST_BOX: inuml+=4; break;
					case EMCST_LOG: inuml+=kexplogsegs*2; break;
					case EMCST_EXP: inuml+=kexplogsegs*2; break;
				}
			}

			int ivbaseA = VB.GetNumVertices();
			int reserveA = 1024;
			
			lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
			vw.Lock( pTARG, & VB, reserveA );

			fvec2 uv;

			int icountA = 0;
			
			const float kz = 0.0f;

			float fx = float(pthis->miX);
			float fy = float(pthis->miY+pskin->GetScrollY());
			float fw = float(pthis->miW);
			float fh = float(kh);

			lev2::SVtxV12C4T16 v0( fvec3(fx,fy,kz), uv, 0xffffffff );
			lev2::SVtxV12C4T16 v1( fvec3(fx+fw,fy,kz), uv, 0xffffffff );
			lev2::SVtxV12C4T16 v2( fvec3(fx+fw,fy+fh,kz), uv, 0xffffffff );
			lev2::SVtxV12C4T16 v3( fvec3(fx,fy+fh,kz), uv, 0xffffffff );

			vw.AddVertex( v0 );
			vw.AddVertex( v1 );
			vw.AddVertex( v2 );

			vw.AddVertex( v0 );
			vw.AddVertex( v2 );
			vw.AddVertex( v3 ); 
			
			icountA += 6;

			float fmin = pthis->mCurveObject->GetMin();
			float fmax = pthis->mCurveObject->GetMax();
			float frng = (fmax-fmin);

			for( int i=0; i<ksegs; i++ )
			{
				std::pair<float,float> data_a = data.GetItemAtIndex(i);
				std::pair<float,float> data_b = data.GetItemAtIndex(i+1);

				float fia = data_a.first;
				float fib = data_b.first;
				float fiya = data_a.second;
				float fiyb = data_b.second;

				float fx0 = fx+(fia*fw);
				float fx1 = fx+(fib*fw);
				float fy0 = fy+fh-(fiya*fh);
				float fy1 = fy+fh-(fiyb*fh);
	
				switch( pthis->mCurveObject->GetSegmentType(i) )
				{
					case EMCST_LOG:
					case EMCST_EXP:
					{	
						for( int j=0; j<kexplogsegs; j++ )
						{	int k=j+1;
							float fj = float(j)/float(kexplogsegs);
							float fk = float(k)/float(kexplogsegs);
							float fiaL = (fj*fib)+(1.0f-fj)*fia;
							float fiaR = (fk*fib)+(1.0f-fk)*fia;
							float fsL = pthis->mCurveObject->Sample(fiaL);
							float fsR = pthis->mCurveObject->Sample(fiaR);

							float fuL = (fsL-fmin)/frng;
							float fuR = (fsR-fmin)/frng;

							fx0 = fx+(fiaL*fw);
							fx1 = fx+(fiaR*fw);
							fy0 = fy+fh-(fuL*fh);
							fy1 = fy+fh-(fuR*fh);
							lev2::SVtxV12C4T16 v0( fvec3(fx0,fy0,kz), uv, 0xffffffff );
							lev2::SVtxV12C4T16 v1( fvec3(fx1,fy1,kz), uv, 0xffffffff );
							vw.AddVertex( v0 );
							vw.AddVertex( v1 );
							icountA += 2;
						}
						break;
					}
					case EMCST_LINEAR:
					{	lev2::SVtxV12C4T16 v0( fvec3(fx0,fy0,kz), uv, 0xffffffff );
						lev2::SVtxV12C4T16 v1( fvec3(fx1,fy1,kz), uv, 0xffffffff );
						vw.AddVertex( v0 );
						vw.AddVertex( v1 );
						icountA += 2;
						break;
					}
					case EMCST_BOX:
					{	lev2::SVtxV12C4T16 v0( fvec3(fx0,fy0,kz), uv, 0xffffffff );
						lev2::SVtxV12C4T16 v1( fvec3(fx1,fy0,kz), uv, 0xffffffff );
						lev2::SVtxV12C4T16 v2( fvec3(fx1,fy1,kz), uv, 0xffffffff );
						vw.AddVertex( v0 );
						vw.AddVertex( v1 );
						vw.AddVertex( v1 );
						vw.AddVertex( v2 );
						icountA += 4;
						break;
					}
				}
			}
			vw.UnLock(pTARG);

			////////////////////////////////////////////////////////////////
			F32 fVPW = (F32) pTARG->FBI()->GetVPW(); 
			F32 fVPH = (F32) pTARG->FBI()->GetVPH();
			if( 0.0f == fVPW ) fVPW = 1.0f;
			if( 0.0f == fVPH ) fVPH = 1.0f;
			fmtx4 mtxortho = pTARG->MTXI()->Ortho( 0.0f, fVPW, 0.0f, fVPH, 0.0f, 1.0f );
			pTARG->MTXI()->PushPMatrix( mtxortho );
			pTARG->MTXI()->PushVMatrix( fmtx4::Identity );
			pTARG->MTXI()->PushMMatrix( fmtx4::Identity );
				pTARG->BindMaterial( & gridmat );
				pTARG->PushModColor( fvec3::Blue() );
					pTARG->GBI()->DrawPrimitive( VB, ork::lev2::EPRIM_TRIANGLES, ivbaseA, 6 );
				pTARG->PopModColor();
				pTARG->PushModColor( fvec3::White() );
					pTARG->GBI()->DrawPrimitive( VB, ork::lev2::EPRIM_LINES, ivbaseA+6, icountA-6 );
				pTARG->PopModColor();
			pTARG->MTXI()->PopPMatrix();
			pTARG->MTXI()->PopVMatrix();
			pTARG->MTXI()->PopMMatrix();
			////////////////////////////////////////////////////////////////

		}
		else
		{
			OrkAssert(false);
		}
	}
	
	int CalcHeight(void) { return kh; } // virtual 

public:

	GedCurveV4Widget( ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj )
		: GedItemNode( mdl, name, prop, obj)
		, mCurveObject( 0 )
		//, mVertexBuffer( 512, 0, ork::lev2::EPRIM_TRIANGLES )
		, mEditPoints(kpoolsize)
		, mEditSegs(kpoolsize)
	{
		mCurveObject = rtti::autocast( obj );

		if( 0 == mCurveObject )
		{
			const reflect::IObjectPropertyObject* pprop = rtti::autocast( GetOrkProp() );
			mCurveObject = rtti::autocast( pprop->Access(GetOrkObj()) );
		}

		if( 0 == mCurveObject )
		{
			const reflect::IObjectPropertyObject* pprop = rtti::autocast( GetOrkProp() );
			ObjProxy<MultiCurve1D>* proxy = rtti::autocast(pprop->Access(GetOrkObj()));
			mCurveObject = proxy->mParent;
		}
		
	}

};
		
void GedFactoryCurve::Describe() {}		

GedItemNode* GedFactoryCurve::CreateItemNode(ObjModel&mdl,const ConstString& Name,const reflect::IObjectProperty *prop,Object* obj) const
{
	GedItemNode* groupnode = new GedLabelNode( mdl, "curve", prop, obj );

	mdl.GetGedWidget()->PushItemNode( groupnode );

	GedItemNode* itemnode = new GedCurveV4Widget( 
		mdl, 
		Name.c_str(),
		prop,
		obj
		);


	mdl.GetGedWidget()->AddChild( itemnode );

	mdl.GetGedWidget()->PopItemNode( groupnode );


	return groupnode;
}
///////////////////////////////////////////////////////////////////////////////
} } }
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedFactoryCurve,"ged.factory.curve1d");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedCurveEditPoint,"GedCurveEditPoint");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedCurveEditSeg,"GedCurveEditSeg");
