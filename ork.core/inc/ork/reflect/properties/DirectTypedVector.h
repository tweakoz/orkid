////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ITypedArray.h"

#include <ork/config/config.h>

namespace ork::reflect {

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

} // namespace ork::reflect
