////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _ORK_MATH_COLLISIONTEST_H
#define _ORK_MATH_COLLISIONTEST_H

#include <ork/config/config.h>
#include <ork/math/plane.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

struct Frustum;
class AABox;
struct Sphere;
struct Circle;

class IAbstractCollidable
{
public:
	
	enum EColFlg
	{
		ECF_NOCOLLISION = 0,
		ECF_ENTERCOLLISION,
		ECF_EXITCOLLISION,
		ECF_TOTALCOLLISION
	};

	virtual EColFlg CollisionTest( const LineSegment3& seg, fvec3& cp, fvec3& vn ) = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct  CollisionTester
{
	static bool FrustumSphereTest( const Frustum& frus, const Sphere& sph );
	static bool FrustumCircleXZTest( const Frustum& frus, const Circle& cir );
	static bool FrustumPointTest( const Frustum& frus, const fvec3& pnt );
	static bool Frustu_aaBoxTest( const Frustum& frus, const AABox& aab );
	static bool FrustumFrustumTest( const Frustum& frus1, const Frustum& frus2 );

	static bool SphereSphereTest( const Sphere& sph1, const Sphere& sph2 );
	static bool SphereAABoxTest( const Sphere& sph, const AABox& aab );

	static bool RayTriangleTest( const fray3& ray, const fvec3& v0, const fvec3& v1, const fvec3& v2, fvec3& isect, float& s, float& t );
	static bool RayTriangleTest( const fray3& ray, const fvec3& v0, const fvec3& v1, const fvec3& v2 );
    static bool RayTriangleTest(    const fray3& ray, const fplane3& FacePlane,
                                    const fplane3& EdgePlane0, const fplane3& EdgePlane1, const fplane3& EdgePlane2,
                                    fvec3& isect );
    static bool RayTriangleTest(    const fray3& ray, const fplane3& FacePlane,
                                    const fplane3& EdgePlane0, const fplane3& EdgePlane1, const fplane3& EdgePlane2,
                                    fvec3& isect, float &fdis );
    
	static bool RaySphereTest(const fray3& ray, const Sphere& sph, float& t);

	static bool AbstractCollidableBisectionTest(	IAbstractCollidable& collidable,
													const float fs, const float fe,
													const LineSegment3& seg,
													fvec3& cp, fvec3& vn, float& fat );

};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

#endif
