---
name: orkcore-python
description: Answer questions about orkid's Python binding system, pybind11 integration, type codec, VarMap bindings, gil_safe_pyobj, factory patterns (wfactory/uifactory), and module initialization. Use when the user asks about Python bindings, pybind11 patterns, GIL safety, or type conversion.
user-invocable: false
---

# Orkid Python Binding System Reference

When answering questions about Python bindings in orkid, consult these files.

## Key Files

| Component | Location |
|-----------|----------|
| Type Codec Header | `ork.core/inc/ork/python/pycodec.h` |
| Type Codec Impl | `ork.core/inc/ork/python/pycodec.inl` |
| pybind11 Adapter | `ork.core/inc/ork/python/pycodec_pybind11.inl` |
| GIL-Safe Wrapper | `ork.core/inc/ork/python/gil_safe_pyobj.h` |
| Unmanaged Pointers | `ork.core/inc/ork/python/wraprawpointer.inl` |
| VarMap Bindings | `ork.core/inc/ork/python/common_bindings/pyext_varmap.inl` |
| Math Bindings | `ork.core/inc/ork/python/common_bindings/pyext_math_la.inl` |
| CrcString Bindings | `ork.core/inc/ork/python/common_bindings/pyext_crcstring.inl` |
| Core pyext.h | `ork.core/pyext/pyext.h` |
| Lev2 pyext.h | `ork.lev2/pyext/src/pyext.h` |
| Module Init (lev2) | `ork.lev2/pyext/src/pyext.cpp` |
| UI Bindings | `ork.lev2/pyext/src/pyext_ui.cpp` |
| Toolbar Bindings | `ork.lev2/pyext/src/pyext_ui_toolbar.cpp` |

## Architecture Overview

### Type Codec (`pb11_typecodec_t`)
Central type conversion between C++ and Python:
```cpp
auto type_codec = python::pb11_typecodec_t::instance();

// Encode C++ → Python
py::object py_val = type_codec->encode(svar128_value);

// Decode Python → C++
svar128_t cpp_val = type_codec->decode(py_object);

// Register custom type
type_codec->registerStdCodec<my_ptr_t>(my_pybind_type);
```

### Module Initialization Pattern
```cpp
void pyinit_my_module(py::module& parent_module) {
  auto type_codec = python::pb11_typecodec_t::instance();
  
  auto my_type = py::class_<MyClass, myclass_ptr_t>(parent_module, "MyClass")
    .def(py::init<>())
    .def("method", ...)
    .def_property("prop", getter, setter);
  
  type_codec->registerStdCodec<myclass_ptr_t>(my_type);
}
```

### Factory Patterns (Widget Creation)
Three standard factories on UI widget types:
```cpp
// wfactory — standalone widget (for overlays, manual composition)
.def_static("wfactory", [type_codec](py::list py_args) -> widget_ptr_t {
  auto decoded_args = type_codec->decodeList(py_args);
  auto name = decoded_args[0].get<std::string>();
  return std::make_shared<MyWidget>(name);
})

// uifactory — add to layout group (returns layout item)
.def_static("uifactory", [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t {
  auto decoded_args = type_codec->decodeList(py_args);
  auto name = decoded_args[0].get<std::string>();
  return lg->makeChild<MyWidget>(name).as_shared();
})
```

Python usage:
```python
widget = lev2.ui.MyWidget.wfactory(["name", arg2])
item = parent.makeChild(uiclass=lev2.ui.MyWidget, args=["name", arg2])
```

### GIL-Safe Python Object (`gil_safe_pyobj`)
For storing Python callbacks in C++ that may be destroyed from non-GIL threads:
```cpp
#include <ork/python/gil_safe_pyobj.h>

auto safe = python::gil_safe_pyobj(py_callback);

widget->_onSomething = [safe]() {
  py::gil_scoped_acquire acquire;
  auto fn = safe.valueAs<py::object>();
  (*fn)();
};
```
- Custom deleter acquires GIL before Py_DECREF
- Copy/move are GIL-free (shared_ptr refcount only)
- **Must use** for any callback stored in C++ that outlives the binding call

