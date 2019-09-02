////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/kernel/prop.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <orktool/manip/manip.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/ui/event.h>

namespace ork { namespace lev2 {

static const float k90Degrees = CFloat::Pi() / float(2.0f);

////////////////////////////////////////////////////////////////////////////////

CManipTrans::CManipTrans( CManipManager& mgr )
	: CManip(mgr)
{

}

CManipSingleTrans::CManipSingleTrans(CManipManager& mgr)
	: CManipTrans(mgr)
{

}

CManipDualTrans::CManipDualTrans(CManipManager& mgr)
	: CManipTrans(mgr)
{
	
}

////////////////////////////////////////////////////////////////////////////////

CManipTX::CManipTX( CManipManager& mgr )
	: CManipSingleTrans(mgr)
{
 	mmRotModel.SetRotateZ( k90Degrees );
	mColor = fcolor4::Red();
}

CManipTY::CManipTY(CManipManager& mgr)
	: CManipSingleTrans(mgr)
{
	mColor = fcolor4::Green();
} 

CManipTZ::CManipTZ(CManipManager& mgr)
	: CManipSingleTrans(mgr)
{
	mmRotModel.SetRotateX( k90Degrees );
	mColor = fcolor4::Blue();
}

CManipTXY::CManipTXY(CManipManager& mgr)
	: CManipDualTrans(mgr)
{
	mColor = fcolor4::Blue();
}

CManipTXZ::CManipTXZ(CManipManager& mgr)
	: CManipDualTrans(mgr)
{
	mColor = fcolor4::Green();
}

CManipTYZ::CManipTYZ(CManipManager& mgr)
	: CManipDualTrans(mgr)
{
	mColor = fcolor4::Red();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CManipTrans::UIEventHandler( const ui::Event& EV )
{	
	ork::fvec2 cm = EV.GetUnitCoordBP();

	//printf( "CManipTrans<%p>::UIEventHandler() evcod<%d>\n", this, int(pEV->miEventCode) );
	bool brval = false;
	switch( EV.miEventCode )
	{	case ui::UIEV_PUSH:
		{	HandleMouseDown(cm);
			brval = true;
			break;
		}
		case ui::UIEV_RELEASE:
		{
			HandleMouseUp(cm);
			brval = true;
			break;
		}
		case ui::UIEV_DRAG:
		{
			HandleDrag(cm);
			brval = true;
			break;
		}
		default:
			break;
	}
	return brval;
}

////////////////////////////////////////////////////////////////////////////////

void CManipTrans::HandleMouseDown(const ork::fvec2& pos)
{
	CCamera *pcam = mManager.getActiveCamera();

	//printf( "CManipTrans::HandleMouseDown() pcam<%p>\n", pcam );
	if( pcam )
	{
		mManager.mManipHandler.Init( pos, pcam->mCameraData.GetIVPMatrix(), pcam->QuatC );
		mBaseTransform = mManager.mCurTransform;
		SelectBestPlane(pos);
	}
}

void CManipTrans::HandleMouseUp(const ork::fvec2& pos)
{
	mManager.DisableManip();
}

void CManipTrans::HandleDrag(const ork::fvec2& pos)
{
	
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void CManipSingleTrans::DrawAxis(GfxTarget* pTARG) const
{
	auto mtl = mManager.GetMaterial();
	auto fbi = pTARG->FBI();
	auto fxi = pTARG->FXI();
	auto gbi = pTARG->GBI();

	fxi->InvalidateStateBlock();

	int inumpasses = mtl->BeginBlock(pTARG);
	{
		for( int ipass=0; ipass<inumpasses; ipass++ )
		{
			bool bDRAW = mtl->BeginPass( pTARG, ipass );

			if( bDRAW )
			{
				if( fbi->IsPickState() )
				{
					gbi->DrawPrimitiveEML( ork::lev2::CGfxPrimitives::GetAxisBoxVB() );
				}
				else
				{
					gbi->DrawPrimitiveEML( ork::lev2::CGfxPrimitives::GetAxisLineVB() );
					gbi->DrawPrimitiveEML( ork::lev2::CGfxPrimitives::GetAxisConeVB() );
				}
			}

			mtl->EndPass(pTARG);
		}
	}
	mtl->EndBlock(pTARG);

}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void CManipSingleTrans::Draw( GfxTarget *pTARG ) const 
{	
	fmtx4 Mat;
	fmtx4 VisMat;
	fmtx4 MatT;
	fmtx4 MatS;
	fmtx4 MatR; 
	fvec3 pos;
	CQuaternion rot;
	float scale;

	mManager.mCurTransform.GetMatrix(Mat);
	Mat.DecomposeMatrix(pos, rot, scale);
	VisMat.ComposeMatrix(pos, rot, 1.0f);

	////////////////////
	// plane/screen cross section check
	////////////////////

	bool bdrawok = true;
	fvec4 v_dir;
	const float vizthresh(0.90f);
	if( GetClass() == CManipTX::GetClassStatic() )
	{
		v_dir = fvec4( 1.0f, 0.0f, 0.0f, 0.0f );
	}
	else if( GetClass() == CManipTY::GetClassStatic() )
	{
		v_dir = fvec4( 0.0f, 1.0f, 0.0f, 0.0f );
	}
	else if( GetClass() == CManipTZ::GetClassStatic() )
	{
		v_dir = fvec4( 0.0f, 0.0f, 1.0f, 0.0f );
	}

	fmtx4 VMatrix = pTARG->MTXI()->RefVMatrix();
	fvec4 wvx = v_dir.Transform(VisMat);
	fvec4 clip_vdir = wvx.Transform(VMatrix);
	if( CFloat::Abs( clip_vdir.GetZ() ) >= vizthresh )
	{
		bdrawok = false;
	}
	float ColorScale = 1.0f;
	if(!bdrawok)
	{
		if(pTARG->FBI()->IsPickState())
			return;
		else
		{
			ColorScale = 0.3f;
		}
		
	}

	////////////////////
	// end plane/screen cross section check
	////////////////////

	fmtx4 MatCur;
	CQuaternion neg_rot;
	
	//mManager.mCurTransform.GetMatrix(MatCur);
	//MatCur.DecomposeMatrix(pos, rot, scale);

	if( mManager.mbWorldTrans )
		rot.Identity();

	MatCur.ComposeMatrix(pos, rot, mManager.GetManipScale());

	if( GetClass() == CManipTX::GetClassStatic() )
		neg_rot.FromAxisAngle(fvec4(0.0f, 1.0f, 0.0f, PI));
	else if( GetClass() == CManipTY::GetClassStatic() )
		neg_rot.FromAxisAngle(fvec4(1.0f, 0.0f, 0.0f, PI));
	else if( GetClass() == CManipTZ::GetClassStatic() )
		neg_rot.FromAxisAngle(fvec4(1.0f, 0.0f, 0.0f, PI));

	////////////////////////
	// Draw Positive Axis
	////////////////////////

	Mat = mmRotModel * MatCur;

	fcolor4 ModColor = pTARG->RefModColor(); 

	pTARG->MTXI()->PushMMatrix(Mat);
	pTARG->PushModColor( ModColor*ColorScale );
	DrawAxis(pTARG);
	pTARG->PopModColor();
	pTARG->MTXI()->PopMMatrix();

	////////////////////////
	// Draw Negative Axis
	////////////////////////

	fmtx4 MatN = mmRotModel * neg_rot.ToMatrix() * MatCur;

	if( false == pTARG->FBI()->IsPickState() )
	{
		ModColor = ModColor*0.5f;
	}
	pTARG->MTXI()->PushMMatrix(MatN);
	pTARG->PushModColor( ModColor*ColorScale );
	DrawAxis(pTARG);
	pTARG->PopModColor();
	pTARG->MTXI()->PopMMatrix();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void CManipSingleTrans::HandleDrag(const ork::fvec2& pos)
{
	CCamera *pcam = mManager.getActiveCamera();

	IntersectWithPlanes( pos );
	bool bisect = CheckIntersect();
	//printf( "bisect<%d>\n", int(bisect) );
	if (  bisect )
	{	
		fvec3 isect = mActiveIntersection->mIntersectionPoint;

		fmtx4 mtx_bas;
		fmtx4 mtx_inv;
		mBaseTransform.GetMatrix(mtx_bas);
		mtx_inv = mtx_bas;
		mtx_inv.Inverse();

		fvec3 isect_loc = isect.Transform( mtx_inv );

		if(GetClass() == CManipTX::GetClassStatic())
		{
			isect_loc.SetY(0.0f);
			isect_loc.SetZ(0.0f);
		}
		else if(GetClass() == CManipTY::GetClassStatic())
		{
			isect_loc.SetX(0.0f);
			isect_loc.SetZ(0.0f);
		}
		else if(GetClass() == CManipTZ::GetClassStatic())
		{
			isect_loc.SetX(0.0f);
			isect_loc.SetY(0.0f);
		}
			
		fvec3 isect_wld = isect_loc.Transform(mtx_bas); 

		if(pcam->mCameraData.GetFrustum().Contains(isect_wld))
		{
			mManager.mCurTransform.Translate(TransformNode::EMODE_ABSOLUTE, isect_wld);
			mManager.ApplyTransform(mManager.mCurTransform);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void CManipDualTrans::Draw(GfxTarget *pTARG ) const
{	
	fmtx4 MatCur;
	fvec3 pos;
	CQuaternion rot;
	float scale;

	mManager.mCurTransform.GetMatrix(MatCur);
	MatCur.DecomposeMatrix(pos, rot, scale);

	if( mManager.mbWorldTrans )
		rot.Identity();

	MatCur.ComposeMatrix(pos, rot, mManager.GetManipScale());

	ork::fvec4 v0, v1, v2, v3;
	GetQuad(1.0f, v0, v1, v2, v3);

	pTARG->MTXI()->PushMMatrix(MatCur);
	{
		pTARG->FXI()->InvalidateStateBlock();
		ork::lev2::CGfxPrimitives::RenderQuad(pTARG, v0, v1, v2, v3);
	}
	pTARG->MTXI()->PopMMatrix();
}

void CManipDualTrans::HandleDrag(const ork::fvec2& pos)
{
	/*CCamera *pcam = mManager.getActiveCamera();
	ui::Viewport *pVP = pcam->GetViewport();

	ork::fmtx4 view;// = pcam->GetVMatrix();
	ork::fmtx4 proj;// = pcam->GetPMatrix();

	ork::fmtx4 tform;
	ork::fvec3 pos = mManager.mCurTransform.GetTransform()->GetPosition();
	ork::CQuaternion rot;
	float scale = 1.0f;
	tform.ComposeMatrix(pos, rot, scale);
	tform.GEMSInverse(tform);

	ork::fvec3 snear = pVP->UnProject(ork::fvec3(start.GetX(), start.GetY(), 0), 
		proj, view, ork::fmtx4::Identity);

	ork::fvec3 sfar = pVP->UnProject(ork::fvec3(start.GetX(), start.GetY(), 1), 
		proj, view, ork::fmtx4::Identity);

	ork::fvec3 enear = pVP->UnProject(ork::fvec3(end.GetX(), end.GetY(), 0),
		proj, view, ork::fmtx4::Identity);

	ork::fvec3 efar = pVP->UnProject(ork::fvec3(end.GetX(), end.GetY(), 1),
		proj, view, ork::fmtx4::Identity);

	ork::fray3 sray(snear.Transform(tform).xyz(), (sfar - snear).Transform(tform).xyz().Normal());
	ork::fray3 eray(enear.Transform(tform).xyz(), (efar - enear).Transform(tform).xyz().Normal());

	ork::fvec3 norm;
	if(GetClass() == CManipTXY::GetClassStatic())
		norm.SetZ(1);
	else if(GetClass() == CManipTXZ::GetClassStatic())
		norm.SetY(1);
	else if(GetClass() == CManipTYZ::GetClassStatic())
		norm.SetX(1);

	ork::fplane3 plane(norm, 0);
	ork::fvec3 spt;
	ork::fvec3 ept;
	float d;

	if(!plane.Intersect(sray, d, spt) || !plane.Intersect(eray, d, ept))
		return;

	ork::fvec3 worlddiff = ept - spt;
	ork::fvec3 endpos = pos + worlddiff;

	if(pcam->mCameraData.GetFrustum().Contains(endpos))
	{
		mManager.mCurTransform.Translate(TransformNode3D::EMODE_ABSOLUTE, mManager.mCurTransform.GetTransform()->GetPosition() + worlddiff);
		mManager.ApplyTransform(mManager.mCurTransform);
	}
	*/
}

////////////////////////////////////////////////////////////////////////////////

void CManipTXY::GetQuad(float ext, ork::fvec4& v0, ork::fvec4& v1, ork::fvec4& v2, ork::fvec4& v3) const
{
	v0.Set(-ext, ext, 0, 1);
	v1.Set(ext, ext, 0, 1);
	v3.Set(ext, -ext, 0, 1);
	v2.Set(-ext, -ext, 0 , 1);
}

void CManipTXZ::GetQuad(float ext, ork::fvec4& v0, ork::fvec4& v1, ork::fvec4& v2, ork::fvec4& v3) const
{
	v0.Set(-ext, 0, ext, 1);
	v1.Set(ext, 0, ext, 1);
	v3.Set(ext, 0, -ext, 1);
	v2.Set(-ext, 0, -ext, 1);
}

void CManipTYZ::GetQuad(float ext, ork::fvec4& v0, ork::fvec4& v1, ork::fvec4& v2, ork::fvec4& v3) const
{
	v0.Set(0, ext, ext, 1);
	v1.Set(0, ext, -ext, 1);
	v3.Set(0, -ext, -ext, 1);
	v2.Set(0, -ext, ext, 1);
}

////////////////////////////////////////////////////////////////////////////////

} }
