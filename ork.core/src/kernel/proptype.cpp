////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/kernel/string/string.h>
#include <ork/rtti/ICastable.h>
#include <ork/reflect/Serializable.h>
#include <ork/object/connect.h>

#include <ork/file/file.h>
#include <ork/application/application.h>

namespace ork {
/*
template <typename T>
T PropType<T>::FindValFromStrings( const std::string& String, const std::string Strings[], T defaultval )
{	T rval = defaultval;
	int idx = 0;
	while( idx>=0 )
	{	const std::string & ptest = Strings[idx];
		if( ptest.length() )
		{	if( ptest == String )
			{	rval = T(idx);
			}
			idx++;
		}
		else
			idx = -1;
	}
	return rval;
}


template <>
std::string PropType<std::string>::FindValFromStrings( const std::string& String, const std::string Strings[], std::string defaultval )
{	std::string rval = defaultval;
	int idx = 0;
	while( idx>=0 )
	{	const std::string & ptest = Strings[idx];
		if( ptest.length() )
		{	//if( ptest == String )
			//{	rval << idx;
			//}
			idx++;
		}
		else
			idx = -1;
	}
	return rval;
}

template <>
PoolString PropType<PoolString>::FindValFromStrings( const std::string& String, const std::string Strings[], PoolString defaultval )
{	PoolString rval = defaultval;
	int idx = 0;
	while( idx>=0 )
	{	const std::string & ptest = Strings[idx];
		if( ptest.length() )
		{	//if( ptest == String )
			//{	rval << idx;
			//}
			idx++;
		}
		else
			idx = -1;
	}
	return rval;
}

template <>
object::Signal PropType<object::Signal>::FindValFromStrings( const std::string& String, const std::string Strings[], object::Signal defaultval )
{	object::Signal rval = defaultval;
	int idx = 0;
	while( idx>=0 )
	{	const std::string & ptest = Strings[idx];
		if( ptest.length() )
		{	//if( ptest == String )
			//{	rval << idx;
			//}
			idx++;
		}
		else
			idx = -1;
	}
	return rval;
}
*/
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<> const EPropType PropType<CClass*>::meType				= EPROPTYPE_CLASSPTR;
template<> const char* PropType<CClass*>::mstrTypeName				= "CLASSPTR";
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<> const EPropType PropType<bool>::meType					= EPROPTYPE_BOOL;
template<> const EPropType PropType<char>::meType					= EPROPTYPE_S8;
template<> const char* PropType<char>::mstrTypeName				= "S8";
template<> const char* PropType<bool>::mstrTypeName				= "BOOL";
///////////////////////////////////////////////////////////////////////////////
template<> void PropType<bool>::ToString(const bool & Value,PropTypeString& pstr)
{
	pstr.set( Value ? "true" : "false" );
}
template<> bool PropType<bool>::FromString(const PropTypeString& String)
{
	if(strcmp(String.c_str(), "true") == 0)
		return true;
	else
		return false;
}
template<> void PropType<bool>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}

