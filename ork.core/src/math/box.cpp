////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


///////////////////////////////////////////////////////////////////////////////
#include <ork/pch.h>

#include <ork/math/box.h>
#include <ork/math/sphere.h>
#include <ork/reflect/RegisterProperty.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::AABox,"AABox");

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

Sphere::Sphere( const CVector3& boxmin, const CVector3& boxmax )
{
	mCenter = (boxmin+boxmax)*0.5f;
	mRadius = (boxmax-mCenter).Mag();
}

void ork::AABox::Describe()
{
	reflect::RegisterProperty("Min", &AABox::mMin);
	reflect::RegisterProperty("Max", &AABox::mMax);

}

AABox::AABox()
{
}
AABox::AABox( const CVector3& vmin, const CVector3& vmax )
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
void AABox::SetMinMax( const CVector3& vmin, const CVector3& vmax )
{
    mMin=vmin;
    mMax=vmax;
    ComputePlanes();
}
bool AABox::Intersect( const Ray3& ray, CVector3& isect_in, CVector3& isect_out ) const
{
    float tmin, tmax, tymin, tymax, tzmin, tzmax;

    const CVector3& bndx = ray.mbSignX ? mMin : mMax;
    const CVector3& bndix = ray.mbSignX ? mMax : mMin;
    const CVector3& bndy = ray.mbSignY ? mMin : mMax;
    const CVector3& bndiy = ray.mbSignY ? mMax : mMin;
    const CVector3& bndz = ray.mbSignZ ? mMin : mMax;
    const CVector3& bndiz = ray.mbSignZ ? mMax : mMin;

    const CVector3& ori = ray.mOrigin;

    tmin = (bndx.GetX() - ori.GetX()) * ray.mInverseDirection.GetX();
    tmax = (bndix.GetX() - ori.GetX()) * ray.mInverseDirection.GetX();
    tymin = (bndy.GetY() - ori.GetY()) * ray.mInverseDirection.GetY();
    tymax = (bndiy.GetY() - ori.GetY()) * ray.mInverseDirection.GetY();
   
    if ( (tmin > tymax) || (tymin > tmax) )
        return false;
   
    if (tymin > tmin)
        tmin = tymin;
    if (tymax < tmax)
        tmax = tymax;
   
    tzmin = (bndz.GetZ() - ori.GetZ()) * ray.mInverseDirection.GetZ();
    tzmax = (bndiz.GetZ() - ori.GetZ()) * ray.mInverseDirection.GetZ();
   
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

void AABox::SupportMapping( const CVector3& v, CVector3& result ) const
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
    float fmin = CFloat::TypeMax();
    float fmax = -CFloat::TypeMax();
    mMin = CVector3( fmin,fmin,fmin );
    mMax = CVector3( fmax,fmax,fmax );
}

void AABox::Grow( const CVector3& vin )
{
    mMin.SetX( CFloat::Min( mMin.GetX(), vin.GetX() ) );
    mMin.SetY( CFloat::Min( mMin.GetY(), vin.GetY() ) );
    mMin.SetZ( CFloat::Min( mMin.GetZ(), vin.GetZ() ) );

    mMax.SetX( CFloat::Max( mMax.GetX(), vin.GetX() ) );
    mMax.SetY( CFloat::Max( mMax.GetY(), vin.GetY() ) );
    mMax.SetZ( CFloat::Max( mMax.GetZ(), vin.GetZ() ) );

}
void AABox::EndGrow()
{
    ComputePlanes();
}

///////////////////////////////////////////////////////////////////////////////

void AABox::ComputePlanes()
{
    CVector3 nX( 1.0f, 0.0f, 0.0f );
    CVector3 nY( 0.0f, 1.0f, 0.0f );
    CVector3 nZ( 0.0f, 1.0f, 0.0f );

    mPlaneNX[0] = CPlane( nX, mMin.GetX() );
    mPlaneNX[1] = CPlane( -nX, mMax.GetX() );

    mPlaneNY[0] = CPlane( nY, mMin.GetY() );
    mPlaneNY[1] = CPlane( -nY, mMax.GetY() );

    mPlaneNZ[0] = CPlane( nZ, mMin.GetZ() );
    mPlaneNZ[1] = CPlane( -nZ, mMax.GetZ() );
}

bool AABox::Contains(const float test_point_X, const float test_point_Z) const
 {
    if (test_point_X > mMax.GetX())
        return false;

    if (test_point_X < mMin.GetX())
        return false;

    if (test_point_Z > mMax.GetZ())
        return false;

    if (test_point_Z < mMin.GetZ())
        return false;

    return true;
 }

 void AABox::Constrain(float &test_point_X, float &test_point_Z) const
 {
    if (test_point_X > mMax.GetX())
        test_point_X = mMax.GetX();
    else if (test_point_X < mMin.GetX())
        test_point_X = mMin.GetX();

    if (test_point_Z > mMax.GetZ())
        test_point_Z = mMax.GetZ();
    else if (test_point_Z < mMin.GetZ())
        test_point_Z = mMin.GetZ();

 }
 
 ///////////////////////////////////////////////////////////////////////////////

 bool AABox::Contains(const CVector3& test_point) const
 {
    if (test_point.GetX() > mMax.GetX())
        return false;

    if (test_point.GetX() < mMin.GetX())
        return false;

    if (test_point.GetZ() > mMax.GetZ())
        return false;

    if (test_point.GetZ() < mMin.GetZ())
        return false;

    if (test_point.GetY() > mMax.GetY())
        return false;

    if (test_point.GetY() < mMin.GetY())
        return false;


    return true;
 }
 
void AABox::Constrain(CVector3& test_point) const
{
    if (test_point.GetX() > mMax.GetX())
        test_point.SetX(mMax.GetX());
    else if (test_point.GetX() < mMin.GetX())
        test_point.SetX(mMin.GetX());

    if (test_point.GetZ() > mMax.GetZ())
        test_point.SetZ(mMax.GetZ());
    else if (test_point.GetZ() < mMin.GetZ())
        test_point.SetZ(mMin.GetZ());

    if (test_point.GetY() > mMax.GetY())
        test_point.SetY(mMax.GetY());
    else if (test_point.GetY() < mMin.GetY())
        test_point.SetY(mMin.GetY());


}

///////////////////////////////////////////////////////////////////////////////

CVector3 AABox::Corner( int idx ) const
{
    CVector3 rval;
    rval.SetX( ((idx & 1) == 1) ? mMax.GetX() : mMin.GetX() );
    rval.SetY( ((idx & 2) == 1) ? mMax.GetY() : mMin.GetY() );
    rval.SetZ( ((idx & 4) == 1) ? mMax.GetZ() : mMin.GetZ() );
    return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool AABox::PostDeserialize(reflect::IDeserializer &)
{
	ComputePlanes();
    return(true);
}



}
///////////////////////////////////////////////////////////////////////////////
