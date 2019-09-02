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
#include <ork/reflect/Serialize.h>
#include <ork/reflect/BidirectionalSerializer.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork
{

template<> float Vector2<float>::Sin( float fin )
{
	return CFloat::Sin( fin );
}
template<> float Vector2<float>::Cos( float fin )
{
	return CFloat::Cos( fin );
}
template<> float Vector2<float>::Sqrt( float fin )
{
	return CFloat::Sqrt( fin );
}
template<> float Vector2<float>::Epsilon()
{
	return CFloat::Epsilon();
}
template<> float Vector2<float>::Abs( float fin )
{
	return CFloat::Abs( fin );
}

///////////////////////////////////////////////////////////////////////////////

template<> double Vector2<double>::Sin( double fin )
{
	return std::sin(fin);
}
template<> double Vector2<double>::Cos( double fin )
{
	return std::cos(fin);
}
template<> double Vector2<double>::Sqrt( double fin )
{
	return std::sqrt( fin );
}
template<> double Vector2<double>::Epsilon()
{
	return double(CFloat::Epsilon());
}
template<> double Vector2<double>::Abs( double fin )
{
	return std::abs(fin);
}

///////////////////////////////////////////////////////////////////////////////

template<> const EPropType CPropType<fvec2>::meType				= EPROPTYPE_VEC2REAL;
template<> const char* CPropType<fvec2>::mstrTypeName					= "VEC2REAL";
template<> void CPropType<fvec2>::ToString( const fvec2 & Value, PropTypeString& tstr )
{
	fvec2 v = Value;
	tstr.format( "%g %g", float(v.GetX()), float(v.GetY()));
}

template<> fvec2 CPropType<fvec2>::FromString(const PropTypeString& String)
{
	float x, y;
	sscanf(String.c_str(), "%g %g", &x, &y);
	return fvec2(float(x), float(y));
}

///////////////////////////////////////////////////////////////////////////////

template class Vector2<float>;			// explicit template instantiation
template class Vector2<double>;		// explicit template instantiation
template class CPropType<fvec2>;

///////////////////////////////////////////////////////////////////////////////
namespace reflect {
template<> void Serialize( const fvec2*in, fvec2*out, reflect::BidirectionalSerializer& bidi )
{
	if( bidi.Serializing() )
	{
		bidi.Serializer()->Hint("fvec2");
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
