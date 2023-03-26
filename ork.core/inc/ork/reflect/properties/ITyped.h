////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "ObjectProperty.h"
#include "ITyped.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename T> class ITyped : public ObjectProperty {

public:
  virtual void get(T& value, object_constptr_t obj) const  = 0;
  virtual void set(const T& value, object_ptr_t obj) const = 0;

private:
  void deserialize(serdes::node_ptr_t) const override;
  void serialize(serdes::node_ptr_t) const override;

protected:
  ITyped() {
  }
};

}} // namespace ork::reflect
