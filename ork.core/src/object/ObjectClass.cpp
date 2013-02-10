////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/object/ObjectClass.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/IObjectProperty.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::object::ObjectClass, "ObjectClass");

namespace ork { namespace object {

static const reflect::Description *ParentClassDescription(const rtti::Class *clazz)
{
	const ObjectClass *object_class = rtti::downcast<const ObjectClass *>(clazz);

	if(object_class)
	{
		return &object_class->Description();
	}
	else
	{
		return NULL;
	}
}

void ObjectClass::Describe()
{
}

ObjectClass::ObjectClass(const rtti::RTTIData &data)
	: rtti::Class(data)
	, mDescription()
{
}

void ObjectClass::Initialize()
{
	Class::Initialize();
	mDescription.SetParentDescription(ParentClassDescription(Parent()));

	reflect::Description::PropertyMapType& propmap = mDescription.Properties();

	for( reflect::Description::PropertyMapType::iterator it=propmap.begin(); it!=propmap.end(); it++ )
	{
		ConstString name = it->first;
		reflect::IObjectProperty* prop = it->second;

		rtti::Class* propclass = prop->GetClass();

		propclass->SetName( name, false );

	}
}

reflect::Description &ObjectClass::Description()
{
	return mDescription;
}

const reflect::Description &ObjectClass::Description() const
{
	return mDescription;
}

} }

