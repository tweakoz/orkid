////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/python/context.h>
#include <memory>

namespace ork::python {

///////////////////////////////////////////////////////////////////////////////
// GIL-safe wrapper for pybind11 Python objects.
//
// Stores a py::object via shared_ptr with a custom deleter that acquires
// the GIL before calling Py_DECREF. This ensures safe destruction from
// any thread — including C++ destructor chains that don't hold the GIL.
//
// Copy/move are GIL-free (shared_ptr refcount ops only).
// Only the final reference's destruction acquires the GIL.
//
// Usage:
//   auto safe = gil_safe_pyobj(some_py_callable);
//   member_callback = [safe]() {
//     py::gil_scoped_acquire acquire;
//     auto fn = safe.valueAs<py::function>();
//     (*fn)(args...);
//   };
///////////////////////////////////////////////////////////////////////////////

struct gil_safe_pyobj {
  std::shared_ptr<pybind11::object> _obj;

  gil_safe_pyobj() = default;

  explicit gil_safe_pyobj(pybind11::object obj) {
    _obj = std::shared_ptr<pybind11::object>(
      new pybind11::object(std::move(obj)),
      [](pybind11::object* p) {
        if (Py_IsInitialized()) {
          pybind11::gil_scoped_acquire gil;
          delete p;
        }
        // else: interpreter shutting down — leak to avoid UB from Py_DECREF
      }
    );
  }

  // Copy/move all default — just shared_ptr refcount ops, no GIL needed
  gil_safe_pyobj(const gil_safe_pyobj&)            = default;
  gil_safe_pyobj(gil_safe_pyobj&&)                 = default;
  gil_safe_pyobj& operator=(const gil_safe_pyobj&) = default;
  gil_safe_pyobj& operator=(gil_safe_pyobj&&)      = default;
  ~gil_safe_pyobj()                                = default;

  template <typename T>
  std::shared_ptr<T> valueAs() const {
    return std::static_pointer_cast<T>(_obj);
  }

  explicit operator bool() const { return _obj && _obj->ptr() != nullptr; }
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::python
