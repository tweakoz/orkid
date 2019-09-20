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

static const float k90Degrees = Float::Pi() / float(2.0f);

////////////////////////////////////////////////////////////////////////////////

ManipTrans::ManipTrans( ManipManager& mgr )
	: Manip(mgr)
{

}

ManipSingleTrans::ManipSingleTrans(ManipManager& mgr)
	: ManipTrans(mgr)
{

}

ManipDualTrans::ManipDualTrans(ManipManager& mgr)
	: ManipTrans(mgr)
{

}

////////////////////////////////////////////////////////////////////////////////

ManipTX::ManipTX( ManipManager& mgr )
	: ManipSingleTrans(mgr)
{
 	mmRotModel.SetRotateZ( k90Degrees );
	mColor = fcolor4::Red();
}

ManipTY::ManipTY(ManipManager& mgr)
	: ManipSingleTrans(mgr)
{
	mColor = fcolor4::Green();
}

ManipTZ::ManipTZ(ManipManager& mgr)
	: ManipSingleTrans(mgr)
{
	mmRotModel.SetRotateX( k90Degrees );
	mColor = fcolor4::Blue();
}

ManipTXY::ManipTXY(ManipManager& mgr)
	: ManipDualTrans(mgr)
{
	mColor = fcolor4::Blue();
}

ManipTXZ::ManipTXZ(ManipManager& mgr)
	: ManipDualTrans(mgr)
{
	mColor = fcolor4::Green();
}

