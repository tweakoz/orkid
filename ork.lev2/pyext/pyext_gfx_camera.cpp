////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/camera/uicam.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void pyinit_gfx_camera(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto ezuicam_type = //
      py::class_<EzUiCam, ezuicam_ptr_t>(module_lev2, "EzUiCam")
          .def(py::init<>())
          .def("uiEventHandler", [](ezuicam_ptr_t cam, ui::event_ptr_t event) -> bool {
            return cam->UIEventHandler(event);
          })
          .def(
              "createDrawable", //
              [](ezuicam_ptr_t cam) -> drawable_ptr_t {
                return cam->createOverlayDrawable();
              })
          .def(
              "lookAt",
              [](ezuicam_ptr_t uic, fvec3 eye, fvec3 tgt, fvec3 up) { //
                uic->lookAt(eye,tgt,up);
              })
          .def("updateMatrices", [](ezuicam_ptr_t cam) {
            cam->updateMatrices();
          })
          .def_property_readonly(
              "cameradata",
              [](ezuicam_ptr_t uic) -> cameradata_ptr_t { //
                // TODO: this is not efficient
                //  get ezuicam to use shared ptrs instead of by value
                auto camdata = std::make_shared<CameraData>();
                *camdata = uic->_camcamdata;
                return camdata;
              })
          .def_property(
              "base_zmoveamt",
              [](ezuicam_ptr_t uic) -> float { //
                return uic->_base_zmoveamt;
              },
              [](ezuicam_ptr_t uic, float zamt) { //
                uic->_base_zmoveamt = zamt;
              })
          .def_property(
              "fov",
              [](ezuicam_ptr_t uic) -> float { //
                return uic->_fov;
              },
              [](ezuicam_ptr_t uic, float fov) { //
                uic->_fov = fov;
              })
          .def_property_readonly(
              "center",
              [](ezuicam_ptr_t uic) -> fvec3 { //
                return uic->mvCenter;
              })
          .def_property_readonly(
              "orientation",
              [](ezuicam_ptr_t uic) -> fquat { //
                return uic->QuatC;
              })
          .def_property(
              "loc",
              [](ezuicam_ptr_t uic) -> fvec3 { //
                return uic->CamLoc;
              },
              [](ezuicam_ptr_t uic, fvec3 loc) { //
                uic->CamLoc = loc;
                uic->PrevCamLoc = loc;
              })
          .def_property(
              "distance",
              [](ezuicam_ptr_t uic) -> float { //
                return uic->mfLoc;
              },
              [](ezuicam_ptr_t uic, float loc) { //
                uic->mfLoc = loc;
              })
          .def_property(
              "constrainZ",
              [](ezuicam_ptr_t uic) -> bool { //
                return uic->_constrainZ;
              },
              [](ezuicam_ptr_t uic, bool c) { //
                uic->_constrainZ = c;
              })
          ;
  type_codec->registerStdCodec<ezuicam_ptr_t>(ezuicam_type);
  /////////////////////////////////////////////////////////////////////////////
} // void pyinit_gfx_camera(py::module& module_lev2) {

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2 {