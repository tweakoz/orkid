////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <orktool/manip/manip.h>
//
#include <ork/math/misc_math.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/ui/event.h>

#define MatRotScale 1.0f

namespace ork { namespace lev2 {

////////////////////////////////////////////////////////////////////////////////

CManipRot::CManipRot( CManipManager& mgr, const fvec4 & LocRotMat )
	: CManip(mgr)
	, mLocalRotationAxis( LocRotMat )
{
}

CManipRX::CManipRX( CManipManager& mgr )
	: CManipRot(mgr,fvec4( 1.0f, 0.0f, 0.0f) )
{
	mmRotModel.SetRotateZ( CFloat::Pi() / float(2.0f) );
	mColor = fcolor4::Red();
}

CManipRY::CManipRY( CManipManager& mgr )
	: CManipRot(mgr,fvec4( 0.0f, 1.0f, 0.0f))
{
	mColor = fcolor4::Green();
}

CManipRZ::CManipRZ(CManipManager& mgr)
	: CManipRot(mgr,fvec4( 0.0f, 0.0f, 1.0f))
{
	mmRotModel.SetRotateX( CFloat::Pi() / float(2.0f) );
	mColor = fcolor4::Blue();
}

////////////////////////////////////////////////////////////////////////////////

F32 CManipRX::CalcAngle( fvec4 & inv_isect, fvec4 & inv_lisect ) const
{
	F32 angle2 = rect2pol_ang( inv_isect.GetY(), inv_isect.GetZ() );
	F32 angle1 = rect2pol_ang( inv_lisect.GetY(), inv_lisect.GetZ() );
	return angle1-angle2;
}
F32 CManipRY::CalcAngle( fvec4 & inv_isect, fvec4 & inv_lisect ) const
{
	F32 angle1 = rect2pol_ang( inv_isect.GetX(), inv_isect.GetZ() );
	F32 angle2 = rect2pol_ang( inv_lisect.GetX(), inv_lisect.GetZ() );
	return angle1-angle2;
}
F32 CManipRZ::CalcAngle( fvec4 & inv_isect, fvec4 & inv_lisect ) const
{
	F32 angle2 = rect2pol_ang( inv_isect.GetX(), inv_isect.GetY() );
	F32 angle1 = rect2pol_ang( inv_lisect.GetX(), inv_lisect.GetY() );
	return angle1-angle2;
}

////////////////////////////////////////////////////////////////////////////////

void CManipRot::Draw( GfxTarget *pTARG ) const
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

	bool bdrawok = true;
	fvec4 v_dir;
	const float vizthresh(0.15f);
	if( GetClass() == CManipRX::GetClassStatic() )
	{
		v_dir = fvec4( 1.0f, 0.0f, 0.0f, 0.0f );
	}
	else if( GetClass() == CManipRY::GetClassStatic() )
	{
		v_dir = fvec4( 0.0f, 1.0f, 0.0f, 0.0f );
	}
	else if( GetClass() == CManipRZ::GetClassStatic() )
	{
		v_dir = fvec4( 0.0f, 0.0f, 1.0f, 0.0f );
	}

	fmtx4 VMatrix = pTARG->MTXI()->RefVMatrix();
	fvec4 wvx = v_dir.Transform(VisMat);
	fvec4 clip_vdir = wvx.Transform(VMatrix);
	if( CFloat::Abs( clip_vdir.GetZ() ) <= vizthresh )
	{
		bdrawok = false;
	}

	///////////////////////////////////////////////
	///////////////////////////////////////////////

	if(mManager.mbWorldTrans)
	{
		MatT.SetToIdentity();
		MatR.SetToIdentity();
		MatT.Translate(pos);

		MatR = rot.ToMatrix();
	}

	MatS.SetScale(mManager.GetManipScale() * MatRotScale);
	Mat = MatS * mmRotModel * MatR * MatT;

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

	fcolor4 ModColor = pTARG->RefModColor(); 


