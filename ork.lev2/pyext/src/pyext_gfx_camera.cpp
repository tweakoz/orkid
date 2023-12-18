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
          .def("uiEventHandler", [](ezuicam_ptr_t cam, ui::event_constptr_t event) -> bool {
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
          .def("copyFrom", [](ezuicam_ptr_t cam_dest, ezuicam_ptr_t cam_src) {
            cam_dest->_base_zmoveamt = cam_src->_base_zmoveamt;
            cam_dest->_fov = cam_src->_fov;
            cam_dest->_constrainZ = cam_src->_constrainZ;
            cam_dest->mvCenter = cam_src->mvCenter;
            cam_dest->QuatC = cam_src->QuatC;
            cam_dest->QuatHeading = cam_src->QuatHeading;
            cam_dest->QuatElevation = cam_src->QuatElevation;
            cam_dest->QuatCPushed = cam_src->QuatCPushed;
            cam_dest->CamLoc = cam_src->CamLoc;
            cam_dest->PrevCamLoc = cam_src->PrevCamLoc;
            cam_dest->mfLoc = cam_src->mfLoc;
            cam_dest->_curMatrices = cam_src->_curMatrices;
            cam_dest->_vpdim = cam_src->_vpdim;
            cam_dest->_camcamdata = cam_src->_camcamdata;


           // cam_dest->mNear = cam_src->mNear;
           // cam_dest->mFar = cam_src->mFar;
            //cam_dest->mFovY = cam_src->mFovY;
            //cam_dest->mAspect = cam_src->mAspect;            
          })
          .def("dump", [](ezuicam_ptr_t cam) {
              printf( "ezuicam<%p> dump\n", (void*) cam.get() );
              printf( "  _base_zmoveamt<%g>\n", cam->_base_zmoveamt );
              printf( "  _fov<%g>\n", cam->_fov );
              printf( "  _constrainZ<%d>\n", cam->_constrainZ );
              printf( "  mvCenter<%g %g %g>\n", cam->mvCenter.x, cam->mvCenter.y, cam->mvCenter.z );
              printf( "  QuatC<%g %g %g %g>\n", cam->QuatC.x, cam->QuatC.y, cam->QuatC.z, cam->QuatC.w );
              printf( "  CamLoc<%g %g %g>\n", cam->CamLoc.x, cam->CamLoc.y, cam->CamLoc.z );
              printf( "  PrevCamLoc<%g %g %g>\n", cam->PrevCamLoc.x, cam->PrevCamLoc.y, cam->PrevCamLoc.z );
              printf( "  mfLoc<%g>\n", cam->mfLoc );
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
          .def_property(
              "center",
              [](ezuicam_ptr_t uic) -> fvec3 { //
                return uic->mvCenter;
              },
              [](ezuicam_ptr_t uic, fvec3 center) { //
                uic->mvCenter = center;
              })
          .def_property(
              "orientation",
              [](ezuicam_ptr_t uic) -> fquat { //
                return uic->QuatC;
              },
              [](ezuicam_ptr_t uic, fquat quat) { //
                uic->QuatC = quat;
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