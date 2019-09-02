////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector2.hpp>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/math/tnumber.h>
#include <ork/reflect/Serialize.h>
#include <ork/reflect/BidirectionalSerializer.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork
{

template<> float TVector2<float>::Sin( float fin )
{
	return CFloat::Sin( fin );
}
template<> float TVector2<float>::Cos( float fin )
{
	return CFloat::Cos( fin );
}
template<> float TVector2<float>::Sqrt( float fin )
{
	return CFloat::Sqrt( fin );
}
template<> float TVector2<float>::Epsilon()
{
	return CFloat::Epsilon();
}
template<> float TVector2<float>::Abs( float fin )
{
	return CFloat::Abs( fin );
}

///////////////////////////////////////////////////////////////////////////////

template<> double TVector2<double>::Sin( double fin )
{
	return std::sin(fin);
}
template<> double TVector2<double>::Cos( double fin )
{
	return std::cos(fin);
}
template<> double TVector2<double>::Sqrt( double fin )
{
	return std::sqrt( fin );
}
template<> double TVector2<double>::Epsilon()
{
	return double(CFloat::Epsilon());
}
template<> double TVector2<double>::Abs( double fin )
{
	return std::abs(fin);
}

///////////////////////////////////////////////////////////////////////////////

template<> const EPropType CPropType<CVector2>::meType				= EPROPTYPE_VEC2REAL;
template<> const char* CPropType<CVector2>::mstrTypeName					= "VEC2REAL";
template<> void CPropType<CVector2>::ToString( const CVector2 & Value, PropTypeString& tstr )
{
	CVector2 v = Value;
	tstr.format( "%g %g", float(v.GetX()), float(v.GetY()));
}

template<> CVector2 CPropType<CVector2>::FromString(const PropTypeString& String)
{
	float x, y;
	sscanf(String.c_str(), "%g %g", &x, &y);
	return CVector2(float(x), float(y));
}

///////////////////////////////////////////////////////////////////////////////

template class TVector2<float>;			// explicit template instantiation
template class TVector2<double>;		// explicit template instantiation
template class CPropType<CVector2>;

///////////////////////////////////////////////////////////////////////////////
namespace reflect {
template<> void Serialize( const CVector2*in, CVector2*out, reflect::BidirectionalSerializer& bidi )
{
	if( bidi.Serializing() )
	{
		bidi.Serializer()->Hint("CVector2");
		for( int i=0; i<2; i++ )
		{
			bidi | in->GetArray()[i];
		}
	}
	else
	{
		for( int i=0; i<2; i++ )
		{
			bidi | out->GetArray()[i];
		}
	}
}
}

}
