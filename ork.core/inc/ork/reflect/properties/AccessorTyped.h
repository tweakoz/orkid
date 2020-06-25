////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/IObjectPropertyType.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename T> class AccessorTyped : public IObjectPropertyType<T> {
public:
  AccessorTyped(void (Object::*getter)(T&) const, void (Object::*setter)(const T&));

  void Get(T& value, const Object* obj) const override;
  void Set(const T& value, Object* obj) const override;

private:
  void (Object::*mGetter)(T&) const;
  void (Object::*mSetter)(const T&);
};

}} // namespace ork::reflect
