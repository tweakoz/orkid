///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/math/gradient.h>
#include <ork/math/multicurve.h>
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
  /////////////////////////////////////////////////////////////////////////////////
    auto curve_type = //
      py::class_<MultiCurve1D,Object,multicurve1d_ptr_t>(module_core, "MultiCurve1D");
  type_codec->registerStdCodec<multicurve1d_ptr_t>(curve_type);
  /////////////////////////////////////////////////////////////////////////////////
    auto gradient_type = //
      py::class_<gradient_fvec4_t,Object,gradient_fvec4_ptr_t>(module_core, "GradientV4");
  type_codec->registerStdCodec<gradient_fvec4_ptr_t>(gradient_type);
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("dmtx4_to_fmtx4", [](const dmtx4& dmtx) -> fmtx4 { //
    return dmtx4_to_fmtx4(dmtx);
  });
  module_core.def("fmtx4_to_dmtx4", [](const fmtx4& dmtx) -> dmtx4 { //
    return fmtx4_to_dmtx4(dmtx);
  });
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("lerp_float", [](float a, float b, float index) -> float { //
    return ::std::lerp(a, b, index);
  });
}

} // namespace ork
