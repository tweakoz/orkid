////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/properties/I.h>
#include <ork/reflect/properties/ITyped.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

//#define DECLARE_TRANSPARENT_CASTABLE(ClassType, BaseType) \
//DECLARE_TRANSPARENT_CASTABLE_INTERNAL(ClassType, RTTI_2_ARG__(::ork::rtti::Castable<ClassType, BaseType>))

//#define DECLARE_TRANSPARENT_CASTABLE_INTERNAL(ClassType, RTTIImplementation) \
//public: \
//	typedef RTTIImplementation RTTITyped; \
//	static RTTIImplementation::RTTIClassClass *GetClassStatic(); \
//	virtual ::ork::rtti::Class *GetClass() const; \
//private: \
//	static RTTIImplementation::RTTIClassClass sClass;

template <typename T> class ITyped : public ObjectProperty {
  DECLARE_TRANSPARENT_TEMPLATE_CASTABLE(ITyped, ObjectProperty)

  // static ork::rtti::Class* GetClassStatic(); // Kill inherited GetClassStatic()
public:
  virtual void Get(T& value, const Object* obj) const = 0;
  virtual void Set(const T& value, Object* obj) const = 0;

private:
  /*virtual*/ bool Deserialize(IDeserializer&, Object*) const;
  /*virtual*/ bool Serialize(ISerializer&, const Object*) const;

protected:
  ITyped() {
  }
};

}} // namespace ork::reflect
