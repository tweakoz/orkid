////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#include <ork/lev2/drm/drm_types.h>
#endif

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_drm(py::module& module_lev2) {
#if defined(__linux__)
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // drm::Mode binding
  /////////////////////////////////////////////////////////////////////////////////

  auto drm_mode_type = py::class_<drm::Mode, drm::mode_ptr_t>(module_lev2, "DrmMode")
    .def_readonly("index", &drm::Mode::index)
    .def_readonly("width", &drm::Mode::width)
    .def_readonly("height", &drm::Mode::height)
    .def_readonly("refresh_rate", &drm::Mode::refresh_rate)
    .def_readonly("bit_depth", &drm::Mode::bit_depth)
    .def_readonly("name", &drm::Mode::name)
    .def("__repr__", [](const drm::Mode& m) -> std::string {
      fxstring<128> fxs;
      fxs.format("DrmMode(%d: %s %dx%d@%dHz %dbpp)",
                 m.index, m.name.c_str(), m.width, m.height, m.refresh_rate, m.bit_depth);
      return fxs.c_str();
    });
  type_codec->registerStdCodec<drm::mode_ptr_t>(drm_mode_type);

  /////////////////////////////////////////////////////////////////////////////////
  // drm::Monitor binding
  /////////////////////////////////////////////////////////////////////////////////

  auto drm_monitor_type = py::class_<drm::Monitor, drm::monitor_ptr_t>(module_lev2, "DrmMonitor")
    .def_readonly("device_letter", &drm::Monitor::device_letter)
    .def_readonly("connector_id", &drm::Monitor::connector_id)
    .def_readonly("connector_type", &drm::Monitor::connector_type)
    .def_readonly("connector_name", &drm::Monitor::connector_name)
    .def_readonly("brand", &drm::Monitor::brand)
    .def_readonly("connected", &drm::Monitor::connected)
    .def_readonly("modes", &drm::Monitor::modes)
    .def("__repr__", [](const drm::Monitor& mon) -> std::string {
      fxstring<256> fxs;
      fxs.format("DrmMonitor(%c: %s [%s] %s, %zu modes)",
                 mon.device_letter,
                 mon.connector_name.c_str(),
                 mon.connector_type.c_str(),
                 mon.connected ? "connected" : "disconnected",
                 mon.modes.size());
      return fxs.c_str();
    });
  type_codec->registerStdCodec<drm::monitor_ptr_t>(drm_monitor_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Static functions for DRM enumeration
  /////////////////////////////////////////////////////////////////////////////////

  module_lev2.def("enumerateDrmMonitors", []() -> drm::monitor_vect_t {
    return drm::DRMContext::enumerateAllMonitors();
  }, "Enumerate all DRM monitors across every /dev/dri/card* on the system");

  module_lev2.def("printDrmMonitors", [](const drm::monitor_vect_t& monitors) {
    drm::DRMContext::printMonitors(monitors);
  }, "Print formatted list of DRM monitors", py::arg("monitors"));

  module_lev2.def("printDrmMonitors", []() {
    auto monitors = drm::DRMContext::enumerateAllMonitors();
    drm::DRMContext::printMonitors(monitors);
  }, "Enumerate and print all DRM monitors across every /dev/dri/card*");

#endif // __linux__
}

} // namespace ork::lev2
