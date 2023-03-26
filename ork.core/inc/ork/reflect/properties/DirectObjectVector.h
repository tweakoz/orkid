////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
