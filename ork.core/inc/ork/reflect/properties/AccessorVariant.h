////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
