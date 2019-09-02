////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
T CPropType<T>::FindValFromStrings( const std::string& String, const std::string Strings[], T defaultval )
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
std::string CPropType<std::string>::FindValFromStrings( const std::string& String, const std::string Strings[], std::string defaultval )
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
PoolString CPropType<PoolString>::FindValFromStrings( const std::string& String, const std::string Strings[], PoolString defaultval )
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
object::Signal CPropType<object::Signal>::FindValFromStrings( const std::string& String, const std::string Strings[], object::Signal defaultval )
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
template<> const EPropType CPropType<CClass*>::meType				= EPROPTYPE_CLASSPTR;
template<> const char* CPropType<CClass*>::mstrTypeName				= "CLASSPTR";
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<> const EPropType CPropType<bool>::meType					= EPROPTYPE_BOOL;
template<> const EPropType CPropType<char>::meType					= EPROPTYPE_S8;
template<> const char* CPropType<char>::mstrTypeName				= "S8";
template<> const char* CPropType<bool>::mstrTypeName				= "BOOL";
///////////////////////////////////////////////////////////////////////////////
template<> void CPropType<bool>::ToString(const bool & Value,PropTypeString& pstr)
{
	pstr.set( Value ? "true" : "false" );
}
template<> bool CPropType<bool>::FromString(const PropTypeString& String)
{
	if(strcmp(String.c_str(), "true") == 0)
		return true;
	else
		return false;
}
template<> void CPropType<bool>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}

