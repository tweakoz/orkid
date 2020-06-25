////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/pch.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/gradient.h>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/register.h>
#include <ork/reflect/enum_serializer.inl>
#include <ork/reflect/properties/DirectTyped.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <math.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork
{

void GradientBase::Describe()
{
}

GradientBase::GradientBase()
{
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Gradient<T>::Gradient()
{
	mData.AddSorted( 0.0f, T() );
	mData.AddSorted( 1.0f, T() );
}

template <typename T> bool Gradient<T>::PreDeserialize( ork::reflect::IDeserializer& deser )
{	mData.clear();
	return true;
}

template <typename T> void Gradient<T>::AddDataPoint( float flerp, const T& data )
{
	mData.AddSorted(flerp,data);
}

template <typename T> T Gradient<T>::Sample( float fu )
{
	if( fu < 0.0f ) return T();
	if( fu > 1.0f ) return T();
	if( isnan(fu) ) return T();

	bool bdone = false;
	int isega = 0;
	int isegb = 0;
	int inumv = int(mData.size());
	while( false == bdone )
	{	isegb = (isega+1);
		OrkAssert( isegb<inumv );
		if(		(fu >= mData.GetItemAtIndex(isega).first)
			&&	(fu <= mData.GetItemAtIndex(isegb).first) )
		{	bdone = true;
		}
		else
		{	isega++;
		}
	}
	const std::pair<float,T>& VA = mData.GetItemAtIndex(isega);
	const std::pair<float,T>& VB = mData.GetItemAtIndex(isegb);
	float dU = VB.first-VA.first;
	float Base = VA.first;
	float fsu = (fu-Base)/dU;
	float fisu = 1.0f - fsu;
	T rval = (VA.second*fisu)+(VB.second*fsu);
	return rval;
}

template <typename T> void Gradient<T>::Describe()
{
	ork::reflect::RegisterMapProperty( "points", & Gradient<T>::mData );
	ork::reflect::annotatePropertyForEditor<Gradient>( "points", "editor.visible", "false" );
	ork::reflect::annotateClassForEditor< Gradient >( "editor.class", ConstString("ged.factory.gradient") );

}

///////////////////////////////////////////////////////////////////////////////

template <typename T> GradientD2<T>::GradientD2()
{
	Gradient<T>* g0 = new Gradient<T>;
	Gradient<T>* g1 = new Gradient<T>;
	mData.AddSorted( 0.0f, g0 );
	mData.AddSorted( 1.0f, g1 );
}

template <typename T> GradientD2<T>::~GradientD2()
{
	for( size_t i=0; i<mData.size(); i++ )
	{
		ork::Object* pobj = mData.GetItemAtIndex(i).second;
		delete pobj;
	}
}

template <typename T> bool GradientD2<T>::PreDeserialize( ork::reflect::IDeserializer& deser )
{	mData.clear();
	return true;
}

template <typename T> void GradientD2<T>::Describe()
{
	//ork::reflect::RegisterMapProperty( "points", & GradientD2<T>::mData );
	//ork::reflect::annotatePropertyForEditor<GradientD2>( "points", "editor.visible", "false" );
}

///////////////////////////////////////////////////////////////////////////////

}

typedef ork::Gradient<ork::fvec4> GradientV4;
typedef ork::GradientD2<ork::fvec4> GradientD2V4;
INSTANTIATE_TRANSPARENT_RTTI(ork::GradientBase,"GradientBase");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(GradientV4,"GradientV4");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(GradientD2V4,"GradientD2V4");

template class ork::orklut<float,ork::fvec4>;
//template class ork::orklut<float, ork::orklut<float,ork::fvec4> >;
template class ork::GradLut<ork::fvec4>;
//template class ork::GradLut< ork::GradLut<ork::fvec4> >; //ork::orklut<float,ork::fvec4>;
template class ork::Gradient<ork::fvec4>;
template class ork::GradientD2<ork::fvec4>;
