////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "DirectEnum.h"
//#include "ITyped.hpp"

////////////////////////////////////////////////
// commonly needed inline member types
//  (with custom serdes)
////////////////////////////////////////////////

//#include <ork/asset/Asset.inl>
//#include <ork/math/cvector2.hpp>
//#include <ork/math/cvector3.hpp>
//#include <ork/math/cvector4.hpp>
//#include <ork/math/quaternion.hpp>
//#include <ork/math/cmatrix3.hpp>
//#include <ork/math/cmatrix4.hpp>

////////////////////////////////////////////////

namespace ork::reflect {

template <typename T>
DirectEnum<T>::DirectEnum(T Object::*property)
    : _member(property) {
}

template <typename T> void DirectEnum<T>::get(T& value, object_constptr_t obj) const {
  value = obj.get()->*_member;
}

template <typename T> void DirectEnum<T>::set(const T& value, object_ptr_t obj) const {
  obj.get()->*_member = value;
}

template <typename T> void DirectEnum<T>::deserialize(serdes::node_ptr_t) const {
}
template <typename T> void DirectEnum<T>::serialize(serdes::node_ptr_t) const {
}


} // namespace ork::reflect
