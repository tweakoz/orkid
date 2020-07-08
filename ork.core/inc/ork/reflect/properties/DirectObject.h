#pragma once
////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "IObject.h"

#include <ork/config/config.h>

namespace ork::reflect {

template <typename MemberType> //
class DirectObject : public ObjectProperty {

  using sharedptrtype_t      = MemberType;
  using rawptrtype_t         = typename MemberType::element_type;
  using sharedconstptrtype_t = typename std::shared_ptr<const rawptrtype_t>;

  sharedptrtype_t Object::*_member;

public:
  DirectObject(sharedptrtype_t Object::*);

  void get(sharedptrtype_t& value, object_constptr_t instance) const;
  void set(const sharedptrtype_t& value, object_ptr_t instance) const;

  sharedptrtype_t access(object_ptr_t) const;
  sharedconstptrtype_t access(object_constptr_t) const;

  void deserialize(serdes::node_ptr_t) const override;
  void serialize(serdes::node_ptr_t) const override;
};

} // namespace ork::reflect
