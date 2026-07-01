/*
    nanobind/nb_misc.h: Miscellaneous bits (GIL, etc.)

    Copyright (c) 2022 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

NAMESPACE_BEGIN(NB_NAMESPACE)

struct gil_scoped_acquire {
public:
    // REENTRANT / sub-interpreter-safe. If a thread state is already current on
    // this OS thread, the GIL is held and there is nothing to do — and we MUST NOT
    // call PyGILState_Ensure(): it is main-interpreter bound, so under a
    // sub-interpreter on the main thread it would try to attach the main-interp
    // gilstate thread-state over the current sub tstate → CPython
    // "_PyThreadState_Attach: non-NULL old thread state" fatal. (This is the exact
    // py_deleter case — a python-owning shared_ptr destructing while the sub-interp
    // is bound on the render thread.) Only Ensure when truly GIL-less. The stock
    // obind copy unconditionally Ensure'd; the real nanobind acquire is reentrant.
    gil_scoped_acquire() noexcept {
        if (_PyThreadState_UncheckedGet() == nullptr) {
            state     = PyGILState_Ensure();
            _acquired = true;
        }
    }
    ~gil_scoped_acquire() {
        if (_acquired)
            PyGILState_Release(state);
    }
    gil_scoped_acquire(const gil_scoped_acquire &) = delete;
    gil_scoped_acquire& operator=(const gil_scoped_acquire &) = delete;

private:
    PyGILState_STATE state{};
    bool             _acquired = false;
};

class gil_scoped_release {
public:
    gil_scoped_release() noexcept : state(PyEval_SaveThread()) { }
    ~gil_scoped_release() { PyEval_RestoreThread(state); }
    gil_scoped_release(const gil_scoped_release &) = delete;
    gil_scoped_release& operator=(const gil_scoped_release &) = delete;

private:
    PyThreadState *state;
};

inline void set_leak_warnings(bool value) noexcept {
    detail::set_leak_warnings(value);
}

inline void set_implicit_cast_warnings(bool value) noexcept {
    detail::set_implicit_cast_warnings(value);
}

inline dict globals() {
    PyObject *p = PyEval_GetGlobals();
    if (!p)
        raise("obind::globals(): no frame is currently executing!");
    return borrow<dict>(p);
}

inline bool is_alive() noexcept {
    return detail::is_alive();
}

NAMESPACE_END(NB_NAMESPACE)
