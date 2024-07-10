///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//#include "pyext.h"
#include <ork/python/pycodec.inl>

namespace py = pybind11;
using namespace pybind11::literals;
using adapter_t = ork::python::pybind11adapter;

#include <ork/python/common_bindings/pyext_math_la.inl>


///////////////////////////////////////////////////////////////////////////////
namespace ork::python {
void init_math_la_double(py::module& module_core,python::pb11_typecodec_ptr_t type_codec) {
  pyinit_math_la_t<double>(module_core, "d", type_codec);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork