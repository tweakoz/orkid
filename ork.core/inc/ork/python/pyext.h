#pragma once

#include <ork/pch.h>
#include <ork/python/context.h>
#include <ork/python/wraprawpointer.inl>
#include <ork/kernel/varmap.inl>

ORK_PUSH_SYMVIZ_PUBLIC

namespace ork::python {

using decoderfn_t = std::function<void(const pybind11::object& inpval, ork::varmap::val_t& outval)>;
using encoderfn_t = std::function<void(const ork::varmap::val_t& inpval, pybind11::object& outval)>;

struct TypeCodec {
  //////////////////////////////////
  static std::shared_ptr<TypeCodec> instance();
  pybind11::object encode(const ork::varmap::val_t& val) const;
  ork::varmap::val_t decode(const pybind11::object& val) const;
  void registerCodec(
      const pybind11::object& pytype, //
      const ork::TypeId& orktypeid,
      encoderfn_t efn,
      decoderfn_t dfn);
  //////////////////////////////////
  // register std codec (will reduce boilerplate for a lot of cases)
  //////////////////////////////////
  template <typename T> void registerStdCodec(const pybind11::object& pytype) {
    this->registerCodec(
        pytype, //
        TypeId::of<T>(),
        [](const ork::varmap::val_t& inpval, pybind11::object& outval) { // encoder
          outval = pybind11::cast(inpval.Get<T>());
        },
        [](const pybind11::object& inpval, ork::varmap::val_t& outval) { // decoder
          auto tek = inpval.cast<T>();
          outval.Set<T>(tek);
        });
  }
  //////////////////////////////////
protected:
  TypeCodec();
  svar64_t _impl;
};

} // namespace ork::python

ORK_POP_SYMVIZ
