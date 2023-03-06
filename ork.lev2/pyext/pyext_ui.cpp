////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/surface.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/anchor.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_ui(py::module& module_lev2) {
  auto uimodule   = module_lev2.def_submodule("ui", "ui operations");
  auto type_codec = python::TypeCodec::instance();
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
  auto drwev_type = py::class_<ui::DrawEvent, uidrawevent_ptr_t>(module_lev2, "DrawEvent")       //
      .def_property_readonly("context", [](uidrawevent_ptr_t event) -> ctx_t { //
        return ctx_t(event->GetTarget());
      });
  type_codec->registerStdCodec<uidrawevent_ptr_t>(drwev_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto evhandlerrestult_type = //
      py::class_<ui::HandlerResult>(uimodule, "UiHandlerResult")
          .def(py::init<>());
  type_codec->registerStdCodec<ui::HandlerResult>(evhandlerrestult_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto widget_type = //
      py::class_<ui::Widget, uiwidget_ptr_t >(uimodule, "UiWidget")
      .def_property_readonly("name", [](uiwidget_ptr_t widget) -> std::string { //
        return widget->GetName();
      })
      .def_property_readonly("x", [](uiwidget_ptr_t widget) -> int { //
        return widget->x();
      })
      .def_property_readonly("y", [](uiwidget_ptr_t widget) -> int { //
        return widget->y();
      })
      .def_property_readonly("width", [](uiwidget_ptr_t widget) -> int { //
        return widget->width();
      })
      .def_property_readonly("height", [](uiwidget_ptr_t widget) -> int { //
        return widget->height();
      })
      .def("setPos", [](uiwidget_ptr_t widget, int x, int y)  { //
        widget->SetPos(x,y);
      })
      .def("setSize", [](uiwidget_ptr_t widget, int w, int h)  { //
        widget->SetSize(w,h);
      })
      .def("setRect", [](uiwidget_ptr_t widget, int x, int y, int w, int h)  { //
        widget->SetRect(x,y,w,h);
      });
  type_codec->registerStdCodec<uiwidget_ptr_t>(widget_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto group_type = //
      py::class_<ui::Group, ui::Widget, uigroup_ptr_t >(uimodule, "UiGroup");
  type_codec->registerStdCodec<uigroup_ptr_t>(group_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto layoutgroup_type = //
      py::class_<ui::LayoutGroup, ui::Group, uilayoutgroup_ptr_t >(uimodule, "UiLayoutGroup")
      .def_property_readonly("layout", [](uilayoutgroup_ptr_t lgrp) -> uilayout_ptr_t { //
        return lgrp->_layout;
      })
      .def("layoutAndAddChild", [](uilayoutgroup_ptr_t lgrp, uiwidget_ptr_t w) -> uilayout_ptr_t { //
        return lgrp->layoutAndAddChild(w);
      });
  type_codec->registerStdCodec<uilayoutgroup_ptr_t>(layoutgroup_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto surface_type = //
      py::class_<ui::Surface, ui::Group, uisurface_ptr_t >(uimodule, "UiSurface");
  type_codec->registerStdCodec<uisurface_ptr_t>(surface_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto viewport_type = //
      py::class_<ui::Viewport, ui::Surface, uiviewport_ptr_t >(uimodule, "UiViewport");
  type_codec->registerStdCodec<uiviewport_ptr_t>(viewport_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto layout_type = //
      py::class_<ui::anchor::Layout, uilayout_ptr_t >(uimodule, "UiLayout")
      .def_property("top", [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
        return layout->top();
      },
      [](uilayout_ptr_t layout, uiguide_ptr_t g){
        return layout->_top = g;
      })
      .def_property("bottom", [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
        return layout->bottom();
      },
      [](uilayout_ptr_t layout, uiguide_ptr_t g){
        layout->_bottom = g;
      })
      .def_property("left", [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
        return layout->left();
      },
      [](uilayout_ptr_t layout, uiguide_ptr_t g){
        layout->_left = g;
      })
      .def_property("right", [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
        return layout->right();
      },
      [](uilayout_ptr_t layout, uiguide_ptr_t g){
        layout->_right = g;
      })
      .def_property("centerH", [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
        return layout->centerH();
      },
      [](uilayout_ptr_t layout, uiguide_ptr_t g){
        layout->_centerH = g;
      })
      .def_property("centerV", [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
        return layout->centerV();
      },
      [](uilayout_ptr_t layout, uiguide_ptr_t g){
        layout->_centerV = g;
      });
  type_codec->registerStdCodec<uilayout_ptr_t>(layout_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto guide_type = //
      py::class_<ui::anchor::Guide, uiguide_ptr_t >(uimodule, "UiGuide")
      .def_property_readonly("margin", [](uiguide_ptr_t guide) -> int { //
        return guide->_margin;
      })
      .def_property_readonly("sign", [](uiguide_ptr_t guide) -> int { //
        return guide->_sign;
      })
      .def_property_readonly("fixed", [](uiguide_ptr_t guide) -> int { //
        return guide->_fixed;
      })
      .def_property_readonly("proportion", [](uiguide_ptr_t guide) -> float { //
        return guide->_proportion;
      });
  type_codec->registerStdCodec<uiguide_ptr_t>(guide_type);
  /////////////////////////////////////////////////////////////////////////////////
}

}