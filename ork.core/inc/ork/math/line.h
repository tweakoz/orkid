////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _ORK_MATH_LINE_H
#define _ORK_MATH_LINE_H

#include <ork/kernel/any.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T>
class InfiniteLine2D
{
    typedef TVector2<T> vec2_type;
protected:
    vec2_type   mNormal;    // A, B
    T           mfC;        // C
    // Ax+By+c==0
public:
    InfiniteLine2D( ) : mNormal(T(0.0f),T(1.0f)), mfC(T(0.0f)) {}

    inline  void CalcFromNormalAndOrigin( const vec2_type& Normal, const vec2_type& PosVec )
    {
        // mFC = C
        mNormal = Normal;
        mfC = T(0.0f);
        mfC = GetPointDistance( PosVec ) * T(-1.0f);

    }
    inline  void CalcFromTwoPoints( const vec2_type& Pnt0, const vec2_type& Pnt1 )
    {
        T dX = (Pnt1.GetX()-Pnt0.GetX());
        if( dX==T(0.0f) )
        {
            if( Pnt1.GetY()<Pnt0.GetY() )           mNormal.Set( 1.0f, 0.0f );
            else if( Pnt1.GetY()>Pnt0.GetY() )      mNormal.Set( -1.0f, 0.0f );
            else {
                                                    OrkAssert(false); // pnt1 must != pnt0
            }
        }
        else
        {   // b = y-mx
            // a = m*b
            // c = -ax-by
            T dY = (Pnt1.GetY()-Pnt0.GetY());
            T m = dY/dX;
            T b = (Pnt0.GetY()-m*Pnt0.GetX());
            T a = m*b;
            mNormal.Set( a, b );
            mNormal.Normalize();
            T c = -a * Pnt0.GetX() -b * Pnt0.GetY();
            mfC = c;
        }
    }
    inline  void CalcFromTwoPoints( float x0, float y0, float x1, float y1 )
    {
        T dX = (x1-x0);
        if( dX==T(0.0f) )
        {
            if( y1<y0 )             mNormal.Set( 1.0f, 0.0f );
            else if( y1>y0 )        mNormal.Set( -1.0f, 0.0f );
            else {
                                    OrkAssert(false); // pnt1 must != pnt0
            }
        }
        else
        {   // b = y-mx
            // a = m*b
            // c = -ax-by

            // m = -A/B
            // 1/m = B/-A
            // B = -A/m
            // b = -C/B
            // 1/b = B/-C
            // B = -C/b

            // B = -A/m
            // B = -C/b
            // -A/m = -C/b
            // m/-A = b/-C
            // m = -Ab/-C
            // m = Ab/C
            // Ab/C = -A/B
            // Ab = -AC/B
            // b = -C/B
            // bB = -C
            // C = -bB
            // C = bA/m
            // Ax-Ay/m+bA/m = 0
            // Ax=Ay/m-bA/m
            // Axm=Ay-Ab
            // Axm+Ab-Ay=0
            // Axm+Ab-Ay+1=1

            T dY = (y1-y0);
            T m = dY/dX;
            T b = (y0-m*x0);
            T a = m*b;
            mNormal.Set( a, b );
            mNormal.Normalize();
            T c = -a * x0 -b * y0;
            T fZ = a*x0+b*y0+c;
            mfC = -c;
        }
    }
    float GetPointDistance( const vec2_type& point ) const
    {
        return mNormal.Dot(point) + mfC;
    }
     float GetPointDistance( float fx, float fy ) const
    {
        return (mNormal.GetX()*fx)+(mNormal.GetY()*fy) + mfC;
    }
    bool IsPointInFront( const vec2_type& point ) const
    {
        T distance = GetPointDistance(point);
        return (distance >= T(0.0f));
    }
    bool IsPointBehind( const vec2_type& point ) const
    {
        return (!IsPointInFront(point));
    }
    bool IsPointInFront( float fx, float fy ) const
    {
        T distance = GetPointDistance(fx,fy);
        return (distance >= T(0.0f));
    }
    bool IsPointBehind( float fx, float fy ) const
    {
        return (!IsPointInFront(fx,fy));
    }
};

