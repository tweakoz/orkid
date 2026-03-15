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
              [](probecompdata_ptr_t cd, const std::string& val) { cd->_renderLayer = val; });
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
  /////////////////////////////////////////////////////////////////////////////////
  // Module-level probe baking functions (operate on simulation)
  // Must be called from _onGpuUpdate (outside beginFrame/endFrame)
  /////////////////////////////////////////////////////////////////////////////////
  module_ecs.def("markProbesDirty", [](simulation_ptr_t sim) {
    auto probe_sys = sim->findSystem<ProbeSystem>();
    if (probe_sys) {
      probe_sys->markAllDirty();
    }
  });
  module_ecs.def("bakeProbes", [](simulation_ptr_t sim, ctx_t ctx, std::string output_base) -> int {
    auto probe_sys = sim->findSystem<ProbeSystem>();
    if (!probe_sys) return 0;
    return probe_sys->bakeAll(ctx.get(), output_base);
  });
  module_ecs.def("areProbesClean", [](simulation_ptr_t sim) -> bool {
    auto probe_sys = sim->findSystem<ProbeSystem>();
    if (!probe_sys) return true;
    return probe_sys->areAllClean();
  });
  module_ecs.def("activateBakeOnlyProbes", [](simulation_ptr_t sim) {
    auto probe_sys = sim->findSystem<ProbeSystem>();
    if (probe_sys) probe_sys->activateBakeOnly();
  });
  module_ecs.def("deactivateBakeOnlyProbes", [](simulation_ptr_t sim) {
    auto probe_sys = sim->findSystem<ProbeSystem>();
    if (probe_sys) probe_sys->deactivateBakeOnly();
  });
}
} // namespace ork::ecs
