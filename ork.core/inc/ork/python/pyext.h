#pragma once

#include <ork/pch.h>
#include <ork/python/context.h>
#include <ork/python/wraprawpointer.inl>
#include <ork/kernel/varmap.inl>

ORK_PUSH_SYMVIZ_PUBLIC

namespace ork::python {

using varval_t    = ork::varmap::VarMap::value_type;
using decoderfn_t = std::function<void(const pybind11::object& inpval, varval_t& outval)>;
using encoderfn_t = std::function<void(const varval_t& inpval, pybind11::object& outval)>;

struct TypeCodec {
  //////////////////////////////////
  static std::shared_ptr<TypeCodec> instance();
  pybind11::object encode(const varval_t& val) const;
  varval_t decode(const pybind11::object& val) const;
  void registerCodec(
      const pybind11::object& pytype, //
      const ork::TypeId& orktypeid,
      encoderfn_t efn,
      decoderfn_t dfn);
  //////////////////////////////////
  // register std codec (will reduce boilerplate for a lot of cases)
  //////////////////////////////////
  template <typename ORKTYPE> void registerStdCodec(const pybind11::object& pytype) {
    this->registerCodec(
        pytype, //
        TypeId::of<ORKTYPE>(),
        [](const varval_t& inpval, pybind11::object& outval) { // encoder
          outval = pybind11::cast(inpval.get<ORKTYPE>());
        },
        [](const pybind11::object& inpval, varval_t& outval) { // decoder
          auto ork_val = inpval.cast<ORKTYPE>();
          outval.set<ORKTYPE>(ork_val);
        });
  }
  //////////////////////////////////
  // register std codec (will reduce boilerplate for a lot of cases)
  //////////////////////////////////
  template <typename PYREPR, typename ORKTYPE> void registerRawPtrCodec(const pybind11::object& pytype) {
    this->registerCodec(
        pytype, //
        TypeId::of<ORKTYPE>(),
        [](const varval_t& inpval, pybind11::object& outval) { // encoder
          auto rawval = inpval.get<ORKTYPE>();
          outval      = pybind11::cast(PYREPR(rawval));
        },
        [](const pybind11::object& inpval, varval_t& outval) { // decoder
          auto intermediate_val = inpval.cast<PYREPR>();
          auto ptr_val          = intermediate_val.get();
          outval.set<ORKTYPE>(ptr_val);
        });
  }
  //////////////////////////////////
protected:
  TypeCodec();
  svar64_t _impl;
};

} // namespace ork::python

ORK_POP_SYMVIZ
