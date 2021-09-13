////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/math/cvector2.h>
#include <ork/math/line.h>
#include <ork/math/plane.h>
#include <ork/math/frustum.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

void Frustum::SupportMapping(const vec3_type& v, vec3_type& result ) const
{
    float num3;
    //const fvec3* pres = 0;
    num3 = 0.0f; //mNearCorners[0].Dot(v); //fvec3.Dot(this.corners[0], v, num3);
    for (int i=0; i<4; i++ )
    {
        float num2;
        num2 = mNearCorners[i].Dot(v); //, ref v, out num2);
        if (num2 > num3)
        {
            result = mNearCorners[i];
            num3 = num2;
        }
        num2 = mFarCorners[i].Dot(v); //, ref v, out num2);
        if (num2 > num3)
        {
            result = mFarCorners[i];
            num3 = num2;
        }
    }
   // result = *pres;
}

///////////////////////////////////////////////////////////////////////////////

void Frustum::CalcCorners()
{
    //private const int NearPlaneIndex = 0;
    //private const int FarPlaneIndex = 1;
    //private const int LeftPlaneIndex = 2;
    //private const int RightPlaneIndex = 3;
    //private const int TopPlaneIndex = 4;
    //private const int BottomPlaneIndex = 5;

	vec3_type ray_dir, ray_pos;

	bool bv;

	float planedist = 0.0f;

	//mNearCorners[4]; // tl tr br bl

	ray_type testray;

    _nearPlane.PlaneIntersect( _leftPlane, testray.mOrigin, testray.mDirection );
    bv = _topPlane.Intersect    (testray, planedist, mNearCorners[0] );	// corner 0
    bv = _bottomPlane.Intersect (testray, planedist, mNearCorners[3] );	// corner 3

	_rightPlane.PlaneIntersect( _nearPlane, testray.mOrigin, testray.mDirection );
    bv = _topPlane.Intersect    ( testray, planedist, mNearCorners[1] );	// corner 1
    bv = _bottomPlane.Intersect ( testray, planedist, mNearCorners[2] );	// corner 2

	_leftPlane.PlaneIntersect(  _farPlane, testray.mOrigin, testray.mDirection );
    bv = _topPlane.Intersect    ( testray, planedist, mFarCorners[0] );	// corner 4
    bv = _bottomPlane.Intersect ( testray, planedist, mFarCorners[3] );	// corner 7

	_farPlane.PlaneIntersect( _rightPlane, testray.mOrigin, testray.mDirection );
    _topPlane.Intersect         ( testray, planedist, mFarCorners[1] );	// corner 5
    _bottomPlane.Intersect      ( testray, planedist, mFarCorners[2] );	// corner 6

}

///////////////////////////////////////////////////////////////////////////////

void Frustum::set( const mtx44_type& IVPMatrix )
{
	float minv = -1.0f;
	float maxv = 1.0f;
	float minz = 0.0f;
	float maxz = 1.0f;

	vec4_type Vx0y0(minv, maxv, minz);
	vec4_type Vx1y0(maxv, maxv, minz);
	vec4_type Vx1y1(maxv, minv, minz);
	vec4_type Vx0y1(minv, minv, minz);

	mtx44_type::UnProject( IVPMatrix, Vx0y0, mNearCorners[0] );
	mtx44_type::UnProject( IVPMatrix, Vx1y0, mNearCorners[1] );
	mtx44_type::UnProject( IVPMatrix, Vx1y1, mNearCorners[2] );
	mtx44_type::UnProject( IVPMatrix, Vx0y1, mNearCorners[3] );

	Vx0y0.setZ(maxz);
	Vx1y0.setZ(maxz);
	Vx1y1.setZ(maxz);
	Vx0y1.setZ(maxz);

	mtx44_type::UnProject( IVPMatrix, Vx0y0, mFarCorners[0] );
	mtx44_type::UnProject( IVPMatrix, Vx1y0, mFarCorners[1] );
	mtx44_type::UnProject( IVPMatrix, Vx1y1, mFarCorners[2] );
	mtx44_type::UnProject( IVPMatrix, Vx0y1, mFarCorners[3] );

	vec3_type camrayN, camrayF;

	mtx44_type::UnProject( IVPMatrix, fvec4(0.0f,0.0f,minz), camrayN );
	mtx44_type::UnProject( IVPMatrix, fvec4(0.0f,0.0f,maxz), camrayF );

    vec4_type camrayHALF = (camrayN+camrayF)*float(0.5f);

	mXNormal = mFarCorners[1] - mFarCorners[0];
	mYNormal = mFarCorners[3] - mFarCorners[0];
    mZNormal = (camrayF-camrayN);
    mXNormal.Normalize();
    mYNormal.Normalize();
    mZNormal.Normalize();

	vec3_type  inNormal = mZNormal*float(-1.0f);
	_nearPlane.CalcFromNormalAndOrigin( mZNormal, camrayN );
    _farPlane.CalcFromNormalAndOrigin( inNormal, camrayF );

	double t = EPSILON;
    _topPlane.CalcPlaneFromTriangle( mFarCorners[1], mFarCorners[0], mNearCorners[0], EPSILON );
    _bottomPlane.CalcPlaneFromTriangle( mNearCorners[3], mFarCorners[3], mFarCorners[2],EPSILON );
    _leftPlane.CalcPlaneFromTriangle( mNearCorners[0], mFarCorners[0], mFarCorners[3] ,EPSILON );
    _rightPlane.CalcPlaneFromTriangle( mNearCorners[2], mFarCorners[2], mFarCorners[1],EPSILON );
	//CalcCorners()l

	mCenter = (  mFarCorners[0]+mFarCorners[1]+mFarCorners[2]+mFarCorners[3]
			   + mNearCorners[0]+mNearCorners[1]+mNearCorners[2]+mNearCorners[3] ) * 0.125f;

#if 0// test (camrayHALF should always be infront of planes, => all of these should return > 0
    F32 Dn = _nearPlane.pointDistance( camrayHALF );
    F32 Df = _farPlane.pointDistance( camrayHALF );
    F32 Dt = _topPlane.pointDistance( camrayHALF );
    F32 Db = _bottomPlane.pointDistance( camrayHALF );
    F32 Dl = _leftPlane.pointDistance( camrayHALF );
    F32 Dr = _rightPlane.pointDistance( camrayHALF );
    orkprintf( "Dn %f Df %f Dt %f Db %f Dl %f Dr %f\n", Dn, Df, Dt, Db, Dl, Dr );
#endif
}

void Frustum::set( const mtx44_type& VMatrix, const mtx44_type& PMatrix )
{
	mtx44_type IVPMatrix;
	mtx44_type VPMatrix = VMatrix*PMatrix;
	IVPMatrix.inverseOf(VPMatrix);
	set( IVPMatrix );
}

///////////////////////////////////////////////////////////////////////////////

bool	Frustum::contains(const vec3_type& v) const
{
	if(_topPlane.IsPointBehind(v))
		return false;

	if(_bottomPlane.IsPointBehind(v))
		return false;

	if(_leftPlane.IsPointBehind(v))
		return false;

	if(_rightPlane.IsPointBehind(v))
		return false;

	if(_nearPlane.IsPointBehind(v))
		return false;

	if(_farPlane.IsPointBehind(v))
		return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

