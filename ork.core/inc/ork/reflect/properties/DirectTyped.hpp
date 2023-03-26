////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "DirectTyped.h"
#include "ITyped.hpp"

////////////////////////////////////////////////
// commonly needed inline member types
//  (with custom serdes)
////////////////////////////////////////////////

#include <ork/asset/Asset.inl>
#include <ork/math/cvector2.hpp>
#include <ork/math/cvector3.hpp>
#include <ork/math/cvector4.hpp>
#include <ork/math/quaternion.hpp>
#include <ork/math/cmatrix3.hpp>
#include <ork/math/cmatrix4.hpp>

////////////////////////////////////////////////

namespace ork { namespace reflect {

class ISerializer;

template <typename T>
DirectTyped<T>::DirectTyped(T Object::*property)
    : _member(property) {
}

template <typename T> void DirectTyped<T>::get(T& value, object_constptr_t obj) const {
  value = obj.get()->*_member;
}

template <typename T> void DirectTyped<T>::set(const T& value, object_ptr_t obj) const {
  obj.get()->*_member = value;
}

}} // namespace ork::reflect