template<> void CPropType<char>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
template<> void CPropType<char>::ToString(const char & Value, PropTypeString& tstr )
{
	tstr.format( "%c", Value );
}
template<> char CPropType<char>::FromString(const PropTypeString& String)
{
	char Value;
	sscanf(String.c_str(), "%c", &Value);
	return Value;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<> const EPropType CPropType<s16>::meType					= EPROPTYPE_S16;
template<> const EPropType CPropType<s32>::meType					= EPROPTYPE_S32;
template<> const EPropType CPropType<s64>::meType					= EPROPTYPE_S64;
///////////////////////////////////////////////////////////////////////////////
template<> const char* CPropType<s16>::mstrTypeName				= "S16";
template<> const char* CPropType<s32>::mstrTypeName				= "S32";
template<> const char* CPropType<s64>::mstrTypeName					= "S64";
///////////////////////////////////////////////////////////////////////////////
template<> void CPropType<s16>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
template<> void CPropType<s32>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
template<> void CPropType<s64>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
template<> void CPropType<s16>::ToString(const s16& Value, PropTypeString& tstr )
{
	tstr.format( "%d", int(Value) );
}
template<> void CPropType<s32>::ToString(const s32 & Value, PropTypeString& tstr )
{
	tstr.format( "%d", int(Value) );
}
template<> void CPropType<s64>::ToString(const s64 & Value, PropTypeString& tstr )
{
	tstr.format( "%d", Value );
}
///////////////////////////////////////////////////////////////////////////////
template<> s16 CPropType<s16>::FromString(const PropTypeString& String)
{
	int Value;
	sscanf(String.c_str(), "%d", &Value);
	return s16(Value);
}
template<> s32 CPropType<s32>::FromString(const PropTypeString& String)
{
	int Value;
	sscanf(String.c_str(), "%d", &Value);
	return long(Value);
}
template<> s64 CPropType<s64>::FromString(const PropTypeString& String)
{
	s64 Value;
	sscanf(String.c_str(), "%lld", &Value);
	return Value;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<> const EPropType CPropType<U16>::meType					= EPROPTYPE_U16;
template<> const EPropType CPropType<U32>::meType					= EPROPTYPE_U32;
template<> const char* CPropType<U16>::mstrTypeName					= "U16";
template<> const char* CPropType<U32>::mstrTypeName					= "U32";
///////////////////////////////////////////////////////////////////////////////
template<> void CPropType<U16>::ToString(const U16 & Value, PropTypeString& tstr )
{
	tstr.format("%d", Value);
}
template<> U16 CPropType<U16>::FromString(const PropTypeString& String)
{
	U16 Value;
	sscanf(String.c_str(), "%hu", &Value);
	return Value;
}
template<> void CPropType<U32>::ToString(const U32 & Value, PropTypeString& tstr )
{
	tstr.format("%d", Value);
}
template<> U32 CPropType<U32>::FromString(const PropTypeString& String)
{
	U32 Value;
	sscanf(String.c_str(), "%u", &Value);
	return Value;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<> const EPropType CPropType<float>::meType					= EPROPTYPE_REAL;
template<> const EPropType CPropType<double>::meType				= EPROPTYPE_REAL;
template<> const char* CPropType<float>::mstrTypeName				= "REAL";
template<> const char* CPropType<double>::mstrTypeName				= "REAL";
///////////////////////////////////////////////////////////////////////////////
template<> void CPropType<float>::ToString(const float & Value, PropTypeString& tstr )
{
	tstr.format("%g", float(Value));
}

template<> float CPropType<float>::FromString(const PropTypeString& String)
{
	float Value;
	sscanf(String.c_str(), "%g", &Value);
	return float(Value);
}
template<> void CPropType<float>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}

template<> void CPropType<double>::ToString(const double & Value, PropTypeString& tstr )
{
	tstr.format( "%f", float(Value) );
}

template<> double CPropType<double>::FromString(const PropTypeString& String)
{
	float Value;
	sscanf(String.c_str(), "%f", &Value);
	return double(Value);
}
template<> void CPropType<double>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<> const EPropType CPropType<std::string>::meType			= EPROPTYPE_STRING;
template<> const EPropType CPropType<const char *>::meType			= EPROPTYPE_STRING;
template<> const EPropType CPropType<PoolString>::meType			= EPROPTYPE_STRING;
template<> const EPropType CPropType<Char4>::meType					= EPROPTYPE_STRING;
template<> const EPropType CPropType<Char8>::meType					= EPROPTYPE_STRING;
template<> const EPropType CPropType<file::Path>::meType			= EPROPTYPE_STRING;
template<> const char* CPropType<std::string>::mstrTypeName			= "STRING";
template<> const char* CPropType<const char *>::mstrTypeName		= "STRING";
template<> const char* CPropType<PoolString>::mstrTypeName			= "STRING";
template<> const char* CPropType<Char4>::mstrTypeName				= "STRING";
template<> const char* CPropType<Char8>::mstrTypeName				= "STRING";
template<> const char* CPropType<file::Path>::mstrTypeName			= "STRING";
///////////////////////////////////////////////////////////////////////////////
template<> void CPropType<file::Path>::ToString(const file::Path & Value, PropTypeString& tstr)
{
	tstr.format( "%s", Value.c_str() );
}
template<> file::Path CPropType<file::Path>::FromString(const PropTypeString& String)
{
	return file::Path(String.c_str());
}
template<> void CPropType<file::Path>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
template<> void CPropType<std::string>::ToString(const std::string & Value, PropTypeString& tstr)
{
	tstr.format( "%s", Value.c_str() );
}
template<> std::string CPropType<std::string>::FromString(const PropTypeString& String)
{
	return std::string(String.c_str());
}
template<> void CPropType<std::string>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
template<> void CPropType<Char4>::ToString(const Char4 & Value, PropTypeString& tstr)
{
	tstr.format( "%s", Value.c_str() );
}
template<> Char4 CPropType<Char4>::FromString(const PropTypeString& String)
{
	return Char4(String.c_str());
}
template<> void CPropType<Char4>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
template<> void CPropType<Char8>::ToString(const Char8 & Value, PropTypeString& tstr)
{
	tstr.format( "%s", Value.c_str() );
}
template<> Char8 CPropType<Char8>::FromString(const PropTypeString& String)
{
	return Char8(String.c_str());
}
template<> void CPropType<Char8>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
template<> void CPropType<const char *>::ToString( const char * const & Value, PropTypeString& tstr)
{
	tstr.format( "%s", Value );
}
template<> const char * CPropType<const char *>::FromString(const PropTypeString& String)
{
	static char FixedStringBuffer[256];
	strcpy( FixedStringBuffer, String.c_str() );
	return FixedStringBuffer;
}
template<> void CPropType<const char *>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
template<> PoolString CPropType<PoolString>::FromString(const PropTypeString& Value)
{
	const char *value = Value.c_str();
	return AddPooledString(value);
}

template<> void CPropType<PoolString>::ToString(const PoolString & Value, PropTypeString& tstr)
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
template<> void CPropType<PoolString>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<> const EPropType CPropType<ork::Object*>::meType							= EPROPTYPE_OBJECTPTR;
template<> const EPropType CPropType<const ork::Object*>::meType					= EPROPTYPE_OBJECTPTR;
template<> const EPropType CPropType<ork::object::Signal>::meType					= EPROPTYPE_OBJECTPTR;

template<> const char* CPropType<ork::Object*>::mstrTypeName						= "OBJECTPTR";
template<> const char* CPropType<const ork::Object*>::mstrTypeName					= "OBJECTPTR";
template<> const char* CPropType<ork::object::Signal>::mstrTypeName				= "OBJECTPTR";

template<> void CPropType<ork::Object*>::ToString(ork::Object* const & Value, PropTypeString& tstr)
{
	ork::Object* pObjRef = static_cast<ork::Object*>(Value);
	tstr.format("%p", pObjRef );
}
template<> void CPropType<const ork::Object*>::ToString(const ork::Object* const & Value, PropTypeString& tstr)
{
	const ork::Object* pObjRef = static_cast<const ork::Object*>(Value);
	tstr.format("%p", pObjRef );
}
template<> void CPropType<ork::object::Signal>::ToString(ork::object::Signal const & Value, PropTypeString& tstr)
{
	tstr.format("%p", (void*) & Value );
}

template<> ork::Object* CPropType<ork::Object*>::FromString(const PropTypeString& String)
{
	ork::Object *pobj;
	sscanf(String.c_str(), "%p", &pobj);
	return pobj;
}
template<> const ork::Object* CPropType<const ork::Object*>::FromString(const PropTypeString& String)
{
	const ork::Object* pobj;
	sscanf(String.c_str(), "%p", &pobj);
	return pobj;
}
template<> ork::object::Signal CPropType<ork::object::Signal>::FromString(const PropTypeString& String)
{
	ork::object::Signal* pobj;
	sscanf(String.c_str(), "%p", &pobj);
	return *pobj;
}
template<> void CPropType<ork::object::Signal*>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
template<> void CPropType<const ork::Object*>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = 0;
	ValueStrings = 0;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<> const EPropType CPropType<rtti::ICastable*>::meType		= EPROPTYPE_OBJECTPTR;
template<> const char* CPropType<rtti::ICastable*>::mstrTypeName	= "OBJECTPTR";

template<> void CPropType<rtti::ICastable*>::ToString(rtti::ICastable* const & Value, PropTypeString& tstr)
{
	rtti::ICastable* pObjRef = static_cast<rtti::ICastable*>(Value);
	tstr.format("%p", pObjRef );
}

template<> void CPropType<const rtti::ICastable*>::ToString(const rtti::ICastable* const & Value, PropTypeString& tstr)
{
	const rtti::ICastable* pObjRef = static_cast<const rtti::ICastable*>(Value);
	tstr.format("%p", pObjRef );
}

template<> rtti::ICastable* CPropType<rtti::ICastable*>::FromString(const PropTypeString& String)
{
	rtti::ICastable *pobj;
	sscanf(String.c_str(), "%p", &pobj);
	return pobj;
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// PropertyType Template Instantiations
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

template class CPropType<bool>;
template class CPropType<S32>;
template class CPropType<U32>;
template class CPropType<U16>;
//template class CPropType<int>;
template class CPropType<float>;
template class CPropType<std::string>;
template class CPropType<Char8>;
template class CPropType<rtti::ICastable*>;
//template class CPropType<ork::Object*>;
template class CPropType<ork::Object*>;
template class CPropType<ork::object::Signal>;
template class CPropType<PoolString>;

}