template <typename T>
struct TLineSegment3
{
	typedef TVector3<T> vec3_type;

	vec3_type	mStart;
	vec3_type	mEnd;

	TLineSegment3( const vec3_type& s, const vec3_type& e ) : mStart(s), mEnd(e) {}
};

///////////////////////////////////////////////////////////////////////////////

template <typename T>
class TLineSegment2
{
	typedef TVector2<T> vec2_type;
protected:
	vec2_type	mStart;
	vec2_type	mEnd;


	TLineSegment2( const vec2_type& s, const vec2_type& e ) : mStart(s), mEnd(e) {}
	TLineSegment2() {};

};

///////////////////////////////////////////////////////////////////////////////

template <typename T>
class TLineSegment2Helper
{
	typedef TVector2<T> vec2_type;

	vec2_type	mStart;
	vec2_type	mEnd;
	vec2_type	mOrigin;  
	T			mMag;
public:
	float GetPointDistanceSquared( const vec2_type  &pt ) const;
	float GetPointDistancePercent( const vec2_type  &pt ) const;
	T GetMag() const { return(mMag);}
	TLineSegment2Helper( const vec2_type& s, const vec2_type& e );
	TLineSegment2Helper();
	void SetStartEnd( const vec2_type& s, const vec2_type& e );
	T GetStartX() const { return(mStart.GetX());}

};

///////////////////////////////////////////////////////////////////////////////

template <typename T>
struct TRay3
{
    typedef TVector3<T> vec3_type;

    vec3_type   mOrigin;
    vec3_type   mDirection;
    vec3_type   mInverseDirection;
    bool        mbSignX;
    bool        mbSignY;
    bool        mbSignZ;
    T           mdot_dd;
    T           mdot_do;
    T           mdot_oo;
    int         mID;

    TRay3() : mID(-1) {}
    TRay3( const vec3_type& o, const vec3_type& d )
        : mOrigin(o)
        , mDirection(d)
        , mInverseDirection(1.0f/d.GetX(), 1.0f/d.GetY(), 1.0f/d.GetZ())
        , mID(-1)
    {
        mdot_dd = mDirection.Dot(mDirection);
        mdot_do = mDirection.Dot(mOrigin);
        mdot_oo = mOrigin.Dot(mOrigin);
        mbSignX = (mInverseDirection.GetX()>=0.0f);
        mbSignY = (mInverseDirection.GetY()>=0.0f);
        mbSignZ = (mInverseDirection.GetZ()>=0.0f);

    }
    void SetID( int id ) { mID = id; }
    int GetID() const { return mID; }

    void Lerp( const TRay3& a, const TRay3& b, float fi )
    {
        vec3_type o, d;
        o.Lerp( a.mOrigin, b.mOrigin, fi );
        d.Lerp( a.mDirection, b.mDirection, fi );
        d.Normalize();
        *this = TRay3( o, d );
    }
    void BiLerp( const TRay3& x0y0, const TRay3&  x1y0, const TRay3& x0y1, const TRay3&  x1y1, float fx, float fy )
    {
        TRay3 t; t.Lerp( x0y0, x1y0, fx );
        TRay3 b; b.Lerp( x0y1, x1y1, fx );
        Lerp( t, b, fy );
    }
};

typedef TRay3<float> fray3;
typedef TRay3<double> dray3;

///////////////////////////////////////////////////////////////////////////////
// temporary till all code done being refactored

typedef TLineSegment2<float> LineSegment2;
typedef TLineSegment3<float> LineSegment3;
typedef TLineSegment2Helper<float> LineSegment2Helper;
typedef TRay3<float> Ray3;

struct Ray3HitTest
{
    int miSphTests;
    int miSphTestsPassed;
    int miTriTests;
    int miTriTestsPassed;
    Ray3HitTest() : miSphTests(0), miTriTests(0), miSphTestsPassed(0), miTriTestsPassed(0) {}
    void OnHit( const any32& userdata, const Ray3& r ){ DoOnHit(userdata,r);    }
    virtual void DoOnHit( const any32& userdata, const Ray3& r ){}
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
#endif
