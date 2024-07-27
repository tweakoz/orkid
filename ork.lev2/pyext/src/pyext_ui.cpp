////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/surface.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/viewport_scenegraph.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/box.h>
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/lev2/ui/popups.inl>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/profiling.inl>
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_ui_ged(py::module& module_ui);
void pyinit_ui_layout(py::module& module_ui);

void pyinit_ui(py::module& module_lev2) {
  auto uimodule   = module_lev2.def_submodule("ui", "ui operations");
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  uimodule.def(
      "popupOpenDialog",
      [](std::string title,                         //
         std::string default_path_and_file,         //
         std::vector<std::string> filter_patterns,  //
         bool allow_multiple_selects) -> py::list { //
        std::string values = ui::popupOpenDialog(title, default_path_and_file, filter_patterns, allow_multiple_selects);
        py::list rval;
        // split values by |
        std::string delimiter = "|";
        size_t pos            = 0;
        std::string token;
        while ((pos = values.find(delimiter)) != std::string::npos) {
          token = values.substr(0, pos);
          rval.append(token);
          values.erase(0, pos + delimiter.length());
        }
        rval.append(values);
        return rval;
      });
  uimodule.def(
      "popupSaveDialog",
      [](std::string title,                                         //
         std::string default_path_and_file,                         //
         std::vector<std::string> filter_patterns) -> std::string { //
        return ui::popupSaveDialog(title, default_path_and_file, filter_patterns);
      });
  uimodule.def(
      "popupFolderDialog",
      [](std::string title,                         //
         std::string default_path) -> std::string { //
        return ui::popupFolderDialog(title, default_path);
      });
  /////////////////////////////////////////////////////////////////////////////////
  auto uicontext_type = //
      py::class_<ui::Context, ui::context_ptr_t>(module_lev2, "Context")
          .def_property_readonly("hasKeyboardFocus", [](ui::context_ptr_t uictx) -> bool { return uictx->hasKeyboardFocus(); })
          .def("hasMouseFocus", [](ui::context_ptr_t uictx, uiwidget_ptr_t w) -> bool { return uictx->hasMouseFocus(w.get()); })
          .def("dumpWidgets", [](ui::context_ptr_t uictx, std::string label) { uictx->dumpWidgets(label); })
          .def("isKeyDown", [](ui::context_ptr_t uictx, int keycode) -> bool { return uictx->isKeyDown(keycode); })
          .def_property("overlayWidget", //
            [](ui::context_ptr_t uictx) -> uiwidget_ptr_t { //
              return uictx->_overlayWidget; //
            }, //
            [](ui::context_ptr_t uictx, uiwidget_ptr_t w) { //
              uictx->_overlayWidget = w; //
            }) //
          ;
  type_codec->registerStdCodec<ui::context_ptr_t>(uicontext_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto uievent_type = //
      py::class_<ui::Event, ui::event_ptr_t>(module_lev2, "Event")
          .def( "__repr__", [](ui::event_ptr_t ev) -> std::string { //
            return ev->description();
          })
          .def(
              "clone",                                    //
              [](ui::event_ptr_t ev) -> ui::event_ptr_t { //
                auto cloned_event = std::make_shared<ui::Event>();
                *cloned_event     = *ev;
                return cloned_event;
              })
          .def_property_readonly(
              "pos",                            //
              [](ui::event_ptr_t ev) -> fvec2 { //
                return fvec2(ev->miX, ev->miY);
              })
          .def_property_readonly(
              "unit_pos",                       //
              [](ui::event_ptr_t ev) -> fvec2 { //
                return fvec2(ev->mfUnitX, ev->mfUnitY);
              })
          .def_property_readonly(
              "screen_dim",                     //
              [](ui::event_ptr_t ev) -> fvec2 { //
                return fvec2(ev->miScreenWidth, ev->miScreenHeight);
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
            "wheel_x",                           //
            [](ui::event_ptr_t ev) -> int { //
              return ev->miMWX;
            })
          .def_property_readonly(
            "wheel_y",                           //
            [](ui::event_ptr_t ev) -> int { //
              return ev->miMWY;
            })
          .def_property_readonly(
              "midiController",               //
              [](ui::event_ptr_t ev) -> int { //
                return ev->_midiController;
              })
          .def_property_readonly(
              "midiValue",                    //
              [](ui::event_ptr_t ev) -> int { //
                return ev->_midiValue;
              })
          .def_property_readonly(
              "keycode",                      //
              [](ui::event_ptr_t ev) -> int { //
                return ev->miKeyCode;
              })
          .def_property_readonly(
              "code",                              //
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
              "super",                        //
              [](ui::event_ptr_t ev) -> int { //
                return int(ev->mbSUPER);
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
              })
              .def_property_readonly("rayN", [](ui::event_ptr_t ev) -> fvec4 { //
                return ev->mvRayN;
              })
              .def_property_readonly("rayF", [](ui::event_ptr_t ev) -> fvec4 { //
                return ev->mvRayF;
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
      py::class_<ui::HandlerResult>(uimodule, "HandlerResult").def(py::init<>())
      .def(
          "setHandler",
          [](ui::HandlerResult& hr, uiwidget_ptr_t handler) { //
            hr.mHandler = handler.get();
          });
  type_codec->registerStdCodec<ui::HandlerResult>(evhandlerrestult_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto widget_type = //
      py::class_<ui::Widget, uiwidget_ptr_t>(uimodule, "Widget")
          .def_property(
              "evhandler",
              [](uiwidget_ptr_t widget) -> py::object { //
                return py::none();
              },
              [](uiwidget_ptr_t widget, py::object callback) { //
                widget->_uservars.makeValueForKey<py::object>("_hold_ev_callback", callback);
                widget->_evhandler = [widget](ui::event_constptr_t ev) -> ui::HandlerResult {
                  ui::HandlerResult rval;
                  EASY_BLOCK("pyui::evh1", profiler::colors::Red);
                  py::gil_scoped_acquire acquire_gil;
                  EASY_END_BLOCK;
                  EASY_BLOCK("pyui::evh2", profiler::colors::Red);
                  auto cb = widget->_uservars.typedValueForKey<py::object>("_hold_ev_callback").value();
                  if (cb) {
                    auto pyrval = cb(ev);
                    if (pyrval) {
                      rval = py::cast<ui::HandlerResult>(pyrval);
                    }
                  }
                  return rval;
                };
              })
          .def_property(
              "userID",
              [](uiwidget_ptr_t widget) -> uint64_t { //
                return widget->_userID;
              },
              [](uiwidget_ptr_t widget, uint64_t uid) { //
                widget->_userID = uid;
              })
          .def_property_readonly(
              "name",
              [](uiwidget_ptr_t widget) -> std::string { //
                return widget->GetName();
              })
          .def_property_readonly(
              "x",
              [](uiwidget_ptr_t widget) -> int { //
                return widget->x();
              })
          .def_property_readonly(
              "y",
              [](uiwidget_ptr_t widget) -> int { //
                return widget->y();
              })
          .def_property_readonly(
              "x2",
              [](uiwidget_ptr_t widget) -> int { //
                return widget->x()+widget->width()-1;
              })
          .def_property_readonly(
              "y2",
              [](uiwidget_ptr_t widget) -> int { //
                return widget->y()+widget->height()-1;
              })
          .def_property_readonly(
              "width",
              [](uiwidget_ptr_t widget) -> int { //
                return widget->width();
              })
          .def_property_readonly(
              "height",
              [](uiwidget_ptr_t widget) -> int { //
                return widget->height();
              })
          .def_property_readonly(
              "aspect",
              [](uiwidget_ptr_t widget) -> float { //
                return float(widget->width())/float(widget->height());
              })
          .def(
              "setPos",
              [](uiwidget_ptr_t widget, int x, int y) { //
                widget->SetPos(x, y);
              })
          .def(
              "setDirty",
              [](uiwidget_ptr_t widget) { //
                widget->SetDirty();
              })
          .def(
              "setSize",
              [](uiwidget_ptr_t widget, int w, int h) { //
                widget->SetSize(w, h);
              })
          .def("setRect", [](uiwidget_ptr_t widget, int x, int y, int w, int h) { //
            widget->SetRect(x, y, w, h);
          })
          .def_property("ignoreEvents", [](uiwidget_ptr_t widget) -> bool { //
            return widget->_ignoreEvents;
          }, [](uiwidget_ptr_t widget, bool x) { //
            widget->_ignoreEvents = x;
          })
          .def_property("enableDraw", [](uiwidget_ptr_t widget) -> bool { //
            return widget->_enableDraw;
          }, [](uiwidget_ptr_t widget, bool x) { //
            widget->_enableDraw = x;
          })
          .def("getUserVar", [type_codec](uiwidget_ptr_t widget, std::string key) -> py::object { //
            py::object rval;
            if(widget->_uservars.hasKey(key)){
              rval = type_codec->encode(widget->_uservars.valueForKey(key));
            }
            return rval;
          });
  type_codec->registerStdCodec<uiwidget_ptr_t>(widget_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto group_type = //
      py::class_<ui::Group, ui::Widget, uigroup_ptr_t>(uimodule, "Group").def("updateLayout", [](uigroup_ptr_t grp) {
        grp->DoLayout();
      });
  type_codec->registerStdCodec<uigroup_ptr_t>(group_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto layoutgroup_type = //
      py::class_<ui::LayoutGroup, ui::Group, uilayoutgroup_ptr_t>(uimodule, "LayoutGroup")
          .def_property(
              "clearColor",
              [](uilayoutgroup_ptr_t lgrp) -> fvec4 { //
                return lgrp->clearColor();
              },
              [](uilayoutgroup_ptr_t lgrp, fvec4 c) { //
                lgrp->setClearColor(c);
              })
          .def_property_readonly(
              "layout",
              [](uilayoutgroup_ptr_t lgrp) -> uilayout_ptr_t { //
                return lgrp->_layout;
              })
          .def_property_readonly(
              "horizontal_guides",
              [](uilayoutgroup_ptr_t lgrp) -> std::vector<uiguide_ptr_t> { //
                std::multimap<float, uiguide_ptr_t> sorted;
                for (auto g : lgrp->horizontalGuides()) {
                  sorted.insert(std::make_pair(g->sortKey(), g));
                }
                std::vector<uiguide_ptr_t> rval;
                for (auto item : sorted) {
                  rval.push_back(item.second);
                }
                return rval;
              })
          .def_property_readonly(
              "vertical_guides",
              [](uilayoutgroup_ptr_t lgrp) -> std::vector<uiguide_ptr_t> { //
                std::multimap<float, uiguide_ptr_t> sorted;
                for (auto g : lgrp->verticalGuides()) {
                  sorted.insert(std::make_pair(g->sortKey(), g));
                }
                std::vector<uiguide_ptr_t> rval;
                for (auto item : sorted) {
                  rval.push_back(item.second);
                }
                return rval;
              })
          .def(
              "layoutAndAddChild",
              [](uilayoutgroup_ptr_t lgrp, uiwidget_ptr_t w) -> uilayout_ptr_t { //
                return lgrp->layoutAndAddChild(w);
              })
          .def(
              "removeChild",
              [](uilayoutgroup_ptr_t lgrp, uilayout_ptr_t ch) { //
                lgrp->removeChild(ch);
              })
          .def(
              "replaceChild",
              [](uilayoutgroup_ptr_t lgrp, uilayout_ptr_t ch, uilayoutitem_ptr_t rep) { //
                lgrp->replaceChild(ch, rep);
              })
          .def(
              "makeEvTestBox",
              [type_codec](
                  uilayoutgroup_ptr_t lgrp,
                  py::kwargs kwargs) -> uilayoutitem_ptr_t { //
                uilayoutitem_ptr_t rval;
                if (kwargs) {
                  auto var_args      = type_codec->decode_kwargs(kwargs);
                  int w              = var_args.typedValueForKey<int>("w").value();
                  int h              = var_args.typedValueForKey<int>("h").value();
                  int x              = var_args.typedValueForKey<int>("x").value();
                  int y              = var_args.typedValueForKey<int>("y").value();
                  fvec4 color_normal = var_args.typedValueForKey<fvec4>("color_normal").value();
                  ;
                  fvec4 color_click = var_args.typedValueForKey<fvec4>("color_click").value();
                  ;
                  fvec4 color_doubleclick = var_args.typedValueForKey<fvec4>("color_doubleclick").value();
                  ;
                  fvec4 color_drag = var_args.typedValueForKey<fvec4>("color_drag").value();
                  ;
                  std::string name = var_args.typedValueForKey<std::string>("name").value();
                  ;
                  auto litem    = lgrp->makeChild<ui::EvTestBox>(name, color_normal);
                  rval = litem.as_shared();
                }
                return rval;
              })
          .def(
              "makeChild",
              [](uilayoutgroup_ptr_t lgrp, py::kwargs kwargs) -> uilayoutitem_ptr_t { //
                uilayoutitem_ptr_t rval;
                if (kwargs) {
                  int width  = 0;
                  int height = 0;
                  int margin = 0;
                  py::list args;
                  py::object uifactory;
                  int args_parsed = 0;
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "uiclass") {
                      auto uiclass_obj   = py::cast<py::object>(item.second);
                      bool has_uifactory = py::hasattr(uiclass_obj, "uifactory");
                      OrkAssert(has_uifactory);
                      uifactory = uiclass_obj.attr("uifactory");
                      args_parsed++;
                    } else if (key == "args") {
                      args = py::cast<py::list>(item.second);
                      args_parsed++;
                    }
                  }
                  OrkAssert(args_parsed == 2);
                  rval = py::cast<uilayoutitem_ptr_t>(uifactory(lgrp, args));
                }
                return rval;
              })
          .def(
              "makeRowsColumns",
              [](uilayoutgroup_ptr_t lgrp, py::kwargs kwargs) -> py::list { //
                py::list rval;
                if (kwargs) {
                  py::list rccounts;
                  int margin = 0;
                  py::list args;
                  py::object uirc_factory;
                  int args_parsed = 0;
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "rccounts") {
                      rccounts = py::cast<py::list>(item.second);
                      args_parsed++;
                    } else if (key == "margin") {
                      margin = py::cast<int>(item.second);
                      args_parsed++;
                    } else if (key == "uiclass") {
                      auto uiclass_obj   = py::cast<py::object>(item.second);
                      bool has_uifactory = py::hasattr(uiclass_obj, "uircfactory");
                      OrkAssert(has_uifactory);
                      uirc_factory = uiclass_obj.attr("uircfactory");
                      args_parsed++;
                    } else if (key == "args") {
                      args = py::cast<py::list>(item.second);
                      args_parsed++;
                    }
                  }
                  OrkAssert(args_parsed == 4);
                  rval = uirc_factory(lgrp, rccounts, margin, args);
                }
                return rval;
              })
              .def(
              "makeGrid",
              [](uilayoutgroup_ptr_t lgrp, py::kwargs kwargs) -> py::list { //
                py::list rval;
                if (kwargs) {
                  int width  = 0;
                  int height = 0;
                  int margin = 0;
                  py::list args;
                  py::object uigrid_factory;
                  int args_parsed = 0;
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "width") {
                      width = py::cast<int>(item.second);
                      args_parsed++;
                    } else if (key == "height") {
                      height = py::cast<int>(item.second);
                      args_parsed++;
                    } else if (key == "margin") {
                      margin = py::cast<int>(item.second);
                      args_parsed++;
                    } else if (key == "uiclass") {
                      auto uiclass_obj   = py::cast<py::object>(item.second);
                      bool has_uifactory = py::hasattr(uiclass_obj, "uigridfactory");
                      OrkAssert(has_uifactory);
                      uigrid_factory = uiclass_obj.attr("uigridfactory");
                      args_parsed++;
                    } else if (key == "args") {
                      args = py::cast<py::list>(item.second);
                      args_parsed++;
                    }
                  }
                  OrkAssert(args_parsed == 5);
                  rval = uigrid_factory(lgrp, width, height, margin, args);
                }
                return rval;
              })
          .def_property(
              "margin",
              [](uilayoutgroup_ptr_t lgrp) -> int { //
                return lgrp->_margin;
              },
              [](uilayoutgroup_ptr_t lgrp, int m) { //
                lgrp->_margin = m;
              });
  type_codec->registerStdCodec<uilayoutgroup_ptr_t>(layoutgroup_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto surface_type = //
      py::class_<ui::Surface, ui::Group, uisurface_ptr_t>(uimodule, "Surface")
          .def(
              "onPostRender",
              [](uisurface_ptr_t surface, py::object callback) { //
                OrkAssert(py::hasattr(callback, "__call__"));
                surface->_uservars.makeValueForKey<py::object>("_hold_postrender_callback", callback);
                surface->_postRenderCallback = [surface]() {
                  py::gil_scoped_acquire acquire_gil;
                  auto cb = surface->_uservars.typedValueForKey<py::object>("_hold_postrender_callback");
                  cb.value()();
                };
              })
          //////////////////////////////////
          .def(
              "decoupleFromUiSize",
              [](uisurface_ptr_t surface, int w, int h) { //
                return surface->decoupleFromUiSize(w, h);
              })
          //////////////////////////////////
          .def_property(
              "aspect_from_rtgroup",
              [](uisurface_ptr_t surface) -> bool { //
                return surface->_aspect_from_rtgroup;
              },
              [](uisurface_ptr_t surface, bool x) { //
                return surface->_aspect_from_rtgroup = x;
              })
          .def_property_readonly(
              "rtgroup",
              [](uisurface_ptr_t surface) -> rtgroup_ptr_t { //
                return surface->_rtgroup;
              })
          .def_property(
              "clearColor",
              [](uisurface_ptr_t surface) -> fvec3 { //
                return surface->GetClearColorRef();
              },
              [](uisurface_ptr_t surface, fvec3 c) { return surface->GetClearColorRef() = c; });
  type_codec->registerStdCodec<uisurface_ptr_t>(surface_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto viewport_type = //
      py::class_<ui::Viewport, ui::Surface, uiviewport_ptr_t>(uimodule, "Viewport");
  type_codec->registerStdCodec<uiviewport_ptr_t>(viewport_type);

  /////////////////////////////////////////////////////////////////////////////////
  auto box_type = //
      py::class_<ui::Box, ui::Widget, uibox_ptr_t>(uimodule, "Box")
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args    = type_codec->decodeList(py_args);
                auto name            = decoded_args[0].get<std::string>();
                auto color           = decoded_args[1].get<fvec4>();
                auto layoutitem      = lg->makeChild<ui::Box>(name, color);
                return layoutitem.as_shared();
              })
          .def_static(
              "uigridfactory",
              [type_codec](uilayoutgroup_ptr_t lg, int grid_w, int grid_h, int m, py::list py_args) -> py::list { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto layoutitems  = lg->makeGridOfWidgets<ui::Box>(grid_w, grid_h, name, color);
                py::list rval;
                for (auto item : layoutitems) {
                  rval.append(item.as_shared());
                }
                return rval;
              })
          .def_static(
              "uircfactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list rowcols, int m, py::list py_args) -> py::list { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                int h = rowcols.size();
                std::vector<int> rccounts;
                for (int i = 0; i < h; i++) {
                  int rc = py::cast<int>(rowcols[i]);
                  rccounts.push_back(rc);
                }
                auto layoutitems  = lg->makeWidgetsRC<ui::Box>(rccounts, name, color);
                py::list rval;
                for (auto item : layoutitems) {
                  rval.append(item.as_shared());
                }
                return rval;
              });
  type_codec->registerStdCodec<uibox_ptr_t>(box_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto evtestbox_type = //
      py::class_<ui::EvTestBox, ui::Widget, uievtestbox_ptr_t>(uimodule, "EvTestBox")
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args    = type_codec->decodeList(py_args);
                auto name            = decoded_args[0].get<std::string>();
                auto color           = decoded_args[1].get<fvec4>();
                auto layoutitem      = lg->makeChild<ui::EvTestBox>(name, color);
                return layoutitem.as_shared();
              })
          .def_static(
              "uigridfactory",
              [type_codec](uilayoutgroup_ptr_t lg, int grid_w, int grid_h, int m, py::list py_args) -> py::list { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto layoutitems  = lg->makeGridOfWidgets<ui::EvTestBox>(grid_w, grid_h, name, color);
                py::list rval;
                for (auto item : layoutitems) {
                  rval.append(item.as_shared());
                }
                return rval;
              });
  type_codec->registerStdCodec<uievtestbox_ptr_t>(evtestbox_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto lambdabox_type = //
      py::class_<ui::LambdaBox, ui::Widget, uilambdabox_ptr_t>(uimodule, "LambdaBox")
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args    = type_codec->decodeList(py_args);
                auto name            = decoded_args[0].get<std::string>();
                auto color           = decoded_args[1].get<fvec4>();
                auto layoutitem      = lg->makeChild<ui::LambdaBox>(name, color);
                return layoutitem.as_shared();
              })
          .def_static(
              "uigridfactory",
              [type_codec](uilayoutgroup_ptr_t lg, int grid_w, int grid_h, int m, py::list py_args) -> py::list { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto layoutitems  = lg->makeGridOfWidgets<ui::LambdaBox>(grid_w, grid_h, name, color);
                py::list rval;
                for (auto item : layoutitems) {
                  rval.append(item.as_shared());
                }
                return rval;
              })
          .def("onPressed", [](uilambdabox_ptr_t lbox, py::object on_pressed) { //
            lbox->_onPressed = [on_pressed, lbox]() {
              printf("pressing lbox<%p>\n", (void*)lbox.get());
              py::gil_scoped_acquire acquire;
              on_pressed();
            };
          });
  type_codec->registerStdCodec<uilambdabox_ptr_t>(lambdabox_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto sgviewport_type = //
      py::class_<ui::SceneGraphViewport, ui::Viewport, uisgviewport_ptr_t>(uimodule, "SceneGraphViewport")
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args    = type_codec->decodeList(py_args);
                auto name            = decoded_args[0].get<std::string>();
                auto layoutitem      = lg->makeChild<ui::SceneGraphViewport>(name);
                return layoutitem.as_shared();
              })
          .def_static(
              "uigridfactory",
              [type_codec](uilayoutgroup_ptr_t lg, int grid_w, int grid_h, int m, py::list py_args) -> py::list { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitems  = lg->makeGridOfWidgets<ui::SceneGraphViewport>(grid_w, grid_h, name);
                py::list rval;
                for (auto item : layoutitems) {
                  rval.append(item.as_shared());
                }
                return rval;
              })
          //////////////////////////////////
          .def(
              "forkDB",
              [](uisgviewport_ptr_t sgview) { //
                sgview->forkDB();
              })
          //////////////////////////////////
          .def_property(
              "scenegraph",
              [](uisgviewport_ptr_t sgview) -> lev2::scenegraph::scene_ptr_t { //
                return sgview->_scenegraph;
              },
              [](uisgviewport_ptr_t sgview, lev2::scenegraph::scene_ptr_t sg) { return sgview->_scenegraph = sg; })
          //////////////////////////////////
          .def_property(
              "cameraName",
              [](uisgviewport_ptr_t sgview) -> std::string { //
                return sgview->_cameraname;
              },
              [](uisgviewport_ptr_t sgview, std::string camname) { return sgview->_cameraname = camname; })
          //////////////////////////////////
          .def_property_readonly(
              "outputnode",
              [](uisgviewport_ptr_t sgview) -> lev2::compositoroutnode_rtgroup_ptr_t { //
                return sgview->_outputnode;
              });
  type_codec->registerStdCodec<uisgviewport_ptr_t>(sgviewport_type);
  /////////////////////////////////////////////////////////////////////////////////
  pyinit_ui_layout(uimodule);
  pyinit_ui_ged(uimodule);
}

} // namespace ork::lev2