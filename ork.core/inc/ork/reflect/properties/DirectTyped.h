////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "ITyped.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename T> class DirectTyped : public ITyped<T> {

public:

  T Object::*_member;

  DirectTyped(T Object::*);

  void get(T&, object_constptr_t) const override;
  void set(const T&, object_ptr_t) const override;
};

}} // namespace ork::reflect
