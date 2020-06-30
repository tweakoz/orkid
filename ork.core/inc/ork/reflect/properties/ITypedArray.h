////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include "IArray.h"

namespace ork { namespace reflect {

template <typename T> class ITypedArray : public IArray {
public:
  ITypedArray() {
  }
  virtual void get(T& value, object_constptr_t obj, size_t index) const  = 0;
  virtual void set(const T& value, object_ptr_t obj, size_t index) const = 0;

private:
  void deserializeElement(serdes::node_ptr_t) const override;
  void serializeElement(serdes::node_ptr_t) const override;
};

}} // namespace ork::reflect
