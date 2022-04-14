////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "AccessorTyped.h"

namespace ork { namespace reflect {

template <typename T>
AccessorTyped<T>::AccessorTyped(void (Object::*getter)(T&) const, void (Object::*setter)(const T&))
    : mGetter(getter)
    , mSetter(setter) {
}

template <typename T> //
void AccessorTyped<T>::get(T& value, object_constptr_t obj) const {
  (obj.get()->*mGetter)(value);
}

template <typename T> //
void AccessorTyped<T>::set(const T& value, object_ptr_t obj) const {
  (obj.get()->*mSetter)(value);
}

////////////////////////////////////////////////////////////////////////////////

}} // namespace ork::reflect
