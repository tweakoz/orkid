////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

bool CollisionTester::FrustumPointTest( const Frustum& frus, const fvec3& pnt )
{
	fvec4 tpos( pnt );
	bool	bv = frus._nearPlane.isPointInFront( tpos );
			bv &= frus._farPlane.isPointInFront( tpos );
			bv &= frus._leftPlane.isPointInFront( tpos );
			bv &= frus._rightPlane.isPointInFront( tpos );
			bv &= frus._topPlane.isPointInFront( tpos );
			bv &= frus._bottomPlane.isPointInFront( tpos );

	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::FrustumSphereTest( const Frustum& frus, const Sphere& sph )
{
	float nrad = -sph.mRadius;
	fvec4 tpos( sph.mCenter );
	
	float nd = frus._nearPlane.pointDistance( tpos );
	float fd = frus._farPlane.pointDistance( tpos );
	float ld = frus._leftPlane.pointDistance( tpos );
	float rd = frus._rightPlane.pointDistance( tpos );
	float td = frus._topPlane.pointDistance( tpos );
	float bd = frus._bottomPlane.pointDistance( tpos );
	
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
	Sphere sph( fvec3( cirXZ.mCenter.x, frus.mCenter.y, cirXZ.mCenter.y ), cirXZ.mRadius );
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

bool CollisionTester::Frustu_aaBoxTest( const Frustum& frus, const AABox& box )
{
	OrkAssertNotImpl();
	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::RayTriangleTest( const fray3& ray, const fvec3& A, const fvec3& B, const fvec3& C, fvec3& isect, float& s, float& t )
{
	const fvec3& O = ray.mOrigin;
	const fvec3& D = ray.mDirection;

	///////////////////////////////////////////////////
	// first calc intersection to plane

	fvec3 vab = (B-A);
	fvec3 vac = (C-A);

	fvec3 Nabc = vab.crossWith(vac);
	float dist_O_to_P = -(O-A).dotWith(Nabc)/(D.dotWith(Nabc));
	isect = O+(D*dist_O_to_P); // plane_isect
		
	if( dist_O_to_P<0.0f ) return false;


	///////////////////////////////////////////////////
	// calc s and t (pseudo-barycentric method)

	//fvec3 vop = (isect-O); // debugging?
	fvec3 voa = (A-O);
	fvec3 vob = (B-O);
	fvec3 voc = (C-O);

	fplane3 PlaneOCA( voc.crossWith(voa).normalized(), O );
	fplane3 PlaneOBA( voa.crossWith(vob).normalized(), O );
		
	t = PlaneOCA.pointDistance( isect )/PlaneOCA.pointDistance(B);
	s = PlaneOBA.pointDistance( isect )/PlaneOBA.pointDistance(C);

	return ((t>=0.0f)&&(t<=1.0f)&&(s>=0.0f)&&(s<=1.0f));

}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::RayTriangleTest( const fray3& ray, const fplane3& FacePlane, const fplane3& EdgePlane0, const fplane3& EdgePlane1, const fplane3& EdgePlane2, fvec3& isect )
{
    float fdis = 0.0f;
    bool bisect = FacePlane.Intersect( ray, fdis, isect );
    bisect &= EdgePlane0.pointDistance(isect)>=0.0f;
    bisect &= EdgePlane1.pointDistance(isect)>=0.0f;
    bisect &= EdgePlane2.pointDistance(isect)>=0.0f;
    return bisect;
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::RayTriangleTest( const fray3& ray, const fplane3& FacePlane, const fplane3& EdgePlane0, const fplane3& EdgePlane1, const fplane3& EdgePlane2, fvec3& isect, float &fdis )
{
    bool bisect = FacePlane.Intersect( ray, fdis, isect );
    bisect &= EdgePlane0.pointDistance(isect)>=0.0f;
    bisect &= EdgePlane1.pointDistance(isect)>=0.0f;
    bisect &= EdgePlane2.pointDistance(isect)>=0.0f;
    return bisect;
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::RayTriangleTest( const fray3& ray, const fvec3& A, const fvec3& B, const fvec3& C )
{
	float s, t;
	fvec3 isect;
	return RayTriangleTest( ray, A, B, C, isect, s, t );
}

///////////////////////////////////////////////////////////////////////////////

bool CollisionTester::RaySphereTest(const fray3& ray, const Sphere& sph, float& t)
{
	///////////////////////////////////////////////////////
    float a_coef = ray.mdot_dd;
    float b_coef = 2.0f*ray.mdot_do;
    float c_coef = ray.mdot_oo - (sph.mRadius * sph.mRadius);
	///////////////////////////////////////////////////////
    float discriminant = (b_coef*b_coef) - (4.0f*a_coef*c_coef);
    if (discriminant < 0.0f) return false; 
	///////////////////////////////////////////////////////
	float dist = sqrtf(discriminant);
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
														const flineseg3& seg,
														fvec3& cp, fvec3& vn, float& fat )
{
	fat = -1.0f;

	fvec3 p0, n0;

	IAbstractCollidable::EColFlg ecf = collidable.CollisionTest( seg, p0, n0 );

	switch( ecf )
	{
		case IAbstractCollidable::ECF_ENTERCOLLISION: // entering collision state
		{
			float dist = (seg.mEnd-seg.mStart).magnitudeSquared();
		
			if( dist<0.00001f )
			{
				cp = p0;
				vn = n0;
				fat = (fs+fe)*0.5f;
				return true;
			}

			fvec3 vm = (seg.mStart+seg.mEnd)*0.5f;
			float fm = (fs+fe)*0.5f;
			fvec3 vp0, vn0;
			fvec3 vp1, vn1;
			float fat0, fat1;
			////////////////////////////////////////
			flineseg3 biseg1( seg.mStart, vm );
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
				flineseg3 biseg2( vm, seg.mEnd );
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
