////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "DirectTypedVector.h"
#include "ITypedArray.hpp"

namespace ork { namespace reflect {

template <typename VectorType> //
DirectTypedVector<VectorType>::DirectTypedVector(VectorType Object::*prop)
    : _member(prop) {
  OrkAssert(_member != nullptr);
}

template <typename VectorType> //
void DirectTypedVector<VectorType>::get(
    value_type& value, //
    object_constptr_t instance,
    size_t index) const {
  value = (instance.get()->*_member)[index];
}

template <typename VectorType> //
void DirectTypedVector<VectorType>::set(
    const value_type& value, //
    object_ptr_t instance,
    size_t index) const {
  (instance.get()->*_member)[index] = value;
}

template <typename VectorType> //
size_t DirectTypedVector<VectorType>::count(object_constptr_t instance) const {
  return size_t((instance.get()->*_member).size());
}

template <typename VectorType> //
void DirectTypedVector<VectorType>::resize(
    object_ptr_t instance, //
    size_t size) const {
  (instance.get()->*_member).resize(size);
}

}} // namespace ork::reflect