template<> void PropType<char>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
template<> void PropType<char>::ToString(const char & Value, PropTypeString& tstr )
{
	tstr.format( "%c", Value );
}
template<> char PropType<char>::FromString(const PropTypeString& String)
{
	char Value;
	sscanf(String.c_str(), "%c", &Value);
	return Value;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<> const EPropType PropType<s16>::meType					= EPROPTYPE_S16;
template<> const EPropType PropType<s32>::meType					= EPROPTYPE_S32;
template<> const EPropType PropType<s64>::meType					= EPROPTYPE_S64;
///////////////////////////////////////////////////////////////////////////////
template<> const char* PropType<s16>::mstrTypeName				= "S16";
template<> const char* PropType<s32>::mstrTypeName				= "S32";
template<> const char* PropType<s64>::mstrTypeName					= "S64";
///////////////////////////////////////////////////////////////////////////////
template<> void PropType<s16>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
template<> void PropType<s32>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
template<> void PropType<s64>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
template<> void PropType<s16>::ToString(const s16& Value, PropTypeString& tstr )
{
	tstr.format( "%d", int(Value) );
}
template<> void PropType<s32>::ToString(const s32 & Value, PropTypeString& tstr )
{
	tstr.format( "%d", int(Value) );
}
template<> void PropType<s64>::ToString(const s64 & Value, PropTypeString& tstr )
{
	tstr.format( "%d", Value );
}
///////////////////////////////////////////////////////////////////////////////
template<> s16 PropType<s16>::FromString(const PropTypeString& String)
{
	int Value;
	sscanf(String.c_str(), "%d", &Value);
	return s16(Value);
}
template<> s32 PropType<s32>::FromString(const PropTypeString& String)
{
	int Value;
	sscanf(String.c_str(), "%d", &Value);
	return long(Value);
}
template<> s64 PropType<s64>::FromString(const PropTypeString& String)
{
	s64 Value;
	sscanf(String.c_str(), "%lld", &Value);
	return Value;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<> const EPropType PropType<U16>::meType					= EPROPTYPE_U16;
template<> const EPropType PropType<U32>::meType					= EPROPTYPE_U32;
template<> const char* PropType<U16>::mstrTypeName					= "U16";
template<> const char* PropType<U32>::mstrTypeName					= "U32";
///////////////////////////////////////////////////////////////////////////////
template<> void PropType<U16>::ToString(const U16 & Value, PropTypeString& tstr )
{
	tstr.format("%d", Value);
}
template<> U16 PropType<U16>::FromString(const PropTypeString& String)
{
	U16 Value;
	sscanf(String.c_str(), "%hu", &Value);
	return Value;
}
template<> void PropType<U32>::ToString(const U32 & Value, PropTypeString& tstr )
{
	tstr.format("%d", Value);
}
template<> U32 PropType<U32>::FromString(const PropTypeString& String)
{
	U32 Value;
	sscanf(String.c_str(), "%u", &Value);
	return Value;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<> const EPropType PropType<float>::meType					= EPROPTYPE_REAL;
template<> const EPropType PropType<double>::meType				= EPROPTYPE_REAL;
template<> const char* PropType<float>::mstrTypeName				= "REAL";
template<> const char* PropType<double>::mstrTypeName				= "REAL";
///////////////////////////////////////////////////////////////////////////////
template<> void PropType<float>::ToString(const float & Value, PropTypeString& tstr )
{
	tstr.format("%g", float(Value));
}

template<> float PropType<float>::FromString(const PropTypeString& String)
{
	float Value;
	sscanf(String.c_str(), "%g", &Value);
	return float(Value);
}
template<> void PropType<float>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}

template<> void PropType<double>::ToString(const double & Value, PropTypeString& tstr )
{
	tstr.format( "%f", float(Value) );
}

template<> double PropType<double>::FromString(const PropTypeString& String)
{
	float Value;
	sscanf(String.c_str(), "%f", &Value);
	return double(Value);
}
template<> void PropType<double>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<> const EPropType PropType<std::string>::meType			= EPROPTYPE_STRING;
template<> const EPropType PropType<const char *>::meType			= EPROPTYPE_STRING;
template<> const EPropType PropType<PoolString>::meType			= EPROPTYPE_STRING;
template<> const EPropType PropType<Char4>::meType					= EPROPTYPE_STRING;
template<> const EPropType PropType<Char8>::meType					= EPROPTYPE_STRING;
template<> const EPropType PropType<file::Path>::meType			= EPROPTYPE_STRING;
template<> const char* PropType<std::string>::mstrTypeName			= "STRING";
template<> const char* PropType<const char *>::mstrTypeName		= "STRING";
template<> const char* PropType<PoolString>::mstrTypeName			= "STRING";
template<> const char* PropType<Char4>::mstrTypeName				= "STRING";
template<> const char* PropType<Char8>::mstrTypeName				= "STRING";
template<> const char* PropType<file::Path>::mstrTypeName			= "STRING";
///////////////////////////////////////////////////////////////////////////////
template<> void PropType<file::Path>::ToString(const file::Path & Value, PropTypeString& tstr)
{
	tstr.format( "%s", Value.c_str() );
}
template<> file::Path PropType<file::Path>::FromString(const PropTypeString& String)
{
	return file::Path(String.c_str());
}
template<> void PropType<file::Path>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
template<> void PropType<std::string>::ToString(const std::string & Value, PropTypeString& tstr)
{
	tstr.format( "%s", Value.c_str() );
}
template<> std::string PropType<std::string>::FromString(const PropTypeString& String)
{
	return std::string(String.c_str());
}
template<> void PropType<std::string>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
template<> void PropType<Char4>::ToString(const Char4 & Value, PropTypeString& tstr)
{
	tstr.format( "%s", Value.c_str() );
}
template<> Char4 PropType<Char4>::FromString(const PropTypeString& String)
{
	return Char4(String.c_str());
}
template<> void PropType<Char4>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
template<> void PropType<Char8>::ToString(const Char8 & Value, PropTypeString& tstr)
{
	tstr.format( "%s", Value.c_str() );
}
template<> Char8 PropType<Char8>::FromString(const PropTypeString& String)
{
	return Char8(String.c_str());
}
template<> void PropType<Char8>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
template<> void PropType<const char *>::ToString( const char * const & Value, PropTypeString& tstr)
{
	tstr.format( "%s", Value );
}
template<> const char * PropType<const char *>::FromString(const PropTypeString& String)
{
	static char FixedStringBuffer[256];
	strcpy( FixedStringBuffer, String.c_str() );
	return FixedStringBuffer;
}
template<> void PropType<const char *>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
template<> PoolString PropType<PoolString>::FromString(const PropTypeString& Value)
{
	const char *value = Value.c_str();
	return AddPooledString(value);
}

template<> void PropType<PoolString>::ToString(const PoolString & Value, PropTypeString& tstr)
{
	if( Value.c_str() )
	{
		tstr.format( "%s", Value.c_str() );
	}
	else
	{
		tstr.format( "" );
	}
}
template<> void PropType<PoolString>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<> const EPropType PropType<ork::Object*>::meType							= EPROPTYPE_OBJECTPTR;
template<> const EPropType PropType<const ork::Object*>::meType					= EPROPTYPE_OBJECTPTR;
template<> const EPropType PropType<ork::object::Signal>::meType					= EPROPTYPE_OBJECTPTR;

template<> const char* PropType<ork::Object*>::mstrTypeName						= "OBJECTPTR";
template<> const char* PropType<const ork::Object*>::mstrTypeName					= "OBJECTPTR";
template<> const char* PropType<ork::object::Signal>::mstrTypeName				= "OBJECTPTR";

template<> void PropType<ork::Object*>::ToString(ork::Object* const & Value, PropTypeString& tstr)
{
	ork::Object* pObjRef = static_cast<ork::Object*>(Value);
	tstr.format("%p", pObjRef );
}
template<> void PropType<const ork::Object*>::ToString(const ork::Object* const & Value, PropTypeString& tstr)
{
	const ork::Object* pObjRef = static_cast<const ork::Object*>(Value);
	tstr.format("%p", pObjRef );
}
template<> void PropType<ork::object::Signal>::ToString(ork::object::Signal const & Value, PropTypeString& tstr)
{
	tstr.format("%p", (void*) & Value );
}

template<> ork::Object* PropType<ork::Object*>::FromString(const PropTypeString& String)
{
	void* pobj;
	sscanf(String.c_str(), "%p", &pobj);
	return (ork::Object*) pobj;
}
template<> const ork::Object* PropType<const ork::Object*>::FromString(const PropTypeString& String)
{
	const void* pobj;
	sscanf(String.c_str(), "%p", &pobj);

	return (const ork::Object*) pobj;
}
template<> ork::object::Signal PropType<ork::object::Signal>::FromString(const PropTypeString& String)
{
	const void* pobj;
	sscanf(String.c_str(), "%p", &pobj);
	return *((const ork::object::Signal*) pobj);
}
template<> void PropType<ork::object::Signal*>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
template<> void PropType<const ork::Object*>::GetValueset( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<> const EPropType PropType<rtti::ICastable*>::meType		= EPROPTYPE_OBJECTPTR;
template<> const char* PropType<rtti::ICastable*>::mstrTypeName	= "OBJECTPTR";

template<> void PropType<rtti::ICastable*>::ToString(rtti::ICastable* const & Value, PropTypeString& tstr)
{
	rtti::ICastable* pObjRef = static_cast<rtti::ICastable*>(Value);
	tstr.format("%p", pObjRef );
}

template<> void PropType<const rtti::ICastable*>::ToString(const rtti::ICastable* const & Value, PropTypeString& tstr)
{
	const rtti::ICastable* pObjRef = static_cast<const rtti::ICastable*>(Value);
	tstr.format("%p", pObjRef );
}

template<> rtti::ICastable* PropType<rtti::ICastable*>::FromString(const PropTypeString& String)
{
	void* pobj;
	sscanf(String.c_str(), "%p", &pobj);
	return (rtti::ICastable*) pobj;
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// PropertyType Template Instantiations
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

template struct PropType<bool>;
template struct PropType<S32>;
template struct PropType<U32>;
template struct PropType<U16>;
//template struct PropType<int>;
template struct PropType<float>;
template struct PropType<std::string>;
template struct PropType<Char8>;
template struct PropType<rtti::ICastable*>;
//template struct PropType<ork::Object*>;
template struct PropType<ork::Object*>;
template struct PropType<ork::object::Signal>;
template struct PropType<PoolString>;

}
