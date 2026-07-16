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
  // The ACTIVE device — whatever GfxInit selected (openxr when ORKID_VR_DRIVER=openxr
  //  and a runtime bound, else NoVR). Content apps ask for this and check .active.
  vrmodule.def("device", [type_codec]() -> orkidvr::device_ptr_t { //
      return orkidvr::device();
  });
  /////////////////////////////////////////////////////////////////////////////////
  auto vrdevice_type = //
      py::class_<orkidvr::Device, orkidvr::device_ptr_t>(module_lev2, "Device")
       .def("setPoseMatrix",[=](orkidvr::device_ptr_t dev, std::string name, const fmtx4& mtx) { //
        dev->_posemap[name] = mtx;
        if (name == "hmd")
          dev->_trackedPoseValid = false;   // direct hmd set disables forward prediction
      })
      .def("setTrackedPose",[=](orkidvr::device_ptr_t dev, const fvec3& pos, const fquat& orient, //
                                const fvec3& linvel, const fvec3& angvel) { //
        dev->setTrackedPose(pos, orient, linvel, angvel);  // enables C++ forward prediction
      })
      .def_property("prediction_bias", [](orkidvr::device_ptr_t dev) -> float { //
        return dev->_predictionBias;
      }, [](orkidvr::device_ptr_t dev, float b) { //
        dev->_predictionBias = b;
      })
      .def_property("pose_conjugate", [](orkidvr::device_ptr_t dev) -> bool { //
        return dev->_poseConjugate;
      }, [](orkidvr::device_ptr_t dev, bool c) { //
        dev->_poseConjugate = c;
      })
      .def_property("FOVR", [](orkidvr::device_ptr_t dev) -> float { //
        return dev->_fov;
      }, [](orkidvr::device_ptr_t dev, float fov_rad) { //
        dev->_fov = fov_rad;
      })
      .def_property("FOVD", [](orkidvr::device_ptr_t dev) -> float { //
        return dev->_fov*RTOD;
      }, [](orkidvr::device_ptr_t dev, float fov_deg) { //
        dev->_fov = fov_deg * DTOR;
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
        return dev->_camera_name;
      }, [](orkidvr::device_ptr_t dev, std::string name) { //
        dev->_camera_name = name;
      })
      .def_property("calibstate", [](orkidvr::device_ptr_t dev) -> int { //
        return dev->_calibstate;
      }, [](orkidvr::device_ptr_t dev, int state) { //
        dev->_calibstate = state;
      })
      .def("resetCalibration", &orkidvr::Device::resetCalibration)
      .def_property("width", [](orkidvr::device_ptr_t dev) -> int { //
        return dev->_width;
      }, [](orkidvr::device_ptr_t dev, int w) { //
        dev->_width = w;
      })
      .def_property("height", [](orkidvr::device_ptr_t dev) -> int { //
        return dev->_height;
      }, [](orkidvr::device_ptr_t dev, int h) { //
        dev->_height = h;
      });
  /////////////////////////////////////////////////////////////////////////////////
  // StandardVrPresentation : host-configured per-eye HMD presentation profile.
  //  the distortion shader (material) + the device calibration values are set
  //  here; the per-eye present pass is executed in C++ by the VR output node.
  /////////////////////////////////////////////////////////////////////////////////
  auto presentation_type = //
      py::class_<orkidvr::StandardVrPresentation, orkidvr::standardvrpresentation_ptr_t>(vrmodule, "StandardVrPresentation")
       .def(py::init<>())
       .def_property("material", [](orkidvr::standardvrpresentation_ptr_t p) -> freestyle_mtl_ptr_t { //
          return p->_material;
        }, [](orkidvr::standardvrpresentation_ptr_t p, freestyle_mtl_ptr_t m) { //
          p->_material  = m;
          p->_resolved  = false;
        })
       .def_property("enable", [](orkidvr::standardvrpresentation_ptr_t p) -> bool { //
          return p->_enable;
        }, [](orkidvr::standardvrpresentation_ptr_t p, bool v) { //
          p->_enable = v;
        })
       .def_property("technique_achromatic", [](orkidvr::standardvrpresentation_ptr_t p) -> std::string { //
          return p->_techniqueAchromatic;
        }, [](orkidvr::standardvrpresentation_ptr_t p, std::string s) { //
          p->_techniqueAchromatic = s;
          p->_resolved            = false;
        })
       .def_property("technique_chromatic", [](orkidvr::standardvrpresentation_ptr_t p) -> std::string { //
          return p->_techniqueChromatic;
        }, [](orkidvr::standardvrpresentation_ptr_t p, std::string s) { //
          p->_techniqueChromatic = s;
          p->_resolved           = false;
        })
       .def_property("distortion_r", [](orkidvr::standardvrpresentation_ptr_t p) -> fvec4 { //
          return p->_distortionR;
        }, [](orkidvr::standardvrpresentation_ptr_t p, fvec4 v) { //
          p->_distortionR = v;
        })
       .def_property("distortion_g", [](orkidvr::standardvrpresentation_ptr_t p) -> fvec4 { //
          return p->_distortionG;
        }, [](orkidvr::standardvrpresentation_ptr_t p, fvec4 v) { //
          p->_distortionG = v;
        })
       .def_property("distortion_b", [](orkidvr::standardvrpresentation_ptr_t p) -> fvec4 { //
          return p->_distortionB;
        }, [](orkidvr::standardvrpresentation_ptr_t p, fvec4 v) { //
          p->_distortionB = v;
        })
       .def("setEyeTransform", [](orkidvr::standardvrpresentation_ptr_t p, int eye, const fmtx4& m) { //
          OrkAssertI(eye >= 0 and eye < 2, "eye index must be 0 (L) or 1 (R)");
          p->_eyeTransform[eye] = m;
        })
       .def("setEyeViewTransform", [](orkidvr::standardvrpresentation_ptr_t p, int eye, const fmtx4& m) { //
          OrkAssertI(eye >= 0 and eye < 2, "eye index must be 0 (L) or 1 (R)");
          p->_eyeViewTransform[eye] = m;
        })
       .def("setLensCenter", [](orkidvr::standardvrpresentation_ptr_t p, int eye, const fvec2& v) { //
          OrkAssertI(eye >= 0 and eye < 2, "eye index must be 0 (L) or 1 (R)");
          p->_lensCenter[eye] = v;
        });
  type_codec->registerStdCodec<orkidvr::standardvrpresentation_ptr_t>(presentation_type);
  /////////////////////////////////////////////////////////////////////////////////
  vrdevice_type.def_property("presentation", [](orkidvr::device_ptr_t dev) -> orkidvr::standardvrpresentation_ptr_t { //
    return dev->_presentation;
  }, [](orkidvr::device_ptr_t dev, orkidvr::standardvrpresentation_ptr_t pres) { //
    dev->_presentation = pres;
  });
  /////////////////////////////////////////////////////////////////////////////////
  type_codec->registerStdCodec<orkidvr::device_ptr_t>(vrdevice_type);
  /////////////////////////////////////////////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
