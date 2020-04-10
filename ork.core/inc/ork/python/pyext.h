#pragma once

#include <ork/pch.h>
#include <ork/python/context.h>
#include <ork/python/wraprawpointer.inl>
#include <ork/kernel/varmap.inl>

ORK_PUSH_SYMVIZ_PUBLIC

namespace ork::python {

using decoderfn_t = std::function<void(const pybind11::object& inpval, ork::varmap::val_t& outval)>;

struct TypeCodec {
  //////////////////////////////////
  static std::shared_ptr<TypeCodec> instance();
  ork::varmap::val_t decode(const pybind11::object& val) const;
  void registerDecoder(const pybind11::object& pytype, decoderfn_t dfn);
  //////////////////////////////////
protected:
  TypeCodec();
  svar32_t _impl;
};

} // namespace ork::python

ORK_POP_SYMVIZ
