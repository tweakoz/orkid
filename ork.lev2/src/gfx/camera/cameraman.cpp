////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/math/polar.h>
#include <math.h>
#include <ork/lev2/gfx/camera/cameraman.h>

INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::CCamera, "CCamera" );

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

void CCamera::Describe()
{
	ork::reflect::RegisterProperty( "Focus", & CCamera::CamFocus );
	ork::reflect::RegisterProperty( "Center", & CCamera::mvCenter );
	ork::reflect::RegisterProperty( "Loc", & CCamera::mfLoc );
	ork::reflect::RegisterProperty( "QuatC", & CCamera::QuatC );

	ork::reflect::AnnotatePropertyForEditor< CCamera >( "Loc", "editor.range.min", "0.1f" );
	ork::reflect::AnnotatePropertyForEditor< CCamera >( "Loc", "editor.range.max", "1000.0f" );
}

///////////////////////////////////////////////////////////////////////////////

CCamera::CCamera()
	: CamFocus(0.0f,0.0f,0.0f)
	, mfLoc( 2400.0f )
	, mvCenter(0.0f,0.0,0.0f)
	, QuatC( 0.0f, -1.0f, 0.0f, 0.0f )
	, locscale( 1.0f )
	, LastMeasuredCameraVelocity( 0.0f, 0.0f, 0.0f )
	, MeasuredCameraVelocity( 0.0f, 0.0f, 0.0f )
	, mbInMotion( false )
	, mpViewport(0)
	, mfWorldSizeAtLocator(1.0f)
	//, ManipHandler( *this )
{
	other_info = (std::string) "";
	mCameraData.SetLev2Camera(this);
	printf( "SETLEV2CAM<%p>\n", this );
}

