////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/python/obind/nanobind.h>
#include <atomic>
#include <memory>

namespace ork::python {

///////////////////////////////////////////////////////////////////////////////
// liveness of the obind-side (ecssim sub-)interpreter — flipped false by
// ~Context2 immediately BEFORE Py_EndInterpreter (and true again when a new
// sub-interpreter context comes up). Py_IsInitialized() alone cannot catch this
// case: the MAIN interpreter is still alive when the ecssim sub-interpreter dies.
///////////////////////////////////////////////////////////////////////////////
std::atomic<bool>& obind_interpreter_alive();

///////////////////////////////////////////////////////////////////////////////
// GIL-safe wrapper for obind (nanobind-fork) objects — the obind sibling of
// gil_safe_pyobj (which is pybind11-flavored and cannot hold obind handles).
//
// STORE THIS, never a raw obind::object, in variants/varmaps whose destruction
// can outlive the interpreter or run without the GIL. The motivating crash
// (cmd-q, 2026-07-02): ~Simulation's dtor BODY deletes its systems (PythonSystem
// -> ~Context2 -> Py_EndInterpreter), then MEMBER destruction releases _vars,
// whose svar128 entries held raw obind::objects from the pycodec decode fallback
// -> Py_DECREF into freed interpreter state -> _Py_Dealloc EXC_BAD_ACCESS.
//
// Copy/move are shared_ptr refcount ops (GIL-free). Only the final reference's
// destruction acts: interpreter alive -> acquire the (reentrant,
// sub-interpreter-safe) GIL and decref; interpreter gone -> deliberately LEAK
// the handle (UB-free beats leak-free during teardown).
///////////////////////////////////////////////////////////////////////////////
struct gil_safe_obind {

  std::shared_ptr<obind::object> _obj;

  gil_safe_obind() = default;

  explicit gil_safe_obind(obind::object obj) {
    _obj = std::shared_ptr<obind::object>( //
        new obind::object(std::move(obj)), //
        [](obind::object* p) {
          if (obind_interpreter_alive().load() and Py_IsInitialized()) {
            obind::gil_scoped_acquire gil;
            delete p;
          } else {
            (void)p->release(); // null the handle so ~object skips the decref
            delete p;
          }
        });
  }

  // Copy/move all default — just shared_ptr refcount ops, no GIL needed
  gil_safe_obind(const gil_safe_obind&)            = default;
  gil_safe_obind(gil_safe_obind&&)                 = default;
  gil_safe_obind& operator=(const gil_safe_obind&) = default;
  gil_safe_obind& operator=(gil_safe_obind&&)      = default;
  ~gil_safe_obind()                                = default;

  // caller must hold the GIL (normal codec-encode paths do)
  obind::object ref() const {
    return _obj ? *_obj : obind::object();
  }

  explicit operator bool() const {
    return _obj && _obj->ptr() != nullptr;
  }
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::python
