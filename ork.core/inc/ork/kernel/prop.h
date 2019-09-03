////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <stdio.h>
#include <string.h>
#include <ork/util/Context.h>
#include <ork/kernel/tempstring.h>
///////////////////////////////////////////////////////////////////////////////
#if defined( NITRO ) || defined(IX) || defined(_PSP) || defined(WII)
inline char *_strdup( const char *str)
{
	unsigned len = strlen( str );
	char *dup = new char[len + 1];
	memcpy( dup, str, len );
	dup[len] = '\0';
	return dup;
}
#endif
///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

// TODO: move this type out to common?

/**
 * Indicates the Type of a CProp instance. Types are divided into categories that assist
 * in writing the most generic code possible for (de)serialization and GUI editing.
 */


enum EPropType
{
	// Bool Type
	EPROPTYPE_BOOL = 0,

	// Signed Basic Types
	EPROPTYPE_S8,
	EPROPTYPE_S16,
	EPROPTYPE_S32,
	EPROPTYPE_S64,

	// Unsigned Basic Types
	EPROPTYPE_U8,
	EPROPTYPE_U16,
	EPROPTYPE_U32,

	// Real Basic Type
	EPROPTYPE_REAL,

	// String Type
	EPROPTYPE_STRING,

	EPROPTYPE_TRANSFORMNODE2D,
	EPROPTYPE_TRANSFORMNODE3D,

	// Fixed-array Types
	EPROPTYPE_VEC2REAL,
	EPROPTYPE_VEC3FLOAT,
	EPROPTYPE_VEC4REAL,
	EPROPTYPE_MAT33REAL,
	EPROPTYPE_MAT44REAL,
	EPROPTYPE_QUATERNION,

	// Class Type
	EPROPTYPE_CLASSPTR,

	// Object types
	EPROPTYPE_OBJECTPTR,
	EPROPTYPE_OBJECTDELEGATE,
	EPROPTYPE_OBJECTREFERENCE,

	EPROPTYPE_SAMPLER,

	// Asset types
	EPROPTYPE_ASSET,
	//EPROPTYPE_MODELASSET,
	//EPROPTYPE_ANIMASSET,

	EPROPTYPE_ARRAY,
	EPROPTYPE_MAP,
	EPROPTYPE_ENUM,

	EPROPTYPE_END,
};


/**
 * Provides a type-safe mechanism for determining the Type and TypeName of a particular
 * C/C++ data type.
 *
 * For example, PropType<bool>::GetType() returns ETYPE_BOOL and PropType<bool>::GetTypeName() returns "BOOL".
 *
 * The CTypedProp implementation of CProp defers to PropType for its runtime type identification information.
 *
 * The definitions of meType and mstrTypeName are all located in attr.hpp as template specializations.
 *
 * The one exception to the rules is ETYPE_OBJECTDELEGATE. That type is not returned by any specialization
 * of PropType because it would conflict with ETYPE_OBJECTPTR. Therefore ETYPE_OBJECTDELEGATE is almost
 * entirely implemented by CObjectDelegateProp.
 */

template<typename T>
class PropType
{
public:

	template <typename U>
	static U FindValFromStrings( const std::string& String, const std::string Strings[], U defaultval );

	static EPropType GetType() { return meType; }
	static const char * GetTypeName() { return mstrTypeName; }

	static void ToString( const T & Value, PropTypeString& pstr );
	static T FromString( const PropTypeString& String );

	static void GetValueSet( std::string const * & ValueStrings, int & NumStrings );

private:

    static const EPropType meType;
	static const char * mstrTypeName;
};

///////////////////////////////////////////////////////////////////////////////

class PropSetContext : public util::Context<PropSetContext>
{

public:

	enum EContext
	{
		EERROR = 0,
		EDESERIALIZE,
		EPROPEDITOR,
	};

	PropSetContext( EContext ectx ) : meCtx( ectx ) {}

	EContext GetValue() const { return meCtx; }

private:

	EContext meCtx;

};


///////////////////////////////////////////////////////////////////////////////

}
