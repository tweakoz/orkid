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

template <typename VectorType> class DirectTypedVector : public ITypedArray<typename VectorType::value_type> {
public:
  typedef typename VectorType::value_type ValueType;

  DirectTypedVector(VectorType Object::*);

private:
  void get(ValueType&, const Object*, size_t) const override;
  void set(const ValueType&, Object*, size_t) const override;
  size_t count(const Object*) const override;
  void resize(Object*, size_t) const override;

  VectorType Object::*mProperty;
};

} // namespace ork::reflect
