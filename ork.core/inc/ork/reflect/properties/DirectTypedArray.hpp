////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "DirectTypedArray.h"
#include "ITypedArray.hpp"

namespace ork { namespace reflect {

template <typename T>
DirectTypedArray<T>::DirectTypedArray(T (Object::*prop)[], size_t size)
    : mProperty(prop)
    , mSize(size) {
}

template <typename T> void DirectTypedArray<T>::Get(T& value, const Object* obj, size_t index) const {
  value = (obj->*mProperty)[index];
}

template <typename T> void DirectTypedArray<T>::Set(const T& value, Object* obj, size_t index) const {
  (obj->*mProperty)[index] = value;
}

template <typename T> size_t DirectTypedArray<T>::Count(const Object*) const {
  return mSize;
}

template <typename T> bool DirectTypedArray<T>::Resize(Object*, size_t size) const {
  return size == mSize;
}

}} // namespace ork::reflect
