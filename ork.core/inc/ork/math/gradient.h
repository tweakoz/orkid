////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _ORK_MATH_GRADIENT_H
#define _ORK_MATH_GRADIENT_H

///////////////////////////////////////////////////////////////////////////////

#include <ork/config/config.h>
#include <ork/object/Object.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork{
///////////////////////////////////////////////////////////////////////////////

namespace reflect { class IDeserializer; }

///////////////////////////////////////////////////////////////////////////////

class  GradientBase : public ork::Object
{
	RttiDeclareConcrete(GradientBase, ork::Object);

public: 

	GradientBase();

};

///////////////////////////////////////////////////////////////////////////////

template <typename T> struct GradLut : public orklut<float,T>
{
	
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class  Gradient : public GradientBase
{
	DECLARE_TRANSPARENT_TEMPLATE_RTTI(Gradient<T>, GradientBase);

	GradLut<T> mData;

	bool PreDeserialize( ork::reflect::IDeserializer& deser );

public: 

	const orklut<float,T>& Data() const { return mData; }
	orklut<float,T>& Data() { return mData; }

	Gradient();

	void AddDataPoint( float flerp, const T& data );
	T Sample( float atlerp );

};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class  GradientD2 : public GradientBase
{
	DECLARE_TRANSPARENT_TEMPLATE_RTTI(GradientD2<T>, GradientBase);

	GradLut< ork::Object* > mData;

	bool PreDeserialize( ork::reflect::IDeserializer& deser );

public: 

	//const orklut<float,T>& Data() const { return mData; }
	//orklut<float,T>& Data() { return mData; }

	GradientD2();
	~GradientD2();
	
	//void AddDataPoint( float flerpX, const T& data );
	//T Sample( float atlerp );

};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

#endif
