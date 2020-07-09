////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ITypedArray.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename ArrayType> //
class DirectTypedArray : public ITypedArray<std::remove_reference_t<decltype(*std::begin(std::declval<ArrayType&>()))>> {
  using element_type                   = std::remove_reference_t<decltype(*std::begin(std::declval<ArrayType&>()))>;
  static constexpr size_t array_length = std::extent<ArrayType>::value;

public:
  DirectTypedArray(ArrayType(Object::*));
  void get(element_type&, object_constptr_t, size_t) const override;
  void set(const element_type&, object_ptr_t, size_t) const override;
  size_t count(object_constptr_t) const override;
  void resize(object_ptr_t obj, size_t size) const override;

private:
  ArrayType(Object::*_property);
  const size_t _size;
};

}} // namespace ork::reflect
