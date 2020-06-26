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

namespace ork { namespace reflect {

class DirectObject : public IObject {
  object_ptr_t Object::*mProperty;

public:
  DirectObject(object_ptr_t Object::*);

  void get(object_ptr_t& value, object_constptr_t instance) const;
  void set(object_ptr_t const& value, object_ptr_t instance) const;

  object_ptr_t access(object_ptr_t) const override;
  object_constptr_t access(object_constptr_t) const override;

  void deserialize(IDeserializer&, object_ptr_t) const override;
  void serialize(ISerializer&, object_constptr_t) const override;
};

}} // namespace ork::reflect
