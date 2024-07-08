////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/event.h>
#include <ork/ecs/ecs.h>
#include <ork/profiling.inl>
#include <iostream>
#include <nanobind/nanobind.h>

namespace nb = nanobind;

///////////////////////////////////////////////////////////////////////////////

namespace ork {
namespace python {
  void init_math(py::module& module_ecssim,python::pb11_typecodec_ptr_t type_codec);
}
}

namespace ork::ecssim {

void register_simulation(nb::module_& module_ecssim,python::pb11_typecodec_ptr_t type_codec);
void register_system(nb::module_& module_ecssim,python::pb11_typecodec_ptr_t type_codec);

} // namespace ork::ecs

////////////////////////////////////////////////////////////////////////////////

void _ecssim_init_classes(nb::module_ &module_ecssim) {
  //auto type_codec = ork::ecssim::simonly_codec_instance();
  auto type_codec = ork::python::pb11_typecodec_t::instance();

  //module_ecs.attr("__name__") = "ecs";
  //////////////////////////////////////////////////////////////////////////////
  //module_ecssim.doc() = "Orkid Ecs Internal (Simulation only) Library";
  //////////////////////////////////////////////////////////////////////////////
  //pyinit_entity(module_ecs);
  //pyinit_component(module_ecs);
  //pyinit_system(module_ecs);
  ::ork::ecssim::register_simulation(module_ecssim,type_codec);
  ::ork::ecssim::register_system(module_ecssim,type_codec);
  //::ork::python::init_math(module_ecssim, type_codec);


  //pyinit_pysys(module_ecs);
  //////////////////////////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////
/*NB_MODULE(_ecssim, m){
  _ecssim_init_classes(m);
}*/
#define NBX_MODULE(name, variable)                                              \
    static PyModuleDef NB_CONCAT(nanobind_module_def_, name);                  \
    [[maybe_unused]] static void NB_CONCAT(nanobind_init_,                     \
                                           name)(::nanobind::module_ &);       \
    NB_MODULE_IMPL(name) {                                                     \
        nanobind::detail::init(NB_DOMAIN_STR);                                 \
        nanobind::module_ m =                                                  \
            nanobind::steal<nanobind::module_>(nanobind::detail::module_new(   \
                NB_TOSTRING(name), &NB_CONCAT(nanobind_module_def_, name)));   \
        try {                                                                  \
            NB_CONCAT(nanobind_init_, name)(m);                                \
            return m.release().ptr();                                          \
        } catch (const std::exception &e) {                                    \
            PyErr_SetString(PyExc_ImportError, e.what());                      \
            return nullptr;                                                    \
        }                                                                      \
    }                                                                          \
    void NB_CONCAT(nanobind_init_, name)(::nanobind::module_ & (variable))

////////////////////////////////////////////////////////////////////////////////
int _ecssim_exec_module(PyObject *m) {
    try {
        nb::detail::init(NB_DOMAIN_STR);                                 \
        nb::module_ mod = nb::borrow<nb::module_>(m);
        _ecssim_init_classes(mod);
    } catch (const std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return -1;
    }
    return 0;
}
static PyModuleDef_Slot _ecssim_slots[] = {
    {Py_mod_exec, (void*)_ecssim_exec_module},
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
    {0, nullptr}
};
static struct PyModuleDef orkengine_ecssim_module = {
    PyModuleDef_HEAD_INIT,
    "_ecssim",
    nullptr,  // module documentation, may be NULL 
    0,       // size of per-interpreter state of the module, or -1 if the module keeps state in global variables. 
    nullptr,  // _core_methods
    _ecssim_slots,  // _ecssim_slots
    nullptr,  // _core_traverse
    nullptr,  // _core_clear
    nullptr   // _core_free
};
extern "C" PyObject* PyInit__ecssim() {
  return PyModuleDef_Init(&orkengine_ecssim_module);
}


