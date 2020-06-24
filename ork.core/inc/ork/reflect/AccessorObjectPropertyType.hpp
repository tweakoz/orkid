////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/AccessorObjectPropertyType.h>

namespace ork { namespace reflect {

template <typename T>
AccessorObjectPropertyType<T>::AccessorObjectPropertyType(void (Object::*getter)(T&) const, void (Object::*setter)(const T&))
    : mGetter(getter)
    , mSetter(setter) {
}

template <typename T> //
void AccessorObjectPropertyType<T>::Get(T& value, const Object* obj) const {
  (obj->*mGetter)(value);
}

template <typename T> //
void AccessorObjectPropertyType<T>::Set(const T& value, Object* obj) const {
  (obj->*mSetter)(value);
}

////////////////////////////////////////////////////////////////////////////////

}} // namespace ork::reflect
