////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include "ObjectProperty.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

class IArray : public ObjectProperty {
public:
  virtual void deserializeElement(serdes::node_ptr_t) const = 0;
  virtual void serializeElement(serdes::node_ptr_t) const     = 0;
  virtual size_t count(object_constptr_t) const                    = 0;
  virtual void resize(object_ptr_t obj, size_t size) const         = 0;

private:
  void deserialize(serdes::node_ptr_t) const override;
  void serialize(serdes::node_ptr_t) const override;

protected:
  IArray() {
  }
};

}} // namespace ork::reflect
