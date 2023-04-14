///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
///////////////////////////////////////////////////////////////////////////////
namespace ork {
void pyinit_math_plane(py::module& module_core);
void pyinit_math_la_float(py::module& module_core);
void pyinit_math_la_double(py::module& module_core);
void pyinit_math(py::module& module_core) {
  auto type_codec = python::TypeCodec::instance();



  /////////////////////////////////////////////////////////////////////////////////
  struct MathConstantsProxy {};
  using mathconstantsproxy_ptr_t = std::shared_ptr<MathConstantsProxy>;
  auto mathconstantsproxy_type   =                                                           //
      py::class_<MathConstantsProxy, mathconstantsproxy_ptr_t>(module_core, "mathconstants") //
          .def(py::init<>())
          .def(
              "__getattr__",                                                                       //
              [type_codec](mathconstantsproxy_ptr_t proxy, const std::string& key) -> py::object { //
                python::varval_t value;
                value.set<void*>(nullptr);
                if (key == "DTOR") {
                  value.set<float>(DTOR);
                }
                return type_codec->encode(value);
              });
  type_codec->registerStdCodec<mathconstantsproxy_ptr_t>(mathconstantsproxy_type);
  /////////////////////////////////////////////////////////////////////////////////
  pyinit_math_plane(module_core);
  pyinit_math_la_float(module_core);
  pyinit_math_la_double(module_core);
}

} // namespace ork
