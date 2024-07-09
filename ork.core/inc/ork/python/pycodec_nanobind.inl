////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/python/pycodec.h>

namespace ork::python {

////////////////////////////////////////////////////////////////

template <typename T> T nanobindadapter::_cast2ork(const object_t& obj) {
  return obind::cast<T>(obj);
}
//////////////////////////////////
template <typename T> obind::object nanobindadapter::handle2object(const T& obj) {
  return obind::cast<object_t>(obj);
}
template <typename T> obind::object nanobindadapter::cast_to_pyobject(const T& obj) {
  return obind::cast<object_t>(obj);
}
template <typename T> obind::object nanobindadapter::cast_to_pyhandle(const T& obj) {
  return obind::cast<handle_t>(obj);
}
template <typename T> void nanobindadapter::cast_to_var(const obind::object& inpval, varval_t& outval) {
  auto ork_val = _cast2ork<T>(inpval);
  outval.set<T>(ork_val);
}
template <typename T> void nanobindadapter::cast_to_v64(const obind::object& inpval, svar64_t& outval) {
  auto ork_val = _cast2ork<T>(inpval);
  outval.set<T>(ork_val);
}

template <typename T> obind::object nanobindadapter::cast_var_to_py(const varval_t& var) {
  return obind::cast(var.get<T>());
}
template <typename T> obind::object nanobindadapter::cast_v64_to_py(const svar64_t& v64) {
  return obind::cast(v64.get<T>());
}
template <typename T> bool nanobindadapter::isinstance(const object_t& obj) {
  return obind::isinstance<T>(obj);
}

template <typename... Args> auto nanobindadapter::init(Args&&... args) {
  return obind::init(std::forward<Args>(args)...);
}

////////////////////////////////////////////////////////////////
} // namespace ork::python
