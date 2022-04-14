////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
#pragma once
////////////////////////////////////////////////////////////////
#include <ork/config/config.h>
#include "ITypedArray.h"
////////////////////////////////////////////////////////////////
namespace ork::reflect {
////////////////////////////////////////////////////////////////
template <typename ArrayType> //
struct array_size : std::extent<ArrayType> {};
////////////////////////////////////////////////////////////////
template <typename ArrayType, size_t N>     //
struct array_size<std::array<ArrayType, N>> //
    : std::tuple_size<std::array<ArrayType, N>> {};
////////////////////////////////////////////////////////////////
template <typename ArrayType> //
using array_element_type = std::remove_reference_t<decltype(*std::begin(std::declval<ArrayType&>()))>;
////////////////////////////////////////////////////////////////
template <typename ArrayType> //
class DirectTypedArray        //
    : public ITypedArray<array_element_type<ArrayType>> {
  using element_type                   = array_element_type<ArrayType>;
  static constexpr size_t array_length = array_size<ArrayType>::value;

public:
  DirectTypedArray(ArrayType(Object::*));
  void get(element_type&, object_constptr_t, size_t) const override;
  void set(const element_type&, object_ptr_t, size_t) const override;
  size_t count(object_constptr_t) const override;
  void resize(object_ptr_t obj, size_t size) const override;

private:
  ArrayType(Object::*_member);
  const size_t _size;
};
////////////////////////////////////////////////////////////////
} // namespace ork::reflect
