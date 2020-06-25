////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORKTOOL_GED_GEDIO_H
#define _ORKTOOL_GED_GEDIO_H

#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/serialize/BinarySerializer.h>
#include <ork/reflect/serialize/BinaryDeserializer.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/reflect/properties/IMap.h>
#include <ork/reflect/properties/IArray.h>
#include <ork/reflect/properties/I.h>
#include <ork/reflect/properties/ITyped.h>
#include <ork/reflect/properties/DirectTyped.h>
#include <ork/rtti/downcast.h>

namespace ork { namespace tool { namespace ged {

///////////////////////////////////////////////////////////////////////////////

struct PropSetterObj 
{
	ork::Object*					mObject;
	const reflect::I*	mProperty;

	PropSetterObj( const reflect::I* prop, ork::Object* obj )
		: mObject( obj ), mProperty(prop)
	{
	}

	template <typename T> void SetValue( T val )
	{	const reflect::ITyped<T>* prop =
		rtti::safe_downcast<const reflect::ITyped<T>*>( mProperty );
		T curval;
		prop->Get( curval, mObject );
		if( curval != val )
		{
			prop->Set( val, mObject );
		}
		ObjectGedEditEvent ev;
		ev.mProperty = mProperty;
		mObject->Notify(&ev);
	}
	template <typename T> void GetValue( T& outval )
	{	const reflect::ITyped<T>* prop =
		 rtti::safe_downcast<const reflect::ITyped<T>*>( mProperty );
		prop->Get( outval, mObject );
	}
};

///////////////////////////////////////////////////////////////////////////////

class IoDriverBase
{
	const reflect::I*			mprop;
	Object*									mobj;
	ObjModel&								mmodel;

public:

	ork::Object* GetObject() const { return mobj; }
	const ork::reflect::I* GetProp() const { return mprop; }
	ObjModel& GetModel() const { return mmodel; }
	IoDriverBase( ObjModel& Model, const reflect::I* prop, Object* obj );


};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class GedIoDriver : public IoDriverBase
{
	PropSetterObj	mpso;
public:
	GedIoDriver( ObjModel& Model, const reflect::I* prop, Object* obj )
		: IoDriverBase( Model, prop, obj )
		, mpso( prop, obj )
	{
	}
	void SetValue( const T& ps )
	{
		mpso.SetValue<T>( ps );
	}
	void GetValue( T& ps )
	{
		mpso.GetValue<T>(ps);
	}
};

///////////////////////////////////////////////////////////////////////////////

} } }

#endif
