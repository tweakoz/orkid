////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

# if defined(ORK_OSX)

#include <ork/pch.h>
#include <objc/runtime.h>

namespace ork {
namespace Objc {

	///////////////////////////////////////////////////////////////
	struct Class;
	struct Object;
	///////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////
	struct Class
	{
		::Class	mClass;
		Class( const char* name );
		//////////////////////////////////////////
		static Class FromObject( id obj );
		Object Alloc();
		//////////////////////////////////////////
		SEL GetSelectorByName( const char* name );
		IMP GetMethodBySel( SEL the_sel );
		IMP GetMethodByName( const char* name );
		//////////////////////////////////////////
		id InvokeI( id obj, const char* methodname );
		id InvokeIV( id obj, const char* methodname, void* arg0 );
		id InvokeII( id obj, const char* methodname, id arg0 );
		id InvokeIII( id obj, const char* methodname, id arg0, id arg1 );
		id InvokeIIII( id obj, const char* methodname, id arg0, id arg1, id arg2 );
		void InvokeV( id obj, const char* methodname );
		void InvokeVI( id obj, const char* methodname, id arg0 );
		void* InvokeP( id obj, const char* methodname );
		void InvokeVVN( id obj, const char* methodname, void* arg0, int arg1 );
		BOOL InvokeBDI( id obj, const char* methodname, double arg0, id arg1 );
		//////////////////////////////////////////
		id InvokeClassMethodI( const char* name );
		void InvokeClassMethodV( const char* name );
		id InvokeClassMethodIP( const char* name, void* p );
		id InvokeClassMethodII( const char* name, id arg0 );
		id InvokeClassMethodIC( const char* name, const char* c );
		//////////////////////////////////////////
	
	};
	//////////////////////////////////////////////////////////////
	struct Object
	{
		Class	mClass;
		id		mInstance;
		//////////////////////////////////////////////////////////////
		Object() : mClass(nil), mInstance(nil) {}
		Object( const Class& cls, id inst ) : mClass(cls), mInstance(inst) {}
		//Object( const Object& oth ) : mClass(oth.mClass), mInstance(oth.mInstance) {}
		Object( id obj ) : mClass(Class::FromObject(obj)), mInstance(obj) {}
		void Dump();
		//////////////////////////////////////////////////////////////
		Object Init();
		Object InitV( const char* methodname, void* arg0 );
		Object InitI( const char* methodname, id arg0 );
		Object InitII( const char* methodname, id arg0, id arg1 );
		Object InitIII( const char* methodname, id arg0, id arg1, id arg2 );
		//////////////////////////////////////////////////////////////
		void InvokeV( const char* methodname );
		id InvokeI( const char* methodname );
		void InvokeVI( const char* methodname, id arg0 );
		void* InvokeP( const char* methodname );
		void InvokeVVN( const char* methodname, void* arg0, int arg1 );
		BOOL InvokeBDI( const char* methodname, double arg0, id arg1 );
	};

	//////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////
} // namespace ObjC
} // namespace ork

#endif // #if defined( ORK_CONFIG_OPENGL ) && defined(_IX)
