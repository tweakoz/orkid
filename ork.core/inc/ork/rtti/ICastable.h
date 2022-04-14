////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/config/config.h>
#include <memory>

namespace ork { namespace rtti {

struct Class;

struct ICastable {
  virtual ~ICastable() {
  }
  virtual Class* GetClass() const = 0;
  static Class* GetClassStatic();

  struct RTTITyped {
    typedef Class RTTICategory;
  };
};

}} // namespace ork::rtti
