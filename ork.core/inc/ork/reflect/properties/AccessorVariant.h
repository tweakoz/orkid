////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "ObjectProperty.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

class AccessorVariant : public ObjectProperty {
public:
  AccessorVariant(
      bool (Object::*getter)(serdes::ISerializer&) const, //
      bool (Object::*setter)(serdes::IDeserializer&));

private:
  void deserialize(serdes::node_ptr_t) const override;
  void serialize(serdes::node_ptr_t) const override;
  bool (Object::*_serialize)(serdes::ISerializer&) const;
  bool (Object::*_deserialize)(serdes::IDeserializer&);
};

}} // namespace ork::reflect
