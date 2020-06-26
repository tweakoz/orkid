////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ObjectProperty.h"
#include "ITyped.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename T> class ITyped : public ObjectProperty {

public:
  virtual void get(T& value, const Object* obj) const = 0;
  virtual void set(const T& value, Object* obj) const = 0;

private:
  void deserialize(IDeserializer&, Object*) const override;
  void serialize(ISerializer&, const Object*) const override;

protected:
  ITyped() {
  }
};

}} // namespace ork::reflect
