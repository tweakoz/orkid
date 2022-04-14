////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ITyped.hpp"

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename class_t, typename member_t> //
class LambdaTyped : public ITyped<member_t> {
public:
  using mutable_ptr_t = class_t*;
  using const_ptr_t   = const class_t*;

  using getter_t = std::function<void(const_ptr_t, member_t&)>;
  using setter_t = std::function<void(mutable_ptr_t, const member_t&)>;

  LambdaTyped(getter_t getr, setter_t setr)
      : _getter(getr)
      , _setter(setr) {
  }

  inline void
  get(member_t& value_out, //
      object_constptr_t obj_inp) const override {
    auto typedobj = std::dynamic_pointer_cast<const class_t>(obj_inp);
    _getter(typedobj.get(), value_out);
  }
  inline void
  set(const member_t& value_inp, //
      object_ptr_t obj_out) const override {
    auto typedobj = std::dynamic_pointer_cast<class_t>(obj_out);
    _setter(typedobj.get(), value_inp);
  }

  getter_t _getter;
  setter_t _setter;
};

}} // namespace ork::reflect
