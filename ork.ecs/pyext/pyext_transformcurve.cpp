////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/TransformCurveComponent.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_transformcurve(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto tccompdata_type = //
      py::class_<TransformCurveComponentData, ComponentData, transformcurvecompdata_ptr_t>(
          module_ecs, "TransformCurveComponentData")
          .def(
              "__repr__",
              [](const transformcurvecompdata_ptr_t& cd) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::TransformCurveComponentData(%p)", cd.get());
                return fxs.c_str();
              })
          .def_property(
              "curve",
              [](transformcurvecompdata_ptr_t cd) -> math::transformcurve_ptr_t { return cd->_curve; },
              [](transformcurvecompdata_ptr_t cd, math::transformcurve_ptr_t val) { cd->_curve = val; })
          .def_property(
              "playbackSpeed",
              [](transformcurvecompdata_ptr_t cd) -> float { return cd->_playbackSpeed; },
              [](transformcurvecompdata_ptr_t cd, float val) { cd->_playbackSpeed = val; })
          .def_property(
              "looping",
              [](transformcurvecompdata_ptr_t cd) -> bool { return cd->_looping; },
              [](transformcurvecompdata_ptr_t cd, bool val) { cd->_looping = val; });
  type_codec->registerStdCodec<transformcurvecompdata_ptr_t>(tccompdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto tcsysdata_type = //
      py::class_<TransformCurveSystemData, SystemData, transformcurvesysdata_ptr_t>(
          module_ecs, "TransformCurveSystemData")
          .def(
              "__repr__",
              [](transformcurvesysdata_ptr_t sd) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::TransformCurveSystemData(%p)", sd.get());
                return fxs.c_str();
              });
  type_codec->registerStdCodec<transformcurvesysdata_ptr_t>(tcsysdata_type);
}
} // namespace ork::ecs
