////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/IObjectPropertyType.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

//#define DECLARE_TRANSPARENT_CASTABLE(ClassType, BaseType) \
//DECLARE_TRANSPARENT_CASTABLE_INTERNAL(ClassType, RTTI_2_ARG__(::ork::rtti::Castable<ClassType, BaseType>))

//#define DECLARE_TRANSPARENT_CASTABLE_INTERNAL(ClassType, RTTIImplementation) \
//public: \
//	typedef RTTIImplementation RTTIType; \
//	static RTTIImplementation::RTTIClassClass *GetClassStatic(); \
//	virtual ::ork::rtti::Class *GetClass() const; \
//private: \
//	static RTTIImplementation::RTTIClassClass sClass;

template<typename T>
class  IObjectPropertyType : public IObjectProperty
{
	DECLARE_TRANSPARENT_TEMPLATE_CASTABLE(IObjectPropertyType, IObjectProperty)

	//static ork::rtti::Class* GetClassStatic(); // Kill inherited GetClassStatic()
public:
    virtual void Get(T &value, const Object *obj) const = 0;
    virtual void Set(const T &value, Object *obj) const = 0;
private:
    /*virtual*/ bool Deserialize(IDeserializer &, Object *) const;
    /*virtual*/ bool Serialize(ISerializer &, const Object *) const;
protected:
	IObjectPropertyType() {}

};

} }

