#pragma once
////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "IObject.h"

#include <ork/config/config.h>

namespace ork::reflect {

struct DirectObjectBase : public ObjectProperty {

  virtual object_ptr_t getObject(object_constptr_t instance) const = 0;
  virtual void setObject(object_ptr_t instance, object_ptr_t obj) const = 0;
};


template <typename MemberType> //
class DirectObject : public DirectObjectBase {

  using element_type         = typename MemberType::element_type;
  using sharedptrtype_t      = MemberType;
  using sharedconstptrtype_t = typename std::shared_ptr<const element_type>;
  //using rawptrtype_t         = typename MemberType::element_type;

  sharedptrtype_t Object::*_member;

public:
  DirectObject(sharedptrtype_t Object::*);

  void get(sharedptrtype_t& value, object_constptr_t instance) const;
  void set(const sharedptrtype_t& value, object_ptr_t instance) const;

  object_ptr_t getObject(object_constptr_t instance) const final;
  void setObject(object_ptr_t instance, object_ptr_t obj) const final;

  sharedptrtype_t access(object_ptr_t) const;
  sharedconstptrtype_t access(object_constptr_t) const;

  void deserialize(serdes::node_ptr_t) const override;
  void serialize(serdes::node_ptr_t) const override;
};

} // namespace ork::reflect
