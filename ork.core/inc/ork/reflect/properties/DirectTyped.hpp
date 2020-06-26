////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "DirectTyped.h"
#include "ITyped.hpp"

namespace ork { namespace reflect {

class ISerializer;

template <typename T>
DirectTyped<T>::DirectTyped(T Object::*property)
    : mProperty(property) {
}

template <typename T> void DirectTyped<T>::get(T& value, object_constptr_t obj) const {
  value = obj.get()->*mProperty;
}

template <typename T> void DirectTyped<T>::set(const T& value, object_ptr_t obj) const {
  obj.get()->*mProperty = value;
}

}} // namespace ork::reflect
