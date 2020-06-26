#pragma once
////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ISharedObject.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

class DirectSharedObject : public ISharedObject {
  object_ptr_t Object::*mProperty;

public:
  DirectSharedObject(object_ptr_t Object::*);

  void get(object_ptr_t& value, const Object* obj) const;
  void set(object_ptr_t const& value, Object* obj) const;

  object_ptr_t Access(Object*) const override;
  object_constptr_t Access(const Object*) const override;

  void deserialize(IDeserializer&, object_ptr_t) const override;
  void serialize(ISerializer&, object_constptr_t) const override;
};

}} // namespace ork::reflect
