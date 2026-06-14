////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/ProbeComponent.h>
#include <ork/ecs/simulation.inl>
#include "../src/core/ProbeComponent_impl.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

using ctx_t = ork::python::unmanaged_ptr<lev2::Context>;

void pyinit_probe(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto probecompdata_type = //
      py::class_<ProbeComponentData, ComponentData, probecompdata_ptr_t>(
          module_ecs, "ProbeComponentData")
          .def(
              "__repr__",
              [](const probecompdata_ptr_t& cd) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::ProbeComponentData(%p)", cd.get());
                return fxs.c_str();
              })
          .def_property(
              "imageDim",
              [](probecompdata_ptr_t cd) -> int { return cd->_imageDim; },
              [](probecompdata_ptr_t cd, int val) { cd->_imageDim = val; })
          .def_property(
              "outputFolder",
              [](probecompdata_ptr_t cd) -> std::string { return cd->_outputFolder; },
              [](probecompdata_ptr_t cd, const std::string& val) { cd->_outputFolder = val; })
          .def_property(
              "outputPrefix",
              [](probecompdata_ptr_t cd) -> std::string { return cd->_outputPrefix; },
              [](probecompdata_ptr_t cd, const std::string& val) { cd->_outputPrefix = val; })
          .def_property(
              "renderLayer",
              [](probecompdata_ptr_t cd) -> std::string { return cd->_renderLayer; },
              [](probecompdata_ptr_t cd, const std::string& val) { cd->_renderLayer = val; })
          .def_property(
              "dynamic",
              [](probecompdata_ptr_t cd) -> bool { return cd->_dynamic; },
              [](probecompdata_ptr_t cd, bool val) { cd->_dynamic = val; });
  type_codec->registerStdCodec<probecompdata_ptr_t>(probecompdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto probesysdata_type = //
      py::class_<ProbeSystemData, SystemData, probesysdata_ptr_t>(
          module_ecs, "ProbeSystemData")
          .def(
              "__repr__",
              [](probesysdata_ptr_t sd) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::ProbeSystemData(%p)", sd.get());
                return fxs.c_str();
              });
  type_codec->registerStdCodec<probesysdata_ptr_t>(probesysdata_type);
  // All probe operations now go through controller.systemNotify / systemRequestWithCallback
}
} // namespace ork::ecs