ManipTYZ::ManipTYZ(ManipManager& mgr)
	: ManipDualTrans(mgr)
{
	mColor = fcolor4::Red();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool ManipTrans::UIEventHandler( const ui::Event& EV )
{
	ork::fvec2 cm = EV.GetUnitCoordBP();

	//printf( "ManipTrans<%p>::UIEventHandler() evcod<%d>\n", this, int(pEV->miEventCode) );
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

void ManipTrans::HandleMouseDown(const ork::fvec2& pos)
{
	Camera *pcam = mManager.getActiveCamera();

	//printf( "ManipTrans::HandleMouseDown() pcam<%p>\n", pcam );
	if( pcam )
	{
		mManager.mManipHandler.Init( pos, pcam->mCameraData.GetIVPMatrix(), pcam->QuatC );
		mBaseTransform = mManager.mCurTransform;
		SelectBestPlane(pos);
	}
}

void ManipTrans::HandleMouseUp(const ork::fvec2& pos)
{
	mManager.DisableManip();
}

void ManipTrans::HandleDrag(const ork::fvec2& pos)
{

}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void ManipSingleTrans::DrawAxis(GfxTarget* pTARG) const
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
					gbi->DrawPrimitiveEML( ork::lev2::GfxPrimitives::GetAxisBoxVB() );
				}
				else
				{
					gbi->DrawPrimitiveEML( ork::lev2::GfxPrimitives::GetAxisLineVB() );
					gbi->DrawPrimitiveEML( ork::lev2::GfxPrimitives::GetAxisConeVB() );
				}
			}

			mtl->EndPass(pTARG);
		}
	}
	mtl->EndBlock(pTARG);

}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void ManipSingleTrans::Draw( GfxTarget *pTARG ) const
{
	fmtx4 Mat;
	fmtx4 VisMat;
	fmtx4 MatT;
	fmtx4 MatS;
	fmtx4 MatR;
	fvec3 pos;
	fquat rot;
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
	if( GetClass() == ManipTX::GetClassStatic() )
	{
		v_dir = fvec4( 1.0f, 0.0f, 0.0f, 0.0f );
	}
	else if( GetClass() == ManipTY::GetClassStatic() )
	{
		v_dir = fvec4( 0.0f, 1.0f, 0.0f, 0.0f );
	}
	else if( GetClass() == ManipTZ::GetClassStatic() )
	{
		v_dir = fvec4( 0.0f, 0.0f, 1.0f, 0.0f );
	}

	fmtx4 VMatrix = pTARG->MTXI()->RefVMatrix();
	fvec4 wvx = v_dir.Transform(VisMat);
	fvec4 clip_vdir = wvx.Transform(VMatrix);
	if( fabs( clip_vdir.GetZ() ) >= vizthresh )
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
	fquat neg_rot;

	//mManager.mCurTransform.GetMatrix(MatCur);
	//MatCur.DecomposeMatrix(pos, rot, scale);

	if( mManager.mbWorldTrans )
		rot.Identity();

	MatCur.ComposeMatrix(pos, rot, mManager.GetManipScale());

	if( GetClass() == ManipTX::GetClassStatic() )
		neg_rot.FromAxisAngle(fvec4(0.0f, 1.0f, 0.0f, PI));
	else if( GetClass() == ManipTY::GetClassStatic() )
		neg_rot.FromAxisAngle(fvec4(1.0f, 0.0f, 0.0f, PI));
	else if( GetClass() == ManipTZ::GetClassStatic() )
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

void ManipSingleTrans::HandleDrag(const ork::fvec2& pos)
{
	Camera *pcam = mManager.getActiveCamera();

	IntersectWithPlanes( pos );
	bool bisect = CheckIntersect();
	//printf( "bisect<%d>\n", int(bisect) );
	if (  bisect )
	{
		fvec3 isect = mActiveIntersection->mIntersectionPoint;

		fmtx4 mtx_bas;
		fmtx4 mtx_inv;
		mBaseTransform.GetMatrix(mtx_bas);
		mtx_inv.inverseOf(mtx_bas);

		fvec3 isect_loc = isect.Transform( mtx_inv );

		if(GetClass() == ManipTX::GetClassStatic())
		{
			isect_loc.SetY(0.0f);
			isect_loc.SetZ(0.0f);
		}
		else if(GetClass() == ManipTY::GetClassStatic())
		{
			isect_loc.SetX(0.0f);
			isect_loc.SetZ(0.0f);
		}
		else if(GetClass() == ManipTZ::GetClassStatic())
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

void ManipDualTrans::Draw(GfxTarget *pTARG ) const
{
	fmtx4 MatCur;
	fvec3 pos;
	fquat rot;
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
		ork::lev2::GfxPrimitives::RenderQuad(pTARG, v0, v1, v2, v3);
	}
	pTARG->MTXI()->PopMMatrix();
}

void ManipDualTrans::HandleDrag(const ork::fvec2& pos)
{
	/*Camera *pcam = mManager.getActiveCamera();
	ui::Viewport *pVP = pcam->GetViewport();

	ork::fmtx4 view;// = pcam->GetVMatrix();
	ork::fmtx4 proj;// = pcam->GetPMatrix();

	ork::fmtx4 tform;
	ork::fvec3 pos = mManager.mCurTransform.GetTransform()->GetPosition();
	ork::fquat rot;
	float scale = 1.0f;
	tform.ComposeMatrix(pos, rot, scale);
	tform.inverseOf(tform);

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
	if(GetClass() == ManipTXY::GetClassStatic())
		norm.SetZ(1);
	else if(GetClass() == ManipTXZ::GetClassStatic())
		norm.SetY(1);
	else if(GetClass() == ManipTYZ::GetClassStatic())
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

void ManipTXY::GetQuad(float ext, ork::fvec4& v0, ork::fvec4& v1, ork::fvec4& v2, ork::fvec4& v3) const
{
	v0.Set(-ext, ext, 0, 1);
	v1.Set(ext, ext, 0, 1);
	v3.Set(ext, -ext, 0, 1);
	v2.Set(-ext, -ext, 0 , 1);
}

void ManipTXZ::GetQuad(float ext, ork::fvec4& v0, ork::fvec4& v1, ork::fvec4& v2, ork::fvec4& v3) const
{
	v0.Set(-ext, 0, ext, 1);
	v1.Set(ext, 0, ext, 1);
	v3.Set(ext, 0, -ext, 1);
	v2.Set(-ext, 0, -ext, 1);
}

void ManipTYZ::GetQuad(float ext, ork::fvec4& v0, ork::fvec4& v1, ork::fvec4& v2, ork::fvec4& v3) const
{
	v0.Set(0, ext, ext, 1);
	v1.Set(0, ext, -ext, 1);
	v3.Set(0, -ext, -ext, 1);
	v2.Set(0, -ext, ext, 1);
}

////////////////////////////////////////////////////////////////////////////////

} }
