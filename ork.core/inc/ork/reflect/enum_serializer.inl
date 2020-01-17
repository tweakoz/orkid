////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/kernel/string/ConstString.h>
#include <ork/reflect/BidirectionalSerializer.h>

/// @file
///
/// BEGIN_ENUM_SERIALIZER(NAMESPACE, ENUMTYPE)
///     DECLARE_ENUM( ENUM )
///     ...
/// END_ENUM_SERIALIZER()
///
///
/// Example usage:
///
/// <pre>
/// BEGIN_ENUM_SERIALIZER(ork::fsm, ComparatorOperator)
///     DECLARE_ENUM(UNDEFINED)
///     DECLARE_ENUM(LESS_THAN)
///     DECLARE_ENUM(LESS_THAN_OR_EQUAL)
///     DECLARE_ENUM(GREATER_THAN)
///     DECLARE_ENUM(GREATER_THAN_OR_EQUAL)
///     DECLARE_ENUM(EQUAL)
///     DECLARE_ENUM(NOT_EQUAL)
/// END_ENUM_SERIALIZER()
/// </pre>
///
/// or for in-class enums:
/// BEGIN_ENUM_SERIALIZER(ork, CountdownTimer::TimerUnits)
///     DECLARE_CLASS_ENUM(CountdownTimer, SECONDS)
///     DECLARE_CLASS_ENUM(CountdownTimer, VBLANKS)
/// END_ENUM_SERIALIZER()
///

namespace ork { namespace reflect {

struct EnumNameMap
{
	int value;
	const char *name;
};

const char *DoSerializeEnum(int value, EnumNameMap *enum_map, BidirectionalSerializer &bidi);
int DoDeserializeEnum(const ConstString &name, EnumNameMap *enum_map, BidirectionalSerializer &bidi);

template<typename EnumType>
inline void SerializeEnum(const EnumType *in, EnumType *out, BidirectionalSerializer &bidi, EnumNameMap *enum_map)
{	    
	if(bidi.Serializing())
	{
		int value = int(*in);
		ConstString name = DoSerializeEnum(value, enum_map, bidi);
		bidi | name;
	}
	else
	{
		ConstString name;
		bidi | name;
		int value = DoDeserializeEnum(name, enum_map, bidi);
		*out = EnumType(value);
	}
}

} }


#define BEGIN_ENUM_SERIALIZER(Namespace, Type) \
	namespace ork { namespace reflect { \
	template<> void Serialize(const Namespace::Type *in, Namespace::Type *out, BidirectionalSerializer &bidi) \
	{ \
		using namespace Namespace; \
		\
		static EnumNameMap sEnumMap[] = {

#define DECLARE_ENUM(ENUM) \
	{ int(ENUM), #ENUM },

#define DECLARE_ENUM_VALUE(ENUM, VALUE) \
	{ int(ENUM), VALUE },

#define DECLARE_CLASS_ENUM(ClassType, ENUM) \
	{ int(ClassType::ENUM), #ENUM },

#define END_ENUM_SERIALIZER() \
			{ 0, NULL } \
		}; \
		\
		ork::reflect::SerializeEnum(in, out, bidi, sEnumMap); \
    } \
    } }

