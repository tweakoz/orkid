////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "DirectObjectVector.h"
#include "ITypedArray.hpp"

namespace ork { namespace reflect {

template <typename VectorType> //
DirectObjectVector<VectorType>::DirectObjectVector(VectorType Object::*prop)
    : _member(prop) {
  OrkAssert(_member != nullptr);
}

template <typename VectorType> //
void DirectObjectVector<VectorType>::get(
    object_ptr_t& element, //
    object_constptr_t instance,
    size_t index) const {
    auto raw_get = (instance.get()->*_member)[index];
  auto typed_element = std::dynamic_pointer_cast<Object>(raw_get);
  element = typed_element;
}

template <typename VectorType> //
void DirectObjectVector<VectorType>::set(
    const object_ptr_t& element, //
    object_ptr_t instance,
    size_t index) const {
  auto& vect  = (instance.get()->*_member);
  using elem_t = typename value_type::element_type;
  auto typed_element = std::dynamic_pointer_cast<elem_t>(element);
  vect[index] = typed_element;
}

template <typename VectorType> //
size_t DirectObjectVector<VectorType>::count(object_constptr_t instance) const {
  auto& vect = (instance.get()->*_member);
  return size_t(vect.size());
}

template <typename VectorType> //
void DirectObjectVector<VectorType>::resize(
    object_ptr_t instance, //
    size_t size) const {
  auto& vect = (instance.get()->*_member);
  vect.resize(size);
}

}} // namespace ork::reflect
