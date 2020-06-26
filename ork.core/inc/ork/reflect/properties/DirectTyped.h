////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ITyped.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename T> class DirectTyped : public ITyped<T> {

  T Object::*mProperty;

public:
  DirectTyped(T Object::*);

  void get(T&, object_constptr_t) const override;
  void set(const T&, object_ptr_t) const override;
};

}} // namespace ork::reflect
