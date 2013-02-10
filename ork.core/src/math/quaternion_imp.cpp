////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/math/quaternion.h>
#include <ork/math/quaternion.hpp>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/reflect/Serialize.h>
#include <ork/reflect/BidirectionalSerializer.h>
#include <ork/kernel/string/string.h>

namespace ork {
template class TQuaternion<float>;		// explicit template instantiation
typedef class TQuaternion<float> CQuaternion;
template<> const EPropType CPropType<CQuaternion>::meType = EPROPTYPE_QUATERNION;
template<> const char* CPropType<CQuaternion>::mstrTypeName	= "QUATERNION";

///////////////////////////////////////////////////////////////////////////////

template<> void CPropType<CQuaternion>::ToString( const CQuaternion & Value, PropTypeString& tstr)
{
	const CQuaternion & v = Value;

	std::string result;
	result += CreateFormattedString("%g ", v.GetX() );
	result += CreateFormattedString("%g ", v.GetY() );
	result += CreateFormattedString("%g ", v.GetZ() );
	result += CreateFormattedString("%g ", v.GetW() );
	tstr.format( "%s", result.c_str() );
}

///////////////////////////////////////////////////////////////////////////////

template<> CQuaternion CPropType<CQuaternion>::FromString(const PropTypeString& String)
{
	float m[4];
	sscanf(String.c_str(), "%g %g %g %g",
		&m[0], &m[1], &m[2], &m[3] );
	CQuaternion result;
	result.SetX( m[0] );
	result.SetY( m[1] );
	result.SetZ( m[2] );
	result.SetW( m[3] );
	return result;
}

///////////////////////////////////////////////////////////////////////////////

namespace reflect {
	
	template<> void Serialize( const CQuaternion*in, CQuaternion*out, reflect::BidirectionalSerializer& bidi )
	{
		if( bidi.Serializing() )
		{
			bidi.Serializer()->Hint("CQuaternion");
			bidi | in->GetX();
			bidi | in->GetY();
			bidi | in->GetZ();
			bidi | in->GetW();
		}
		else
		{
			float m[4];
			for( int i=0; i<4; i++ )
			{
				bidi | m[i];
			}
			out->SetX( m[0] );
			out->SetY( m[1] );
			out->SetZ( m[2] );
			out->SetW( m[3] );
		}
	}
}

}