std::string CCamera::get_full_name( void )
{
	std::string rval = type_name + (std::string) ":" + instance_name + (std::string) ":" + other_info;
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool CCamera::IsXVertical() const
{
	const CVector3& yn = mCameraData.GetYNormal();
	CReal dotY = yn.Dot( CVector3(1.0f,0.0f,0.0f) );
	return ( float(CFloat::Abs(dotY)) > CReal(0.707f) );
}

bool CCamera::IsYVertical() const
{
	const CVector3& yn = mCameraData.GetYNormal();
	CReal dotY = yn.Dot( CVector3(0.0f,1.0f,0.0f) );
	return ( float(CFloat::Abs(dotY)) > CReal(0.707f) );
}

bool CCamera::IsZVertical() const
{
	const CVector3& yn = mCameraData.GetYNormal();
	CReal dotY = yn.Dot( CVector3(0.0f,0.0f,1.0f) );
	return ( float(CFloat::Abs(dotY)) > CReal(0.707f) );
}

CQuaternion CCamera::VerticalRot( CReal amt ) const
{
	CQuaternion Quat;

	if( IsXVertical() )
	{
		CVector4 Rot( 1.0f, 0.0f, 0.0f, amt );
		Quat.FromAxisAngle( Rot );
	}
	else if( IsYVertical() )
	{
		const CVector3& yn = mCameraData.GetYNormal();
		float dotY = yn.Dot( CVector3(0.0f,1.0f,0.0f) );
		float fsign = (dotY>0.0f) ? 1.0f : (dotY<0.0f) ? -1.0f : 0.0f;

		CVector4 Rot( 0.0f, 1.0f, 0.0f, amt*fsign );
		Quat.FromAxisAngle( Rot );
	}
	else if( IsZVertical() )
	{
		CVector4 Rot( 0.0f, 0.0f, 1.0f, amt );
		Quat.FromAxisAngle( Rot );
	}
	return Quat;
}

CQuaternion CCamera::HorizontalRot( CReal amt ) const
{
	CQuaternion Quat;

	if( IsYVertical() )
	{
		CVector4 Rot( 1.0f, 0.0f, 0.0f, amt );
		Quat.FromAxisAngle( Rot );
	}
	else if( IsXVertical() )
	{
		CVector4 Rot( 0.0f, 1.0f, 0.0f, amt );
		Quat.FromAxisAngle( Rot );
	}
	else if( IsZVertical() )
	{
		CVector4 Rot( 0.0f, 0.0f, 1.0f, amt );
		Quat.FromAxisAngle( Rot );
	}
	return Quat;
}

///////////////////////////////////////////////////////////////////////////////
// motion state tracking

bool CCamera::CheckMotion()
{
	CUIViewport *pVP = GetViewport();

	LastMeasuredCameraVelocity = MeasuredCameraVelocity;
	MeasuredCameraVelocity = (CamLoc-PrevCamLoc);

	CReal CurVelMag = MeasuredCameraVelocity.Mag();
	CReal LastVelMag = LastMeasuredCameraVelocity.Mag();

	CVector2 VP;
	if( pVP )
	{
		VP.SetX( (float) pVP->GetW() );
		VP.SetY( (float) pVP->GetH() );
	}
	CVector3 Pos = mvCenter;
	CVector3 UpVector;
	CVector3 RightVector;
	mCameraData.GetPixelLengthVectors( Pos, VP, UpVector, RightVector );
	CReal CameraMotionThresh = RightVector.Mag()/CReal(1000.0f);

	if( mbInMotion )
	{
		if( CurVelMag<CameraMotionThresh ) // start motion
		{
			//GetViewport()->GetTarget()->GetCtxBase()->PopRefreshPolicy();
			mbInMotion = false;
		}
	}
	else
	{
		if( (LastVelMag<CameraMotionThresh) && (CurVelMag>CameraMotionThresh) ) // start motion
		{
			mbInMotion = true;
		}
	}

	return mbInMotion;
}
	
///////////////////////////////////////////////////////////////////////////////

void CCamera::CommonPostSetup( void )
{
	///////////////////////////////
    // billboard support

    F32 UpX = mCameraData.GetIVMatrix().GetElemXY( 0, 0 );
    F32 UpY = mCameraData.GetIVMatrix().GetElemXY( 0, 1 );
    F32 UpZ = mCameraData.GetIVMatrix().GetElemXY( 0, 2 );
    F32 RightX = mCameraData.GetIVMatrix().GetElemXY( 1, 0 );
    F32 RightY = mCameraData.GetIVMatrix().GetElemXY( 1, 1 );
    F32 RightZ = mCameraData.GetIVMatrix().GetElemXY( 1, 2 );

    vec_billboardUp = CVector4( UpX, UpY, UpZ );
    vec_billboardRight = CVector4( RightX, RightY, RightZ );

    ///////////////////////////////
    // generate frustum (useful for many things, like billboarding, clipping, LOD, etc.. )
    // we generate the frustum points, we should also generate plane eqns

	Frustum& frus = mCameraData.GetFrustum();
	frus.Set( mCameraData.GetIVPMatrix() );
	mCameraData.SetXNormal( frus.mXNormal );
	mCameraData.SetYNormal( frus.mYNormal );
    mCameraData.SetZNormal( frus.mZNormal );

    ///////////////////////////////

	CamLoc = mvCenter + ( mCameraData.GetZNormal() * (-mfLoc) );

}


///////////////////////////////////////////////////////////////////////////////

CReal CCamera::ViewLengthToWorldLength( const CVector4 &pos, CReal ViewLength )
{
    return CReal(0.0f);
}

///////////////////////////////////////////////////////////////////////////////

CManipHandler::CManipHandler() //const CCamera& pcam) 
	: Origin( CReal(0.0f), CReal(0.0f), CReal(0.0f) )
//	, mParentCamera(pcam)
{
}

void CManipHandler::Init( const ork::CVector2& posubp, const CMatrix4 & RCurIMVPMat, const CQuaternion & RCurQuat )
{
	IMVPMat = RCurIMVPMat;
	Quat = RCurQuat;
	
	///////////////////////////////////////

	mFrustum.Set( RCurIMVPMat );
 
	CamXNormal = mFrustum.mXNormal;
	CamYNormal = mFrustum.mYNormal;
	CamZNormal = mFrustum.mZNormal;

	///////////////////////////////////////

	IntersectXZ( posubp, XZIntersectBase, XZAngleBase );
	IntersectYZ( posubp, YZIntersectBase, YZAngleBase );
	IntersectXY( posubp, XYIntersectBase, XYAngleBase );    
}

///////////////////////////////////////////////////////////////////////////////

bool CManipHandler::IntersectXZ( const ork::CVector2& posubp, CVector3 &Intersection, CReal &Angle )
{
	CVector3 RayZNormal;
	GenerateIntersectionRays( posubp, RayZNormal, RayNear );
	YNormal = CMatrix4::Identity.GetYNormal();
	XZPlane.CalcFromNormalAndOrigin( YNormal, Origin );
	float isect_dist;
	Ray3 ray;
	ray.mOrigin = RayNear;
	ray.mDirection = RayZNormal;
	DoesIntersectXZ = XZPlane.Intersect( ray, isect_dist, Intersection );

	if( DoesIntersectXZ ) XZAngle = rect2pol_ang( Intersection.GetX(), Intersection.GetZ() );

	Angle = XZAngle;

	return DoesIntersectXZ;
}

///////////////////////////////////////////////////////////////////////////////

bool CManipHandler::IntersectYZ( const ork::CVector2& posubp, CVector3 &Intersection, CReal &Angle )
{
	CVector3 RayZNormal;
	GenerateIntersectionRays( posubp, RayZNormal, RayNear );
	XNormal = CMatrix4::Identity.GetXNormal();
	YZPlane.CalcFromNormalAndOrigin( XNormal, Origin );

	float isect_dist;
	Ray3 ray;
	ray.mOrigin = RayNear;
	ray.mDirection = RayZNormal;
	DoesIntersectYZ = YZPlane.Intersect( ray, isect_dist, Intersection );

	if( DoesIntersectYZ ) YZAngle = rect2pol_ang( Intersection.GetY(), Intersection.GetZ() );

	Angle = YZAngle;

	return DoesIntersectYZ;
}

///////////////////////////////////////////////////////////////////////////////

bool CManipHandler::IntersectXY( const ork::CVector2& posubp, CVector3 &Intersection, CReal &Angle )
{
	CVector3 RayZNormal;
	GenerateIntersectionRays( posubp, RayZNormal, RayNear );
	ZNormal = CMatrix4::Identity.GetZNormal();
	XYPlane.CalcFromNormalAndOrigin( ZNormal, Origin );
	float isect_dist;
	Ray3 ray;
	ray.mOrigin = RayNear;
	ray.mDirection = RayZNormal;
	DoesIntersectXY = XYPlane.Intersect( ray, isect_dist, Intersection );

	if( DoesIntersectXY ) XYAngle = rect2pol_ang( Intersection.GetX(), Intersection.GetY() );

	Angle = XYAngle;

	return DoesIntersectXY;
}

///////////////////////////////////////////////////////////////////////////////

void CManipHandler::Intersect( const ork::CVector2& posubp )
{
	CVector3 Intersection;
	CReal Angle;
	IntersectXZ( posubp, Intersection, Angle );
	IntersectXY( posubp, Intersection, Angle );
	IntersectYZ( posubp, Intersection, Angle );
	//Intersection.dump( "manipisec isXZ" );
	//Intersection.dump( "manipisec isYZ" );
	//Intersection.dump( "manipisec isXY" );
}

///////////////////////////////////////////////////////////////////////////////

CVector4 TRayN;
CVector4 TRayF;

#if defined(ORK_CONFIG_FLOAT64)
typedef double depthreal;
inline double DepthSqrt( double in ) { return sqrt(in); }
#else
typedef float depthreal;
inline float DepthSqrt( float in ) { return sqrtf(in); }
#endif

void CManipHandler::GenerateIntersectionRays( const ork::CVector2& posubp, CVector3& RayZNormal, CVector3& RayNear )  
{
	CVector3 RayFar;
	///////////////////////////////////////////
	CVector3 vWinN( posubp.GetX(), posubp.GetY(), 0.0f );
	CVector3 vWinF( posubp.GetX(), posubp.GetY(), 1.0f );
	CMatrix4::UnProject( IMVPMat, vWinN, RayNear );
	CMatrix4::UnProject( IMVPMat, vWinF, RayFar );
	TRayN = RayNear;
	TRayF = RayFar;
	///////////////////////////////////////////
	CVector3 RayD = (RayFar-RayNear);
	///////////////////////////////////////////
	depthreal draydX = (depthreal) RayD.GetX();
	depthreal draydY = (depthreal) RayD.GetY();
	depthreal draydZ = (depthreal) RayD.GetZ();
	depthreal drayD = 1.0f / DepthSqrt( (draydX*draydX) + (draydY*draydY) + (draydZ*draydZ) );
	draydX *= drayD;
	draydY *= drayD;
	draydZ *= drayD;
	///////////////////////////////////////////
	RayZNormal.SetXYZ( (f32) draydX, (f32) draydY, (f32) draydZ );
	///////////////////////////////////////////
}

} }
