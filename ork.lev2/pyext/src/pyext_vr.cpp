////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/vr/vr.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void pyinit_vr(py::module& module_lev2) {
  auto vrmodule   = module_lev2.def_submodule("orkidvr", "vr operations");
  auto type_codec = python::pb11_typecodec_t::instance();
  vrmodule.def("novr_device", [type_codec]() -> orkidvr::device_ptr_t { //
      return orkidvr::novr::novr_device();
  });
  /////////////////////////////////////////////////////////////////////////////////
  auto vrdevice_type = //
      py::class_<orkidvr::Device, orkidvr::device_ptr_t>(module_lev2, "Device")
       .def("setPoseMatrix",[=](orkidvr::device_ptr_t dev, std::string name, const fmtx4& mtx) { //
        dev->_posemap[name] = mtx;
      })
      .def_property("FOV", [](orkidvr::device_ptr_t dev) -> float { //
        return dev->_fov;
      }, [](orkidvr::device_ptr_t dev, float fov) { //
        dev->_fov = fov;
      })
      .def_property("IPD", [](orkidvr::device_ptr_t dev) -> float { //
        return dev->_IPD;
      }, [](orkidvr::device_ptr_t dev, float ipd) { //
        dev->_IPD = ipd;
      })
      .def_property("near", [](orkidvr::device_ptr_t dev) -> float { //
        return dev->_near;
      }, [](orkidvr::device_ptr_t dev, float near) { //
        dev->_near = near;
      })
      .def_property("far", [](orkidvr::device_ptr_t dev) -> float { //
        return dev->_far;
      }, [](orkidvr::device_ptr_t dev, float far) { //
        dev->_far = far;
      })
      .def_property("active", [](orkidvr::device_ptr_t dev) -> bool { //
        return dev->_active;
      }, [](orkidvr::device_ptr_t dev, bool active) { //
        dev->_active = active;
      })
      .def_property("camera", [](orkidvr::device_ptr_t dev) -> std::string { //
        return dev->_cameraName;
      }, [](orkidvr::device_ptr_t dev, std::string name) { //
        dev->_cameraName = name;
      })
      .def_property("calibstate", [](orkidvr::device_ptr_t dev) -> int { //
        return dev->_calibstate;
      }, [](orkidvr::device_ptr_t dev, int state) { //
        dev->_calibstate = state;
      })
      .def("resetCalibration", &orkidvr::Device::resetCalibration);
  type_codec->registerStdCodec<orkidvr::device_ptr_t>(vrdevice_type);
  /////////////////////////////////////////////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
