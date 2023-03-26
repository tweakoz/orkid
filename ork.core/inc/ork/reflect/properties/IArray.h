////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
  IArray() : ObjectProperty() {
  }
};

}} // namespace ork::reflect
