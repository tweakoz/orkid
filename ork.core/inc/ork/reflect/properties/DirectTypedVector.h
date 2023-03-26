////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once
////////////////////////////////////////////////////////////////
#include <ork/config/config.h>
#include "ITypedArray.h"
////////////////////////////////////////////////////////////////
namespace ork::reflect {
////////////////////////////////////////////////////////////////
template <typename VectorType> //
struct DirectTypedVector       //
    : public ITypedArray<typename VectorType::value_type> {

  using value_type = typename VectorType::value_type;

  DirectTypedVector(VectorType Object::*);

  void get(value_type&, object_constptr_t, size_t) const override;
  void set(const value_type&, object_ptr_t, size_t) const override;
  size_t count(object_constptr_t) const override;
  void resize(object_ptr_t, size_t) const override;

  VectorType Object::*_member;
};
////////////////////////////////////////////////////////////////
} // namespace ork::reflect
