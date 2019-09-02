////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/math/plane.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/collision_test.h>

#include <ork/math/frustum.h>
#include <ork/math/sphere.h>
#include <ork/math/box.h>
#include <ork/math/misc_math.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::FrustumPointTest( const Frustum& frus, const CVector3& pnt )
{
	CVector4 tpos( pnt );
	bool	bv = frus.mNearPlane.IsPointInFront( tpos );
			bv &= frus.mFarPlane.IsPointInFront( tpos );
			bv &= frus.mLeftPlane.IsPointInFront( tpos );
			bv &= frus.mRightPlane.IsPointInFront( tpos );
			bv &= frus.mTopPlane.IsPointInFront( tpos );
			bv &= frus.mBottomPlane.IsPointInFront( tpos );

	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::FrustumSphereTest( const Frustum& frus, const Sphere& sph )
{
	float nrad = -sph.mRadius;
	CVector4 tpos( sph.mCenter );
	
	float nd = frus.mNearPlane.GetPointDistance( tpos );
	float fd = frus.mFarPlane.GetPointDistance( tpos );
	float ld = frus.mLeftPlane.GetPointDistance( tpos );
	float rd = frus.mRightPlane.GetPointDistance( tpos );
	float td = frus.mTopPlane.GetPointDistance( tpos );
	float bd = frus.mBottomPlane.GetPointDistance( tpos );
	
	if(nd < nrad) return false;
	if(fd < nrad) return false;
	if(ld < nrad) return false;
	if(rd < nrad) return false;
	if(td < nrad) return false;
	if(bd < nrad) return false;
	
	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::FrustumCircleXZTest( const Frustum& frus, const Circle& cirXZ )
{
	Sphere sph( CVector3( cirXZ.mCenter.GetX(), frus.mCenter.GetY(), cirXZ.mCenter.GetY() ), cirXZ.mRadius );
	return FrustumSphereTest( frus, sph );
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::FrustumFrustumTest( const Frustum& frus1, const Frustum& frus2 )
{
	OrkAssertNotImpl();
	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::SphereAABoxTest( const Sphere& sph, const AABox& aab )
{
	OrkAssertNotImpl();
	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::FrustumAABoxTest( const Frustum& frus, const AABox& box )
{
	OrkAssertNotImpl();
	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::RayTriangleTest( const Ray3& ray, const CVector3& A, const CVector3& B, const CVector3& C, CVector3& isect, float& s, float& t )
{
	const CVector3& O = ray.mOrigin;
	const CVector3& D = ray.mDirection;

	///////////////////////////////////////////////////
	// first calc intersection to plane

	CVector3 vab = (B-A);
	CVector3 vac = (C-A);

	CVector3 Nabc = vab.Cross(vac);
	float dist_O_to_P = -(O-A).Dot(Nabc)/(D.Dot(Nabc));
	isect = O+(D*dist_O_to_P); // plane_isect
		
	if( dist_O_to_P<0.0f ) return false;


	///////////////////////////////////////////////////
	// calc s and t (pseudo-barycentric method)

	//CVector3 vop = (isect-O); // debugging?
	CVector3 voa = (A-O);
	CVector3 vob = (B-O);
	CVector3 voc = (C-O);

	CPlane PlaneOCA( voc.Cross(voa).Normal(), O );
	CPlane PlaneOBA( voa.Cross(vob).Normal(), O );
		
	t = PlaneOCA.GetPointDistance( isect )/PlaneOCA.GetPointDistance(B);
	s = PlaneOBA.GetPointDistance( isect )/PlaneOBA.GetPointDistance(C);

	return ((t>=0.0f)&&(t<=1.0f)&&(s>=0.0f)&&(s<=1.0f));

}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::RayTriangleTest( const Ray3& ray, const CPlane& FacePlane, const CPlane& EdgePlane0, const CPlane& EdgePlane1, const CPlane& EdgePlane2, CVector3& isect )
{
    float fdis = 0.0f;
    bool bisect = FacePlane.Intersect( ray, fdis, isect );
    bisect &= EdgePlane0.GetPointDistance(isect)>=0.0f;
    bisect &= EdgePlane1.GetPointDistance(isect)>=0.0f;
    bisect &= EdgePlane2.GetPointDistance(isect)>=0.0f;
    return bisect;
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::RayTriangleTest( const Ray3& ray, const CPlane& FacePlane, const CPlane& EdgePlane0, const CPlane& EdgePlane1, const CPlane& EdgePlane2, CVector3& isect, float &fdis )
{
    bool bisect = FacePlane.Intersect( ray, fdis, isect );
    bisect &= EdgePlane0.GetPointDistance(isect)>=0.0f;
    bisect &= EdgePlane1.GetPointDistance(isect)>=0.0f;
    bisect &= EdgePlane2.GetPointDistance(isect)>=0.0f;
    return bisect;
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::RayTriangleTest( const Ray3& ray, const CVector3& A, const CVector3& B, const CVector3& C )
{
	float s, t;
	CVector3 isect;
	return RayTriangleTest( ray, A, B, C, isect, s, t );
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::RaySphereTest(const Ray3& ray, const Sphere& sph, float& t)
{
	///////////////////////////////////////////////////////
    float a_coef = ray.mdot_dd;
    float b_coef = 2.0f*ray.mdot_do;
    float c_coef = ray.mdot_oo - (sph.mRadius * sph.mRadius);
	///////////////////////////////////////////////////////
    float discriminant = (b_coef*b_coef) - (4.0f*a_coef*c_coef);
    if (discriminant < 0.0f) return false; 
	///////////////////////////////////////////////////////
	float dist = ork::sqrtf(discriminant);
	float quadratic = (b_coef < 0.0f) ? (-b_coef - dist)*0.5f : (-b_coef + dist)*0.5f;
    float t0 = quadratic / a_coef;
    float t1 = c_coef / quadratic;
    if( t0 > t1 ) std::swap( t0, t1 );
	///////////////////////////////////////////////////////
    if( t1 < 0.0f ) return false; // object in rays negative direction
	///////////////////////////////////////////////////////
	t = ( t0 < 0.0f ) ? t1 : t0;
	///////////////////////////////////////////////////////
	return true;
}

///////////////////////////////////////////////////////////////////////////////
/// abstract collision test via bisection (binary search) method
/// just provide a IAbstractCollidable adapter to your collidable volume
/// this will also return the normalized progression into the movement vector (ve-vs)
/// that the collision occured (in fat)
///
/// basically you want to use this when no easy analytical method for collision detection exists
/// for a primitive (aka heightmap)
///
///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::AbstractCollidableBisectionTest(	IAbstractCollidable& collidable,
														const float fs, const float fe,
														const LineSegment3& seg,
														CVector3& cp, CVector3& vn, float& fat )
{
	fat = -1.0f;

	CVector3 p0, n0;

	IAbstractCollidable::EColFlg ecf = collidable.CollisionTest( seg, p0, n0 );

	switch( ecf )
	{
		case IAbstractCollidable::ECF_ENTERCOLLISION: // entering collision state
		{
			float dist = (seg.mEnd-seg.mStart).MagSquared();
		
			if( dist<0.00001f )
			{
				cp = p0;
				vn = n0;
				fat = (fs+fe)*0.5f;
				return true;
			}

			CVector3 vm = (seg.mStart+seg.mEnd)*0.5f;
			float fm = (fs+fe)*0.5f;
			CVector3 vp0, vn0;
			CVector3 vp1, vn1;
			float fat0, fat1;
			////////////////////////////////////////
			LineSegment3 biseg1( seg.mStart, vm );
			bool bt1 = AbstractCollidableBisectionTest( collidable, fs, fm, biseg1, vp0, vn0, fat0 );
			////////////////////////////////////////
			if( bt1 ) // first half did intersect
			////////////////////////////////////////
			{
				cp = vp0;
				vn = vn0;
				fat = fat0;
				return true;
			}
			////////////////////////////////////////
			else
			////////////////////////////////////////
			{
				LineSegment3 biseg2( vm, seg.mEnd );
				bool bt2 = AbstractCollidableBisectionTest( collidable, fm, fe, biseg2, vp1, vn1, fat1 );
				////////////////////////////////////////
				if( bt2 ) // 2nd half did intersect
				////////////////////////////////////////
				{
					cp = vp1;
					vn = vn1;
					fat = fat1;
					return true;
				}
			}
			////////////////////////////////////////
			break;
		}
		case IAbstractCollidable::ECF_TOTALCOLLISION:  // full collision state
		{
			cp = p0;
			vn = n0;
			fat = 0.0f;
			return true;
			break;
		}
		case IAbstractCollidable::ECF_EXITCOLLISION:  // full collision state
		{
			cp = p0;
			vn = n0;
			fat = 0.0f;
			break;
		}
		case IAbstractCollidable::ECF_NOCOLLISION:  // full collision state
		{
			break;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
