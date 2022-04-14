////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ITypedArray.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename VectorType> //
class DirectObjectVector       //
    : public ITypedArray< object_ptr_t> {
public:

  using value_type = typename VectorType::value_type;

  DirectObjectVector(VectorType Object::*);

  void get(object_ptr_t&element, object_constptr_t owner, size_t index) const override;
  void set(const object_ptr_t&element, object_ptr_t owner, size_t index) const override;
  size_t count(object_constptr_t owner) const override;
  void resize(object_ptr_t owner, size_t newcount) const override;

  VectorType Object::*_member;

protected:

};

}} // namespace ork::reflect
