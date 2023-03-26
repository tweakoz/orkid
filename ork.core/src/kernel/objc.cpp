////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/objc.h>

#if defined( ORK_OSX )
////////////////////////////////////////////////////
namespace ork { namespace Objc {
////////////////////////////////////////////////////

	////////////////////////////////////////////
	Class::Class( const char* name )
		: mClass(0)
	{
		if( name )
		{	mClass = (::Class) objc_getClass( name );
			OrkAssert( mClass != 0 );
		}
	}
	Class Class::FromObject( id obj )
	{
		Class ret(0);
		ret.mClass = (::Class) object_getClass( obj );
		return ret;
	}
	////////////////////////////////////////////
	Object Class::Alloc()
	{
		OrkAssert( mClass!=nil );
		id inst = class_createInstance( mClass, 0 );
		OrkAssert( inst!=nil );
		Object ret( *this, inst );
		return ret;
	}
	SEL Class::GetSelectorByName( const char* name )
	{
		OrkAssert( mClass!=nil );
		SEL the_sel = sel_getUid( name );
		OrkAssert( the_sel!=NULL );
		return the_sel;
	}
	IMP Class::GetMethodBySel( SEL the_sel )
	{
		OrkAssert( mClass!=nil );
		IMP ret = class_getMethodImplementation( mClass, the_sel );
		OrkAssert( ret!=NULL );
		return ret;
	}	
	IMP Class::GetMethodByName( const char* name )
	{
		SEL the_sel = GetSelectorByName( name );
		IMP ret = class_getMethodImplementation( mClass, the_sel );
		OrkAssert( ret!=nil );
		return ret;
	}
	id Class::InvokeClassMethodI( const char* name )
	{
		typedef id (*invocable)(::Class cls, SEL _cmd );
		SEL the_sel = GetSelectorByName( name );
		OrkAssert( the_sel != nil );
		Method the_meth = class_getClassMethod(mClass, the_sel );
		OrkAssert( the_meth != nil );
		IMP the_imp = method_getImplementation(the_meth);
		OrkAssert( the_imp != nil );
		invocable the_inv = (invocable) the_imp;
		id ret = the_inv( mClass, the_sel );
		return ret; 
	}
	id Class::InvokeClassMethodIC( const char* name, const char* p )
	{
		typedef id (*invocable)(::Class cls, SEL _cmd, const char*  );
		SEL the_sel = GetSelectorByName( name );
		OrkAssert( the_sel != nil );
		Method the_meth = class_getClassMethod(mClass, the_sel );
		OrkAssert( the_meth != nil );
		IMP the_imp = method_getImplementation(the_meth);
		OrkAssert( the_imp != nil );
		invocable the_inv = (invocable) the_imp;
		id ret = the_inv( mClass, the_sel,p );
		return ret; 
	}
	id Class::InvokeClassMethodII( const char* name, id arg )
	{
		typedef id (*invocable)(::Class cls, SEL _cmd, id arg0  );
		SEL the_sel = GetSelectorByName( name );
		OrkAssert( the_sel != nil );
		Method the_meth = class_getClassMethod(mClass, the_sel );
		OrkAssert( the_meth != nil );
		IMP the_imp = method_getImplementation(the_meth);
		OrkAssert( the_imp != nil );
		invocable the_inv = (invocable) the_imp;
		id ret = the_inv( mClass, the_sel, arg );
		return ret; 
	}
	id Class::InvokeClassMethodIP( const char* name, void* p )
	{
		typedef id (*invocable)(::Class cls, SEL _cmd, void*  );
		SEL the_sel = GetSelectorByName( name );
		OrkAssert( the_sel != nil );
		Method the_meth = class_getClassMethod(mClass, the_sel );
		OrkAssert( the_meth != nil );
		IMP the_imp = method_getImplementation(the_meth);
		OrkAssert( the_imp != nil );
		invocable the_inv = (invocable) the_imp;
		id ret = the_inv( mClass, the_sel,p );
		return ret; 
	}
	void Class::InvokeClassMethodV( const char* name )
	{
		typedef void (*invocable)(::Class cls, SEL _cmd );
		SEL the_sel = GetSelectorByName( name );
		OrkAssert( the_sel != nil );
		Method the_meth = class_getClassMethod(mClass, the_sel );
		OrkAssert( the_meth != nil );
		IMP the_imp = method_getImplementation(the_meth);
		OrkAssert( the_imp != nil );
		invocable the_inv = (invocable) the_imp;
		the_inv( mClass, the_sel );
	}
	void* Class::InvokeP( id obj, const char* methodname )
	{
		typedef void* (*invocable)(id self, SEL _cmd );
		SEL the_sel = this->GetSelectorByName( methodname );
		IMP the_imp = this->GetMethodBySel( the_sel );
		invocable i = (invocable) the_imp;
		void* ret = i(obj,the_sel);
		return ret;
	}
	id Class::InvokeI( id obj, const char* methodname )
	{
		typedef id (*invocable)(id self, SEL _cmd );
		SEL the_sel = this->GetSelectorByName( methodname );
		IMP the_imp = this->GetMethodBySel( the_sel );
		invocable i = (invocable) the_imp;
		id ret = i(obj,the_sel);
		return ret;
	}
	id Class::InvokeIV( id obj, const char* methodname, void* arg0 )
	{
		typedef id (*invocable)(id self, SEL _cmd, void* arg0);
		SEL the_sel = this->GetSelectorByName( methodname );
		IMP the_imp = this->GetMethodBySel( the_sel );
		invocable i = (invocable) the_imp;
		id ret = i(obj,the_sel,arg0);
		return ret;
	}
	id Class::InvokeII( id obj, const char* methodname, id arg0 )
	{
		typedef id (*invocable)(id self, SEL _cmd, id arg0);
		SEL the_sel = this->GetSelectorByName( methodname );
		IMP the_imp = this->GetMethodBySel( the_sel );
		invocable i = (invocable) the_imp;
		id ret = i(obj,the_sel,arg0);
		return ret;
	}
	id Class::InvokeIII( id obj, const char* methodname, id arg0, id arg1 )
	{
		typedef id (*invocable)(id self, SEL _cmd, id arg0, id arg1);
		SEL the_sel = this->GetSelectorByName( methodname );
		OrkAssert( the_sel != nil );
		IMP the_imp = this->GetMethodBySel( the_sel );
		OrkAssert( the_imp != nil );
		invocable i = (invocable) the_imp;
		id ret = i(obj,the_sel,arg0,arg1);
		OrkAssert( ret != nil );		
		return ret;
	}
	id Class::InvokeIIII( id obj, const char* methodname, id arg0, id arg1, id arg2 )
	{
		typedef id (*invocable)(id self, SEL _cmd, id arg0, id arg1, id arg2);
		SEL the_sel = this->GetSelectorByName( methodname );
		OrkAssert( the_sel != nil );
		IMP the_imp = this->GetMethodBySel( the_sel );
		OrkAssert( the_imp != nil );
		invocable i = (invocable) the_imp;
		id ret = i(obj,the_sel,arg0,arg1,arg2);
		OrkAssert( ret != nil );		
		return ret;
	}
	void Class::InvokeVI( id obj, const char* methodname, id arg0 )
	{
		typedef void (*invocable)(id self, SEL _cmd, id arg0 );
		SEL the_sel = this->GetSelectorByName( methodname );
		IMP the_imp = this->GetMethodBySel( the_sel );
		invocable i = (invocable) the_imp;
		i(obj,the_sel,arg0);
	}
	void Class::InvokeV( id obj, const char* methodname )
	{
		typedef void (*invocable)(id self, SEL _cmd );
		SEL the_sel = this->GetSelectorByName( methodname );
		IMP the_imp = this->GetMethodBySel( the_sel );
		invocable i = (invocable) the_imp;
		i(obj,the_sel);
	}
	void Class::InvokeVVN( id obj, const char* methodname, void* arg0, int arg1 )
	{
		typedef void (*invocable)(id self, SEL _cmd, void*, int );
		SEL the_sel = this->GetSelectorByName( methodname );
		IMP the_imp = this->GetMethodBySel( the_sel );
		invocable i = (invocable) the_imp;
		i(obj,the_sel,arg0,arg1);	
	}
	BOOL Class::InvokeBDI( id obj, const char* methodname, double arg0, id arg1 )
	{
		typedef BOOL (*invocable)(id self, SEL _cmd, double, id );
		SEL the_sel = this->GetSelectorByName( methodname );
		IMP the_imp = this->GetMethodBySel( the_sel );
		invocable i = (invocable) the_imp;
		return i(obj,the_sel,arg0,arg1);	
	}

