////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "pycodec_pybind11.inl"
#include "pycodec_nanobind.inl"

///////////////////////////////////////////////////////////////////////////////
namespace ork::python {
///////////////////////////////////////////////////////////////////////////////

template <typename ADAPTER, typename... Args>
auto __cast(Args&&... args) -> decltype(ADAPTER::template _cast(std::forward<Args>(args)...)) {
    return ADAPTER::template _cast(std::forward<Args>(args)...);
}
template <typename ADAPTER, typename... Args>
auto __borrow(Args&&... args) -> decltype(ADAPTER::template _borrow(std::forward<Args>(args)...)) {
    return ADAPTER::template _borrow(std::forward<Args>(args)...);
}

template <typename ADAPTER> template <typename ORKTYPE> void TypeCodec<ADAPTER>::registerStdCodec(const object_t& pytype) {
  this->registerCodec(
      pytype, //
      TypeId::of<ORKTYPE>(),
      [](const varval_t& inpval, auto& outval) { // encoder
        outval = ADAPTER::template cast_var_to_py<ORKTYPE>(inpval);
      },
      [](const auto& inpval, varval_t& outval) { // decoder
        ADAPTER::template cast_to_var<ORKTYPE>(inpval, outval);
      });
  this->registerCodec64(
      pytype, //
      TypeId::of<ORKTYPE>(),
      [](const svar64_t& inpval, auto& outval) { // encoder
        outval = ADAPTER::template cast_v64_to_py<ORKTYPE>(inpval);
      },
      [](const auto& inpval, svar64_t& outval) { // decoder
        ADAPTER::template cast_to_v64<ORKTYPE>(inpval, outval);
      });
}

///////////////////////////////////////////////////////////////////////////////

template <typename ADAPTER>
template <typename ORKTYPE>
void TypeCodec<ADAPTER>::registerStdCodecBIG(const object_t& pytype) {
  this->registerCodec(
      pytype, //
      TypeId::of<ORKTYPE>(),
      [](const varval_t& inpval, auto& outval) { // encoder
        outval = ADAPTER::template cast_var_to_py<ORKTYPE>(inpval);
      },
      [](const auto& inpval, varval_t& outval) { // decoder
        ADAPTER::template cast_to_var<ORKTYPE>(inpval, outval);
      });
}

///////////////////////////////////////////////////////////////////////////////

template <typename ADAPTER>
template <typename PYREPR, typename ORKTYPE> //
void TypeCodec<ADAPTER>::registerRawPtrCodec(const object_t& pytype) {
  this->registerCodec(
      pytype, //
      TypeId::of<ORKTYPE>(),
      [](const varval_t& inpval, auto& outval) { // encoder
        auto rawval = inpval.get<ORKTYPE>();
        auto pyrepr = PYREPR(rawval);
        // auto h = ADAPTER::template cast_to_pyobject(pyrepr);
        auto h = __cast<ADAPTER>(pyrepr);
        outval = h; // ADAPTER::cast_to_pyobject(h);
      },
      [](const auto& inpval, varval_t& outval) { // decoder
        auto intermediate_val = inpval.template cast<PYREPR>();
        auto ptr_val          = intermediate_val.get();
        outval.set<ORKTYPE>(ptr_val);
      });
  this->registerCodec64(
      pytype, //
      TypeId::of<ORKTYPE>(),
      [](const svar64_t& inpval, auto& outval) { // encoder
        auto rawval = inpval.get<ORKTYPE>();
        auto pyrepr = PYREPR(rawval);
        // auto h = ADAPTER::template cast_to_pyobject<PYREPR>(pyrepr);
        auto h = __cast<ADAPTER>(pyrepr);
        outval = h; // ADAPTER::cast_to_pyobject(h);
      },
      [](const auto& inpval, svar64_t& outval) { // decoder
        auto intermediate_val = inpval.template cast<PYREPR>();
        auto ptr_val          = intermediate_val.get();
        outval.set<ORKTYPE>(ptr_val);
      });
}

///////////////////////////////////////////////////////////////////////////////

template <typename ADAPTER> varmap::VarMap TypeCodec<ADAPTER>::decode_kwargs(typename ADAPTER::kwargs_t kwargs) { //
  auto from_py = ADAPTER::decodeKwArgs(kwargs);
  varmap::VarMap rval;
  for (auto item : from_py) {
    auto key = item.first;
    auto obj = item.second;
    auto val = this->decode(obj);
    rval.setValueForKey(key, val);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename ADAPTER> std::vector<varval_t> TypeCodec<ADAPTER>::decodeList(typename ADAPTER::list_t py_args) {
  auto as_objlist = ADAPTER::decodeList(py_args);
  std::vector<varval_t> decoded_list;
  for (auto list_item : as_objlist) {
    varval_t val = this->decode(list_item);
    decoded_list.push_back(val);
  }
  return decoded_list;
}

///////////////////////////////////////////////////////////////////////////////

template <typename adapter, typename type_, typename... options, typename... Extra>
auto clazz(typename adapter::module_t& scope, const char* name, const Extra&... extra) {
  return adapter::template clazz<type_, options...>(scope, name, extra...);
}

template <typename adapter, typename... Args, typename... Extra>
auto initor(const typename adapter::initimpl::template constructor<>& init, const Extra&... extra) {
  return adapter::template init<Args...>();
}
template <typename adapter, typename... Extra> auto initor(const Extra&... extra) {
  return adapter::template init<>();
}

template <typename adapter, typename T> bool is_instance(const auto& inpval) {
  return adapter::template isinstance<T>(inpval);
}
template <typename adapter> inline bool is_instance_pystr(const auto& inpval) {
  return adapter::template isinstance<typename adapter::str_t>(inpval);
}
template <typename adapter> inline bool is_instance_pyint(const auto& inpval) {
  return adapter::template isinstance<typename adapter::int_t>(inpval);
}
template <typename adapter> inline bool is_instance_pyfloat(const auto& inpval) {
  return adapter::template isinstance<typename adapter::float_t>(inpval);
}

template <typename adapter, typename T> T cast2ork(const auto& obj) {
  return adapter::template _cast2ork<T>(obj);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::python

///////////////////////////////////////////////////////////////////////////////

namespace pybind11 {
template <typename type_, typename... options, typename... Extra>
auto clazzz(module_& scope, const char* name, const Extra&... extra) {
  return ork::python::pybind11adapter::template clazz<type_, options...>(scope, name, extra...);
}
template <typename type_, typename... options, typename... Extra>
auto clazz_bufp(module_& scope, const char* name, const Extra&... extra) {
  return ork::python::pybind11adapter::template clazz<type_, options...>(scope, name, pybind11::buffer_protocol(), extra...);
}
}
namespace obind {
template <typename type_, typename... options, typename... Extra>
auto clazzz(module_& scope, const char* name, const Extra&... extra) {
  return ork::python::nanobindadapter::template clazz<type_, options...>(scope, name, extra...);
}
template <typename type_, typename... options, typename... Extra>
auto clazz_bufp(module_& scope, const char* name, const Extra&... extra) {
  return ork::python::nanobindadapter::template clazz<type_, options...>(scope, name, extra...);
}
}
