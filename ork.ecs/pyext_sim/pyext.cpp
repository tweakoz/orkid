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
#include <ork/python/pycodec.inl>
#include <ork/python/common_bindings/pyext_crcstring.inl>

///////////////////////////////////////////////////////////////////////////////

namespace nb = obind;
namespace py = obind;
using namespace obind::literals;
using adapter_t = ork::python::nanobindadapter;

#include <ork/python/common_bindings/pyext_math_la.inl>

////////////////////////////////////////////////////////////////////////////////

namespace ork::ecssim {

using codec_ptr_t = typename ork::python::obind_typecodec_ptr_t;
using module_t = typename py::module_;

void register_simulation(module_t& module_ecssim,codec_ptr_t type_codec);
void register_system(module_t& module_ecssim,codec_ptr_t type_codec);
void register_datatable(module_t& module, codec_ptr_t type_codec);
void register_entity(module_t& module, codec_ptr_t type_codec);
void register_component(module_t& module, codec_ptr_t type_codec);

void _ecssim_init_classes(module_t &module_ecssim) {

  auto type_codec_nb = ork::python::TypeCodec<adapter_t>::instance();

  ork::python::_init_crcstring<adapter_t>(module_ecssim, type_codec_nb);
  ork::python::pyinit_math_la_t_vec<float>(module_ecssim, "", type_codec_nb);
  register_datatable(module_ecssim,type_codec_nb);
  register_simulation(module_ecssim,type_codec_nb);
  register_system(module_ecssim,type_codec_nb);
  register_entity(module_ecssim,type_codec_nb);
  register_component(module_ecssim,type_codec_nb);
}

} // namespace ork::ecssim

////////////////////////////////////////////////////////////////////////////////

int _ecssim_exec_module(PyObject *m) {
    printf( "begin _ecssim_exec_module\n");
    try {
        nb::detail::init(NB_DOMAIN_STR);                                 
        nb::module_ mod = nb::borrow<nb::module_>(m);
        ork::ecssim::_ecssim_init_classes(mod);
    } catch (const std::exception &e) {
        PyErr_SetString(PyExc_RuntimeError, e.what());
        return -1;
    }
    printf( "end _ecssim_exec_module\n");
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
  printf( "initializing _ecssim\n");
  return PyModuleDef_Init(&orkengine_ecssim_module);
}