### VarMap Bindings
Dynamic attribute access via `__setattr__`/`__getattr__`:
```python
vm = core.VarMap()
vm.name = "hello"       # __setattr__ → setValueForKey
val = vm.name            # __getattr__ → valueForKey
"name" in vm             # __contains__
len(vm)                  # __len__
```
Keys with dots: `setattr(vm, "editor.filebase", "<assetcache>")`

### Unmanaged Pointers
For non-owning raw pointer wrapping (C++ objects with external lifetime):
```cpp
using ctx_t = ork::python::unmanaged_ptr<Context>;
using pyfxparam_ptr_t = ork::python::unmanaged_const_ptr<FxShaderParam>;
```

### Callback Binding Patterns

**Simple (unsafe for destruction):**
```cpp
.def("onCallback", [](ptr_t obj, py::object callback) {
  obj->_callback = [callback]() {  // UNSAFE: raw py::object capture
    py::gil_scoped_acquire acquire;
    callback();
  };
})
```

**Safe (for long-lived callbacks):**
```cpp
.def("onCallback", [](ptr_t obj, py::object callback) {
  auto safe = python::gil_safe_pyobj(callback);
  obj->_callback = [safe]() {
    py::gil_scoped_acquire acquire;
    auto fn = safe.valueAs<py::object>();
    (*fn)();
  };
})
```

### Blocking Operations (GIL Release)
```cpp
.def("fetch", [](catalog_ptr_t cat, const std::string& id) {
  py::gil_scoped_release release;  // Release GIL during C++ work
  return cat->fetch(id);
})
```

### Dual Adapter System (pybind11 + nanobind)

Orkid supports **two binding backends** via a templated adapter:

```cpp
// In ork.core/inc/ork/python/pycodec.h
struct pybind11adapter { ... };   // For main interpreter
struct nanobindadapter { ... };   // For subinterpreters

using pb11_typecodec_t = TypeCodec<pybind11adapter>;
using obind_typecodec_t = TypeCodec<nanobindadapter>;
```

**Why two:** pybind11 doesn't support `Py_mod_multiple_interpreters` (Python 3.12+ subinterpreter API). The ECS PythonSystem uses nanobind for simulation scripts running in a subinterpreter with its own GIL.

**Common bindings** use `template<typename ADAPTER>` to compile for both:
```cpp
// In ork.core/inc/ork/python/common_bindings/
template <typename ADAPTER>
void _init_varmap(ADAPTER::module_t& module, ADAPTER::codec_ptr_t codec);

// Called from pybind11 path:
_init_varmap<pybind11adapter>(module, pb11_codec);
// Called from nanobind path:
_init_varmap<nanobindadapter>(module, obind_codec);
```

**Adapter-agnostic class binding:**
```cpp
auto my_type = clazz<ADAPTER, MyClass, myclass_ptr_t>(module, "MyClass")
  .construct<>()
  .method("doThing", &MyClass::doThing)
  .prop_ro("name", [](myclass_ptr_t p) { return p->_name; })
  .prop_rw("value", getter_lambda, setter_lambda);
codec->registerStdCodec<myclass_ptr_t>(my_type);
```

**Key files:**
- `ork.core/inc/ork/python/pycodec.h` — adapter definitions
- `ork.core/inc/ork/python/pycodec_pybind11.inl` — pybind11 adapter impl
- `ork.core/inc/ork/python/pycodec_nanobind.inl` — nanobind adapter impl
- `ork.core/inc/ork/python/common_bindings/` — shared binding templates
- `ork.core/src/python/context.cpp` — Context2 subinterpreter management

**Subinterpreter module slot (nanobind only):**
```cpp
static PyModuleDef_Slot slots[] = {
    {Py_mod_exec, (void*)exec_module},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {0, nullptr}
};
```

See `orkecs-core` skill for full ECS dual-binding architecture details.

## How to Answer

1. For new bindings: follow the `pyinit_*` pattern, register with type_codec
2. For callbacks: always use `gil_safe_pyobj` if the callback can outlive the call
3. For blocking ops: release GIL with `py::gil_scoped_release`
4. For widget factories: implement both `wfactory` and `uifactory` statics
5. Check `pyext.h` for available type aliases and includes
6. For subinterpreter bindings: use nanobind adapter, declare `Py_MOD_PER_INTERPRETER_GIL_SUPPORTED`
7. For shared bindings: use `template<typename ADAPTER>` pattern from `common_bindings/`