	pTARG->MTXI()->PushMMatrix(Mat);
	pTARG->PushModColor( ModColor*ColorScale );
	{
		pTARG->FXI()->InvalidateStateBlock();

		CVtxBuffer<SVtxV12C4T16>& vb = ork::lev2::CGfxPrimitives::GetCircleStripVB();

		int inumpasses = mManager.GetMaterial()->BeginBlock(pTARG);

		for( int ipass=0; ipass<inumpasses; ipass++ )
		{
			bool bDRAW = mManager.GetMaterial()->BeginPass( pTARG, ipass );

			if( bDRAW )
			{
				//DrawPrimitiveEML( VBuf, eType, bwire );
				pTARG->GBI()->DrawPrimitiveEML( vb );
			}

			mManager.GetMaterial()->EndPass(pTARG);

		}
		mManager.GetMaterial()->EndBlock(pTARG);
	}
	//ork::lev2::CGfxPrimitives::RenderCircleStrip( pTARG );
	pTARG->PopModColor();
	pTARG->MTXI()->PopMMatrix();
}

////////////////////////////////////////////////////////////////////////////////

float SnapReal( float Input, float SnapVal )
{
	int ival = int(Input/SnapVal);

	float ret( float(ival)*SnapVal );
		
	orkprintf( "SnapReal %f [%d] -> %f \n", Input, ival, ret );

	return ret;
}

////////////////////////////////////////////////////////////////////////////////

bool CManipRot::UIEventHandler( const ui::Event& EV )
{	
	int ex = EV.miX;
	int ey = EV.miY;
	
	fvec2 posubp = EV.GetUnitCoordBP();

	CCamera *pcam = mManager.getActiveCamera();
	
	bool brval = false;
			
	bool isshift = false; //CSystem::IsKeyDepressed(VK_SHIFT );
	bool isctrl = false; //CSystem::IsKeyDepressed(VK_CONTROL );
	
	switch( EV.miEventCode )
	{
		case ui::UIEV_PUSH:
		{	
			mManager.mManipHandler.Init(posubp, pcam->mCameraData.GetIVPMatrix(), pcam->QuatC );
			mBaseTransform = mManager.mCurTransform;

			SelectBestPlane(posubp);

			brval = true;
		}
		break;

		case ui::UIEV_RELEASE:
		{
			mManager.DisableManip();

			brval = true;
		}
		break;

		case ui::UIEV_DRAG:
		{
			IntersectWithPlanes( posubp );

			if ( CheckIntersect() )
			{	
				///////////////////////////////////////////
				// calc normalvectors from base:origin to point on activeintersection plane (in world space)
				const fvec3 & Origin = mBaseTransform.GetTransform().GetPosition();
				fvec3 D1 = (Origin-mActiveIntersection->mIntersectionPoint).Normal();
				fvec3 D0 = (Origin-mActiveIntersection->mBaseIntersectionPoint).Normal();
				///////////////////////////////////////////
				// calc matrix to put worldspace vector into plane local space
				fmtx4 MatWldToObj = mBaseTransform.GetTransform().GetMatrix(); //GetRotation();
				MatWldToObj.Inverse();
				fvec4 bAxisAngle = mLocalRotationAxis; 
				CQuaternion brq;
				brq.FromAxisAngle(bAxisAngle);
				fmtx4 MatObjToPln = brq.ToMatrix();
				MatObjToPln.Inverse();
				fmtx4 MatWldToPln = MatObjToPln*MatWldToObj;
				//fmtx4 MatInvRot = InvQuat.ToMatrix();
				///////////////////////////////////////////
				// calc plane local rotation
				fvec4 AxisAngle = mLocalRotationAxis; 
				fvec4 D0I = fvec4(D0,float(0.0f)).Transform(MatWldToPln);
				fvec4 D1I = fvec4(D1,float(0.0f)).Transform(MatWldToPln);
				//orkprintf( "D0 <%f %f %f>\n", float(D0.GetX()), float(D0.GetY()), float(D0.GetZ()) );
				//orkprintf( "D1 <%f %f %f>\n", float(D1.GetX()), float(D1.GetY()), float(D1.GetZ()) );
				//orkprintf( "D0I <%f %f %f>\n", float(D0I.GetX()), float(D0I.GetY()), float(D0I.GetZ()) );
				//orkprintf( "D1I <%f %f %f>\n", float(D1I.GetX()), float(D1I.GetY()), float(D1I.GetZ()) );
				AxisAngle.SetW( CalcAngle(D0I,D1I) );
				CQuaternion RotQ;
				RotQ.FromAxisAngle( AxisAngle );
				///////////////////
				// Rot Snap
				if( isshift )
				{	float SnapAngleVal( PI2/16.0f );
					fvec4 NewAxisAngle = RotQ.ToAxisAngle();
					float Angle = NewAxisAngle.GetW();
					Angle = SnapReal( Angle, SnapAngleVal );
					NewAxisAngle.SetW( Angle );
					RotQ.FromAxisAngle( NewAxisAngle );
				}
				///////////////////
				// accum rotation
				CQuaternion oq = mBaseTransform.GetTransform().GetRotation();
				CQuaternion NewQ = RotQ.Multiply(oq);
				///////////////////
				// Rot Reset To Identity
				if( isctrl && isshift )
				{
					NewQ.FromAxisAngle( fvec4( float(0.0f), float(1.0f), float(0.0f), float(0.0f) ) );
				}
				///////////////////
				TransformNode mset = mManager.mCurTransform;
				mset.GetTransform().SetRotation( NewQ );
				mManager.ApplyTransform( mset );
				///////////////////
			}

			brval = true;
		}
		break;

		default:
			break;
	}

	return brval;
}

////////////////////////////////////////////////////////////////////////////////


} // namespace lev2
} // namespace ork
