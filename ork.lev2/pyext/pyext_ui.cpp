////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/camera/uicam.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_ui(py::module& module_lev2) {
  auto uimodule   = module_lev2.def_submodule("ui", "ui operations");
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ui::DrawEvent, uidrawevent_ptr_t>(module_lev2, "DrawEvent")       //
      .def_property_readonly("context", [](uidrawevent_ptr_t event) -> ctx_t { //
        return ctx_t(event->GetTarget());
      });
  /////////////////////////////////////////////////////////////////////////////////
  auto uievent_type = //
      py::class_<ui::Event, ui::event_ptr_t>(module_lev2, "UiEvent")
          .def(
              "clone",                        //
              [](ui::event_ptr_t ev) -> ui::event_ptr_t { //
                auto cloned_event      = std::make_shared<ui::Event>();
                *cloned_event          = *ev;
                return cloned_event;
              })
          .def_property_readonly(
              "x",                            //
              [](ui::event_ptr_t ev) -> int { //
                return ev->miX;
              })
          .def_property_readonly(
              "y",                            //
              [](ui::event_ptr_t ev) -> int { //
                return ev->miY;
              })
          .def_property_readonly(
              "keycode",                            //
              [](ui::event_ptr_t ev) -> int { //
                return ev->miKeyCode;
              })
          .def_property_readonly(
              "code",                         //
              [](ui::event_ptr_t ev) -> uint64_t { //
                return uint64_t(ev->_eventcode);
              })
          .def_property_readonly(
              "shift",                        //
              [](ui::event_ptr_t ev) -> int { //
                return int(ev->mbSHIFT);
              })
          .def_property_readonly(
              "alt",                          //
              [](ui::event_ptr_t ev) -> int { //
                return int(ev->mbALT);
              })
          .def_property_readonly(
              "ctrl",                         //
              [](ui::event_ptr_t ev) -> int { //
                return int(ev->mbCTRL);
              })
          .def_property_readonly(
              "left",                         //
              [](ui::event_ptr_t ev) -> int { //
                return int(ev->mbLeftButton);
              })
          .def_property_readonly(
              "middle",                       //
              [](ui::event_ptr_t ev) -> int { //
                return int(ev->mbMiddleButton);
              })
          .def_property_readonly(
              "right",                        //
              [](ui::event_ptr_t ev) -> int { //
                return int(ev->mbRightButton);
              });
  type_codec->registerStdCodec<ui::event_ptr_t>(uievent_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto ezuicam_type = //
      py::class_<EzUiCam, ezuicam_ptr_t>(uimodule, "EzUiCam")
          .def(py::init<>())
          .def("uiEventHandler", [](ezuicam_ptr_t cam, ui::event_ptr_t event) -> bool {
            return cam->UIEventHandler(event);
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
              "constrainZ",
              [](ezuicam_ptr_t uic) -> bool { //
                return uic->_constrainZ;
              },
              [](ezuicam_ptr_t uic, bool c) { //
                uic->_constrainZ = c;
              })
          ;
  type_codec->registerStdCodec<ezuicam_ptr_t>(ezuicam_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto evhandlerrestult_type = //
      py::class_<ui::HandlerResult>(uimodule, "UiHandlerResult")
          .def(py::init<>());
  /////////////////////////////////////////////////////////////////////////////////
}

}