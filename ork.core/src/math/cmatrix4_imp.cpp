////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/math/cmatrix4.h>
#include <ork/math/cmatrix4.hpp>
#include <ork/math/matrix_inverseGEMS.hpp>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/reflect/Serialize.h>
#include <ork/reflect/BidirectionalSerializer.h>
#include <ork/kernel/string/string.h>

namespace ork
{
template<> const EPropType CPropType<CMatrix4>::meType				= EPROPTYPE_MAT44REAL;
template<> const char* CPropType<CMatrix4>::mstrTypeName					= "MAT44REAL";
template<> void CPropType<CMatrix4>::ToString( const CMatrix4 & Value, PropTypeString& tstr)
{
	const CMatrix4 & v = Value;

	std::string result;
	for(int i = 0; i < 15; i++)
		result += CreateFormattedString("%g ", F32(v.elements[i/4][i%4]));
	result += CreateFormattedString("%g", F32(v.elements[3][3]));
	tstr.format( "%s", result.c_str() );
}

template<> CMatrix4 CPropType<CMatrix4>::FromString(const PropTypeString& String)
{
	float m[4][4];
	sscanf(String.c_str(), "%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g",
		&m[0][0], &m[0][1], &m[0][2], &m[0][3],
		&m[1][0], &m[1][1], &m[1][2], &m[1][3],
		&m[2][0], &m[2][1], &m[2][2], &m[2][3],
		&m[3][0], &m[3][1], &m[3][2], &m[3][3]);
	CMatrix4 result;
	for(int i = 0; i < 16; i++)
		result.elements[i/4][i%4] = m[i/4][i%4];
	return result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace reflect {
template<> void Serialize( const CMatrix4*in, CMatrix4*out, reflect::BidirectionalSerializer& bidi )
{
	if( bidi.Serializing() )
	{
		bidi.Serializer()->Hint("CMatrix4");
		for( int i=0; i<16; i++ )
		{
			bidi | in->GetArray()[i];
		}
	}
	else
	{
		for( int i=0; i<16; i++ )
		{
			bidi | out->GetArray()[i];
		}
	}
}
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template class CPropType<CMatrix4>;
template class TMatrix4<float>;		// explicit template instantiation

}
