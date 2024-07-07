////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/pch.h>
#include <ork/python/context.h>
#include <ork/python/wraprawpointer.inl>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/mutex.h>
#include <iostream>

namespace ork::python {

using varval_t    = varmap::var_t;
using decoderfn_t = std::function<void(const pybind11::object& inpval, varval_t& outval)>;
using encoderfn_t = std::function<void(const varval_t& inpval, pybind11::object& outval)>;

using decoderfn64_t = std::function<void(const pybind11::object& inpval, svar64_t& outval)>;
using encoderfn64_t = std::function<void(const svar64_t& inpval, pybind11::object& outval)>;

#define OrkPyAssert(x)                                                                                                             \
  {                                                                                                                                \
    if ((x) == 0) {                                                                                                                \
      char buffer[1024];                                                                                                           \
      snprintf(buffer, sizeof(buffer), "Assert At: [File %s] [Line %d] [Reason: Assertion %s failed]", __FILE__, __LINE__, #x);    \
      try {                                                                                                                        \
        py::object traceback       = py::module::import("traceback");                                                              \
        py::object formatted_stack = traceback.attr("format_stack")();                                                             \
        for (const auto& frame : formatted_stack) {                                                                                \
          std::cout << py::str(frame);                                                                                             \
        }                                                                                                                          \
      } catch (const py::error_already_set& e) {                                                                                   \
        std::cerr << "Failed to print Python call stack: " << e.what() << std::endl;                                               \
      }                                                                                                                            \
      OrkAssertFunction(&buffer[0]);                                                                                               \
    }                                                                                                                              \
  }

struct ORK_API TypeCodec {
  //////////////////////////////////
  ORK_API static std::shared_ptr<TypeCodec> instance();
  pybind11::object encode(const varval_t& val) const;
  pybind11::object encode64(const svar64_t& val) const;
  varval_t decode(const pybind11::object& val) const;
  svar64_t decode64(const pybind11::object& val) const;
  void registerCodec(
      const pybind11::object& pytype, //
      const ork::TypeId& orktypeid,
      encoderfn_t efn,
      decoderfn_t dfn);
  void registerCodec64(
      const pybind11::object& pytype, //
      const ork::TypeId& orktypeid,
      encoderfn64_t efn,
      decoderfn64_t dfn);
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
    this->registerCodec64(
        pytype, //
        TypeId::of<ORKTYPE>(),
        [](const svar64_t& inpval, pybind11::object& outval) { // encoder
          outval = pybind11::cast(inpval.get<ORKTYPE>());
        },
        [](const pybind11::object& inpval, svar64_t& outval) { // decoder
          auto ork_val = inpval.cast<ORKTYPE>();
          outval.set<ORKTYPE>(ork_val);
        });
  }
  template <typename ORKTYPE> void registerStdCodecBIG(const pybind11::object& pytype) {
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
    this->registerCodec64(
        pytype, //
        TypeId::of<ORKTYPE>(),
        [](const svar64_t& inpval, pybind11::object& outval) { // encoder
          auto rawval = inpval.get<ORKTYPE>();
          outval      = pybind11::cast(PYREPR(rawval));
        },
        [](const pybind11::object& inpval, svar64_t& outval) { // decoder
          auto intermediate_val = inpval.cast<PYREPR>();
          auto ptr_val          = intermediate_val.get();
          outval.set<ORKTYPE>(ptr_val);
        });
  }
  //////////////////////////////////
  inline varmap::VarMap decode_kwargs(pybind11::kwargs kwargs) { //
    varmap::VarMap rval;
    if (kwargs) {
      for (auto item : kwargs) {
        auto key = pybind11::cast<std::string>(item.first);
        auto obj = pybind11::cast<pybind11::object>(item.second);
        auto val = this->decode(obj);
        rval.setValueForKey(key, val);
      }
    }
    return rval;
  }
  //////////////////////////////////
  std::vector<varval_t> decodeList(pybind11::list py_args) {
    std::vector<varval_t> decoded_list;
    for (auto list_item : py_args) {
      auto item_val = pybind11::cast<pybind11::object>(list_item);
      varval_t val  = this->decode(item_val);
      decoded_list.push_back(val);
    }
    return decoded_list;
  }
  //////////////////////////////////
  template <typename T> void setProperty(std::string key, const T& value) {
    _props.atomicOp([key, value](varmap::VarMap& unlocked) { //
      unlocked.makeValueForKey<T>(key) = value; //
      });
  }
  template <typename T> attempt_cast_const<T> getProperty(std::string key) const {
    attempt_cast_const<T> rval(nullptr);
    _props.atomicOp([key, &rval](const varmap::VarMap& unlocked) { //
      auto attempt = unlocked.typedValueForKey<T>(key); //
      rval._data = attempt._data; //
      });
    return rval;
  }
  //////////////////////////////////
protected:
  TypeCodec();
  svar128_t _impl;
  LockedResource<varmap::VarMap> _props;
};

using typecodec_ptr_t = std::shared_ptr<TypeCodec>;

} // namespace ork::python
