////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
#include <ork/pch.h>

#include <ork/math/box.h>
#include <ork/math/sphere.h>
#include <ork/reflect/properties/register.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

Sphere::Sphere( const fvec3& boxmin, const fvec3& boxmax )
{
	mCenter = (boxmin+boxmax)*0.5f;
	mRadius = (boxmax-mCenter).Mag();
}

AABox::AABox()
{
}
AABox::AABox( const fvec3& vmin, const fvec3& vmax )
    : mMin( vmin )
    , mMax( vmax )
{
    ComputePlanes();
}
AABox::AABox( const AABox& oth )
    : mMin( oth.mMin )
    , mMax( oth.mMax )
{
    ComputePlanes();
}
void AABox::operator=(const AABox& oth )
{
    mMin = oth.mMin;
    mMax = oth.mMax;
    ComputePlanes();
}
void AABox::SetMinMax( const fvec3& vmin, const fvec3& vmax )
{
    mMin=vmin;
    mMax=vmax;
    ComputePlanes();
}
bool AABox::Intersect( const fray3& ray, fvec3& isect_in, fvec3& isect_out ) const
{
    float tmin, tmax, tymin, tymax, tzmin, tzmax;

    const fvec3& bndx = ray.mbSignX ? mMin : mMax;
    const fvec3& bndix = ray.mbSignX ? mMax : mMin;
    const fvec3& bndy = ray.mbSignY ? mMin : mMax;
    const fvec3& bndiy = ray.mbSignY ? mMax : mMin;
    const fvec3& bndz = ray.mbSignZ ? mMin : mMax;
    const fvec3& bndiz = ray.mbSignZ ? mMax : mMin;

    const fvec3& ori = ray.mOrigin;

    tmin = (bndx.x - ori.x) * ray.mInverseDirection.x;
    tmax = (bndix.x - ori.x) * ray.mInverseDirection.x;
    tymin = (bndy.y - ori.y) * ray.mInverseDirection.y;
    tymax = (bndiy.y - ori.y) * ray.mInverseDirection.y;

    if ( (tmin > tymax) || (tymin > tmax) )
        return false;

    if (tymin > tmin)
        tmin = tymin;
    if (tymax < tmax)
        tmax = tymax;

    tzmin = (bndz.z - ori.z) * ray.mInverseDirection.z;
    tzmax = (bndiz.z - ori.z) * ray.mInverseDirection.z;

    if ( (tmin > tzmax) || (tzmin > tmax) )
        return false;

    if (tzmin > tmin)
        tmin = tzmin;
    if (tzmax < tmax)
        tmax = tzmax;

    isect_in = ori + ray.mDirection*tmin;
    isect_out = ori + ray.mDirection*tmax;

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void AABox::SupportMapping( const fvec3& v, fvec3& result ) const
{
    //Vector3.Dot(ref this.corners[0], ref v, out num3);
    float num3 = v.Dot( Corner(0) );

    int index = 0;
    for (int i = 1; i < 8; i++)
    {
        float num2 = v.Dot( Corner(i) );

        if (num2 > num3)
        {
            index = i;
            num3 = num2;
        }
    }
    result = Corner(index);
}

///////////////////////////////////////////////////////////////////////////////

void AABox::BeginGrow()
{
    float fmin = std::numeric_limits<float>::max();
    float fmax = -std::numeric_limits<float>::max();
    mMin = fvec3( fmin,fmin,fmin );
    mMax = fvec3( fmax,fmax,fmax );
}

void AABox::Grow( const fvec3& vin )
{
    mMin.setX( std::min( mMin.x, vin.x ) );
    mMin.setY( std::min( mMin.y, vin.y ) );
    mMin.setZ( std::min( mMin.z, vin.z ) );

    mMax.setX( std::max( mMax.x, vin.x ) );
    mMax.setY( std::max( mMax.y, vin.y ) );
    mMax.setZ( std::max( mMax.z, vin.z ) );

}
void AABox::EndGrow()
{
    ComputePlanes();
}

///////////////////////////////////////////////////////////////////////////////

void AABox::ComputePlanes()
{
    fvec3 nX( 1.0f, 0.0f, 0.0f );
    fvec3 nY( 0.0f, 1.0f, 0.0f );
    fvec3 nZ( 0.0f, 1.0f, 0.0f );

    mPlaneNX[0] = fplane3( nX, mMin.x );
    mPlaneNX[1] = fplane3( -nX, mMax.x );

    mPlaneNY[0] = fplane3( nY, mMin.y );
    mPlaneNY[1] = fplane3( -nY, mMax.y );

    mPlaneNZ[0] = fplane3( nZ, mMin.z );
    mPlaneNZ[1] = fplane3( -nZ, mMax.z );
}

bool AABox::contains(const float test_point_X, const float test_point_Z) const
 {
    if (test_point_X > mMax.x)
        return false;

    if (test_point_X < mMin.x)
        return false;

    if (test_point_Z > mMax.z)
        return false;

    if (test_point_Z < mMin.z)
        return false;

    return true;
 }

 void AABox::Constrain(float &test_point_X, float &test_point_Z) const
 {
    if (test_point_X > mMax.x)
        test_point_X = mMax.x;
    else if (test_point_X < mMin.x)
        test_point_X = mMin.x;

    if (test_point_Z > mMax.z)
        test_point_Z = mMax.z;
    else if (test_point_Z < mMin.z)
        test_point_Z = mMin.z;

 }

 ///////////////////////////////////////////////////////////////////////////////

 bool AABox::contains(const fvec3& test_point) const
 {
    if (test_point.x > mMax.x)
        return false;

    if (test_point.x < mMin.x)
        return false;

    if (test_point.z > mMax.z)
        return false;

    if (test_point.z < mMin.z)
        return false;

    if (test_point.y > mMax.y)
        return false;

    if (test_point.y < mMin.y)
        return false;


    return true;
 }

void AABox::Constrain(fvec3& test_point) const
{
    if (test_point.x > mMax.x)
        test_point.setX(mMax.x);
    else if (test_point.x < mMin.x)
        test_point.setX(mMin.x);

    if (test_point.z > mMax.z)
        test_point.setZ(mMax.z);
    else if (test_point.z < mMin.z)
        test_point.setZ(mMin.z);

    if (test_point.y > mMax.y)
        test_point.setY(mMax.y);
    else if (test_point.y < mMin.y)
        test_point.setY(mMin.y);


}

///////////////////////////////////////////////////////////////////////////////

fvec3 AABox::Corner( int idx ) const
{
    fvec3 rval;
    rval.setX( ((idx & 1) == 1) ? mMax.x : mMin.x );
    rval.setY( ((idx & 2) == 1) ? mMax.y : mMin.y );
    rval.setZ( ((idx & 4) == 1) ? mMax.z : mMin.z );
    return rval;
}

}
///////////////////////////////////////////////////////////////////////////////
