////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
