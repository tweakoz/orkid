////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _ORK_MATH_BOX_H
#define _ORK_MATH_BOX_H

#include <ork/math/plane.h>
#include <ork/object/Object.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class AABox : public ork::Object
{
	RttiDeclareConcrete(AABox,ork::Object);

	CVector3	mMin;
	CVector3	mMax;

	CPlane		mPlaneNX[2];
	CPlane		mPlaneNY[2];
	CPlane		mPlaneNZ[2];

	void ComputePlanes();

public:

    void SupportMapping( const CVector3& v, CVector3& result) const;

	CVector3 Corner(int n) const;

	bool Intersect( const Ray3& ray, CVector3& isect_in, CVector3& isect_out ) const;
	bool Contains(const CVector3& test_point) const;
	bool Contains(const float test_point_X, const float test_point_Z) const;
    void Constrain(float &test_point_X, float &test_point_Z) const;
    void Constrain(CVector3& test_point) const;

    AABox();
    AABox( const CVector3& vmin, const CVector3& vmax );
    AABox( const AABox& oth );
    void operator=(const AABox& oth );
    
	inline const CVector3& Min() const { return mMin; }
	inline const CVector3& Max() const { return mMax; }
    inline CVector3 GetSize() const { return Max()-Min(); }

	void BeginGrow();
	void Grow( const CVector3& vin );
	void EndGrow();

	void SetMinMax( const CVector3& vmin, const CVector3& vmax );

private:
	virtual bool PostDeserialize(reflect::IDeserializer &);

};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

#endif