	//////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////
	Object Object::Init()
	{
		Object ret( mClass, mClass.InvokeI( mInstance, "init" ) );
		OrkAssert( ret.mInstance!=nil);
		return ret;
	}
	Object Object::InitV( const char* methodname, void* arg0 )
	{
		Object ret( mClass, mClass.InvokeIV( mInstance, methodname, arg0 ) );
		OrkAssert( ret.mInstance!=nil);
		return ret;
	}
	Object Object::InitI( const char* methodname, id arg0 )
	{
		Object ret( mClass, mClass.InvokeII( mInstance, methodname, arg0 ) );
		OrkAssert( ret.mInstance!=nil);
		return ret;
	}
	Object Object::InitII( const char* methodname, id arg0, id arg1 )
	{
		Object ret( mClass, mClass.InvokeIII( mInstance, methodname, arg0, arg1 ) );
		OrkAssert( ret.mInstance!=nil);
		return ret;
	}
	Object Object::InitIII( const char* methodname, id arg0, id arg1, id arg2 )
	{
		Object ret( mClass, mClass.InvokeIIII( mInstance, methodname, arg0, arg1, arg2 ) );
		OrkAssert( ret.mInstance!=nil);
		return ret;
	}
	void Object::InvokeVI( const char* methodname, id arg0 )
	{
		mClass.InvokeVI( mInstance, methodname, arg0 );
	}
	void Object::InvokeV( const char* methodname )
	{
		mClass.InvokeV( mInstance, methodname );
	}
	void* Object::InvokeP( const char* methodname )
	{
		return mClass.InvokeP( mInstance, methodname );
	}
	void Object::InvokeVVN( const char* methodname, void* arg0, int arg1 )
	{
		return mClass.InvokeVVN( mInstance, methodname, arg0, arg1 );
	}
	BOOL Object::InvokeBDI( const char* methodname, double arg0, id arg1 )
	{
		return mClass.InvokeBDI( mInstance, methodname, arg0, arg1 );
	}

