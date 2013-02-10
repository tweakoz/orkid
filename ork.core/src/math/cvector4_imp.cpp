////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/math/cvector4.h>
#include <ork/math/cvector4.hpp>
#include <ork/math/matrix_inverseGEMS.hpp>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/reflect/Serialize.h>
#include <ork/reflect/BidirectionalSerializer.h>

namespace ork
{
///////////////////////////////////////////////////////////////////////////////

template<> float TVector4<float>::Sin( float fin )
{
	return CFloat::Sin( fin );
}
template<> float TVector4<float>::Cos( float fin )
{
	return CFloat::Cos( fin );
}
template<> float TVector4<float>::Sqrt( float fin )
{
	return CFloat::Sqrt( fin );
}
template<> float TVector4<float>::Epsilon()
{
	return CFloat::Epsilon();
}
template<> float TVector4<float>::Abs( float fin )
{
	return CFloat::Abs( fin );
}

///////////////////////////////////////////////////////////////////////////////

template<> double TVector4<double>::Sin( double fin )
{
	return std::sin(fin);
}
template<> double TVector4<double>::Cos( double fin )
{
	return std::cos(fin);
}
template<> double TVector4<double>::Sqrt( double fin )
{
	return std::sqrt( fin );
}
template<> double TVector4<double>::Epsilon()
{
	return double(CFloat::Epsilon());
}
template<> double TVector4<double>::Abs( double fin )
{
	return std::abs(fin);
}

///////////////////////////////////////////////////////////////////////////////

template<> const EPropType CPropType<CVector4>::meType				= EPROPTYPE_VEC4REAL;
template<> const char* CPropType<CVector4>::mstrTypeName					= "VEC4REAL";
template<> void CPropType<CVector4>::ToString(const CVector4 & Value, PropTypeString& tstr)
{
	CVector4 v = Value;
	tstr.format( "%g %g %g %g", float(v.GetX()), float(v.GetY()), float(v.GetZ()), float(v.GetW()));
}

template<> CVector4 CPropType<CVector4>::FromString(const PropTypeString& String)
{
	float x, y, z, w;
	sscanf(String.c_str(), "%g %g %g %g", &x, &y, &z, &w);
	return CVector4(CReal(x), CReal(y), CReal(z), CReal(w));
}

///////////////////////////////////////////////////////////////////////////////

template class TVector4<float>;		// explicit template instantiation
template class TVector4<double>;		// explicit template instantiation
template class CPropType<CVector4>;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace reflect {

template<> void Serialize( const CVector4*in, CVector4*out, reflect::BidirectionalSerializer& bidi )
{
	if( bidi.Serializing() )
	{
		bidi.Serializer()->Hint("CVector4");
		for( int i=0; i<4; i++ )
		{
			bidi | in->GetArray()[i];
		}
	}
	else
	{
		for( int i=0; i<4; i++ )
		{
			bidi | out->GetArray()[i];
		}
	}
}

template<> void Serialize( const orkmap<float,CVector4>*in,  orkmap<float,CVector4>*out, reflect::BidirectionalSerializer& bidi )
{
	if( bidi.Serializing() )
	{
	}
	else
	{
	}
}
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}
