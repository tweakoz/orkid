////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
