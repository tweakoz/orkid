////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "ObjectProperty.h"
#include <ork/config/config.h>

namespace ork::reflect {

namespace serdes{
  struct EnumValue;
  struct EnumType;
  using enumvalue_ptr_t = std::shared_ptr<EnumValue>;
  using enumtype_ptr_t = std::shared_ptr<EnumType>;
}

using enum_abs_array_t = std::vector<serdes::enumvalue_ptr_t>;

class DirectEnumBase : public ObjectProperty {

public:
  // abstract interface
  virtual enum_abs_array_t enumerateEnumerations(object_constptr_t obj) const = 0;
  virtual void setFromString( object_ptr_t obj, const std::string& str ) const = 0;
  virtual std::string toString( object_constptr_t obj ) const = 0;
};

template <typename T> class DirectEnum : public DirectEnumBase {

public:

  T Object::*_member;

  DirectEnum(T Object::*);

  void get(T&, object_constptr_t) const;
  void set(const T&, object_ptr_t) const;
  void deserialize(serdes::node_ptr_t) const final;
  void serialize(serdes::node_ptr_t) const final;

  // abstract interface
  enum_abs_array_t enumerateEnumerations(object_constptr_t obj) const final;
  void setFromString( object_ptr_t obj, const std::string& str ) const final;
  std::string toString( object_constptr_t obj ) const final;

};

} // namespace ork::reflect