	void Object::Dump()
	{
		orkprintf( "//////////////////////////////////\n" );
		orkprintf( "Object<%p> mInstance<%p>\n", this, mInstance );
		id desc = mClass.InvokeI( mInstance, "description" );
		void* pstr = Class::FromObject(desc).InvokeP( desc, "cString" );
		orkprintf( " Desc<%p> cString<%s>\n", desc, (const char*) pstr );
		
		const char* ivarlayout = (const char*) class_getIvarLayout(mClass.mClass);
		orkprintf( " ivarlayout<%s>\n", ivarlayout );
		u_int iNumIvars = 0;
		Ivar* ivars = class_copyIvarList(mClass.mClass, &iNumIvars);
		orkprintf( " iNumIvars<%d>\n", iNumIvars );
		for( int i=0; i<iNumIvars; i++ )
		{
			Ivar v = ivars[i];
			const char* ivname = ivar_getName(v);
			id ivv = object_getIvar(mInstance, v);
			orkprintf( "   iv<%d:%s> %p\n", i, ivname, ivv );
			if( ivv )
			{
				Class cls = Class::FromObject(ivv);
				Object obj( cls, ivv );
				//obj.Dump();
			}
		}
		orkprintf( "//////////////////////////////////\n" );
		//exit(0);
		
	}


////////////////////////////////////////////////////
}}
////////////////////////////////////////////////////
#endif