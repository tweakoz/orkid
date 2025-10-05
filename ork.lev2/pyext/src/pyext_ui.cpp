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
#include <ork/lev2/ui/tabs.h>
#include <ork/lev2/ui/pack.h>
#include <ork/lev2/ui/lineedit.h>
#include <ork/lev2/ui/button.h>
#include <ork/lev2/ui/checkbox.h>
#include <ork/lev2/ui/slider.h>
#include <ork/lev2/ui/combobox.h>
#include <ork/lev2/ui/imgview.h>
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/lev2/ui/popups.inl>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/lev2/gfx/image.h>
#include <ork/profiling.inl>
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_ui_ged(py::module& module_ui);
void pyinit_ui_layout(py::module& module_ui);
void pyinit_ui_box(py::module& module_ui);

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
          .def_property(
              "overlayWidget",                                //
              [](ui::context_ptr_t uictx) -> uiwidget_ptr_t { //
                return uictx->_overlayWidget;                 //
              },                                              //
              [](ui::context_ptr_t uictx, uiwidget_ptr_t w) { //
                uictx->_overlayWidget = w;                    //
              })                                              //
      ;
  type_codec->registerStdCodec<ui::context_ptr_t>(uicontext_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto uievent_type = //
      py::class_<ui::Event, ui::event_ptr_t>(module_lev2, "Event")
          .def(
              "__repr__",
              [](ui::event_ptr_t ev) -> std::string { //
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
              "wheel_x",                      //
              [](ui::event_ptr_t ev) -> int { //
                return ev->miMWX;
              })
          .def_property_readonly(
              "wheel_y",                      //
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
          .def_property_readonly(
              "rayN",
              [](ui::event_ptr_t ev) -> fvec4 { //
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
      py::class_<ui::HandlerResult>(uimodule, "HandlerResult")
          .def(py::init<>())
          .def("setHandler", [](ui::HandlerResult& hr, uiwidget_ptr_t handler) { //
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
                return widget->x() + widget->width() - 1;
              })
          .def_property_readonly(
              "y2",
              [](uiwidget_ptr_t widget) -> int { //
                return widget->y() + widget->height() - 1;
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
                return float(widget->width()) / float(widget->height());
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
          .def(
              "setRect",
              [](uiwidget_ptr_t widget, int x, int y, int w, int h) { //
                widget->SetRect(x, y, w, h);
              })
          .def_property(
              "ignoreEvents",
              [](uiwidget_ptr_t widget) -> bool { //
                return widget->_ignoreEvents;
              },
              [](uiwidget_ptr_t widget, bool x) { //
                widget->_ignoreEvents = x;
              })
          .def_property(
              "enableDraw",
              [](uiwidget_ptr_t widget) -> bool { //
                return widget->_enableDraw;
              },
              [](uiwidget_ptr_t widget, bool x) { //
                widget->_enableDraw = x;
              })
          .def("getUserVar", [type_codec](uiwidget_ptr_t widget, std::string key) -> py::object { //
            py::object rval;
            if (widget->_uservars.hasKey(key)) {
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
          .def_property(
              "flipY",
              [](uisurface_ptr_t surface) -> bool { //
                return surface->_flipY;
              },
              [](uisurface_ptr_t surface, bool x) { //
                surface->_flipY = x;
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
                return surface->_clearColor;
              },
              [](uisurface_ptr_t surface, fvec3 c) { surface->_clearColor = c; });
  type_codec->registerStdCodec<uisurface_ptr_t>(surface_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto viewport_type = //
      py::class_<ui::Viewport, ui::Surface, uiviewport_ptr_t>(uimodule, "Viewport");
  type_codec->registerStdCodec<uiviewport_ptr_t>(viewport_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto sgviewport_type = //
      py::class_<ui::SceneGraphViewport, ui::Viewport, uisgviewport_ptr_t>(uimodule, "SceneGraphViewport")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> uisgviewport_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto box          = std::make_shared<ui::SceneGraphViewport>(name);
                return box;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::SceneGraphViewport>(name);
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
  auto tabsw_type = //
      py::class_<ui::TabWidget, ui::Group, ui::tabwidget_ptr_t>(uimodule, "TabsWidget")
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::TabWidget>(name);
                return layoutitem.as_shared();
              })
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::tabwidget_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto tabs         = std::make_shared<ui::TabWidget>(name);
                return tabs;
              })
          .def("makeChild", [](ui::tabwidget_ptr_t tabs, py::kwargs kwargs) -> ui::widget_ptr_t { //
            ui::widget_ptr_t rval;
            if (kwargs) {
              py::list args;
              py::object wfactory;
              int args_parsed = 0;
              for (auto item : kwargs) {
                auto key = py::cast<std::string>(item.first);
                if (key == "uiclass") {
                  auto uiclass_obj  = py::cast<py::object>(item.second);
                  bool has_wfactory = py::hasattr(uiclass_obj, "wfactory");
                  OrkAssert(has_wfactory);
                  wfactory = uiclass_obj.attr("wfactory");
                  args_parsed++;
                } else if (key == "args") {
                  args = py::cast<py::list>(item.second);
                  args_parsed++;
                }
              }
              OrkAssert(args_parsed == 2);
              rval = py::cast<ui::widget_ptr_t>(wfactory(args));
              tabs->addChild(rval);
            }
            return rval;
          });
  type_codec->registerStdCodec<ui::tabwidget_ptr_t>(tabsw_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto vpack_type = //
      py::class_<ui::VerticalPack, ui::Group, ui::vpack_ptr_t>(uimodule, "VerticalPack")
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::VerticalPack>(name);
                return layoutitem.as_shared();
              })
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::vpack_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto box          = std::make_shared<ui::VerticalPack>(name);
                return box;
              })
          .def(
              "makeChild",
              [](ui::vpack_ptr_t tabs, py::kwargs kwargs) -> ui::widget_ptr_t { //
                ui::widget_ptr_t rval;
                if (kwargs) {
                  py::list args;
                  py::object wfactory;
                  int args_parsed = 0;
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "uiclass") {
                      auto uiclass_obj  = py::cast<py::object>(item.second);
                      bool has_wfactory = py::hasattr(uiclass_obj, "wfactory");
                      OrkAssert(has_wfactory);
                      wfactory = uiclass_obj.attr("wfactory");
                      args_parsed++;
                    } else if (key == "args") {
                      args = py::cast<py::list>(item.second);
                      args_parsed++;
                    }
                  }
                  OrkAssert(args_parsed == 2);
                  rval = py::cast<ui::widget_ptr_t>(wfactory(args));
                  tabs->addChild(rval);
                }
                return rval;
              })
          .def_property(
              "margin",
              [](ui::vpack_ptr_t vpack) -> int { //
                return vpack->_margin;
              },
              [](ui::vpack_ptr_t vpack, int m) { //
                vpack->_margin = m;
              })
          .def_property(
              "item_height",
              [](ui::vpack_ptr_t vpack) -> int { //
                return vpack->_item_height;
              },
              [](ui::vpack_ptr_t vpack, int h) { //
                vpack->_item_height = h;
              })
          .def_property(
              "fill",
              [](ui::vpack_ptr_t vpack) -> bool { //
                return vpack->_fill;
              },
              [](ui::vpack_ptr_t vpack, bool b) { //
                vpack->_fill = b;
              });
  type_codec->registerStdCodec<ui::vpack_ptr_t>(vpack_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto hpack_type = //
      py::class_<ui::HorizontalPack, ui::Group, ui::hpack_ptr_t>(uimodule, "HorizontalPack")
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::HorizontalPack>(name);
                return layoutitem.as_shared();
              })
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::hpack_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto box          = std::make_shared<ui::HorizontalPack>(name);
                return box;
              })
          .def(
              "makeChild",
              [](ui::hpack_ptr_t hpack, py::kwargs kwargs) -> ui::widget_ptr_t { //
                ui::widget_ptr_t rval;
                if (kwargs) {
                  py::list args;
                  py::object wfactory;
                  int args_parsed = 0;
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "uiclass") {
                      auto uiclass_obj  = py::cast<py::object>(item.second);
                      bool has_wfactory = py::hasattr(uiclass_obj, "wfactory");
                      OrkAssert(has_wfactory);
                      wfactory = uiclass_obj.attr("wfactory");
                      args_parsed++;
                    } else if (key == "args") {
                      args = py::cast<py::list>(item.second);
                      args_parsed++;
                    }
                  }
                  OrkAssert(args_parsed == 2);
                  rval = py::cast<ui::widget_ptr_t>(wfactory(args));
                  hpack->addChild(rval);
                }
                return rval;
              })
          .def_property(
              "margin",
              [](ui::hpack_ptr_t hpack) -> int { //
                return hpack->_margin;
              },
              [](ui::hpack_ptr_t hpack, int m) { //
                hpack->_margin = m;
              })
          .def_property(
              "item_width",
              [](ui::hpack_ptr_t hpack) -> int { //
                return hpack->_item_width;
              },
              [](ui::hpack_ptr_t hpack, int w) { //
                hpack->_item_width = w;
              })
          .def_property(
              "fill",
              [](ui::hpack_ptr_t hpack) -> bool { //
                return hpack->_fill;
              },
              [](ui::hpack_ptr_t hpack, bool b) { //
                hpack->_fill = b;
              })
          .def_property(
              "uniform",
              [](ui::hpack_ptr_t hpack) -> bool { //
                return hpack->_uniform;
              },
              [](ui::hpack_ptr_t hpack, bool b) { //
                hpack->_uniform = b;
              });
  type_codec->registerStdCodec<ui::hpack_ptr_t>(hpack_type);
  /////////////////////////////////////////////////////////////////////////////////
  // LineEdit
  auto lineedit_type = //
      py::class_<ui::LineEdit, ui::Widget, ui::lineedit_ptr_t>(uimodule, "LineEdit")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::lineedit_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto deftext      = decoded_args[1].get<std::string>();
                auto color        = decoded_args[2].get<fvec3>();
                auto le           = std::make_shared<ui::LineEdit>(name, color);
                le->setValue(deftext);
                return le;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto deftext      = decoded_args[1].get<std::string>();
                auto color        = decoded_args[2].get<fvec3>();
                auto layoutitem   = lg->makeChild<ui::LineEdit>(name, color);
                layoutitem.typedWidget()->setValue(deftext);
                return layoutitem.as_shared();
              })
          .def_property(
              "text",
              [](ui::lineedit_ptr_t le) -> std::string { //
                return le->_value;
              },
              [](ui::lineedit_ptr_t le, std::string txt) { //
                le->setValue(txt);
              })
          .def_property(
              "fg_color",
              [](ui::lineedit_ptr_t le) -> fvec3 { //
                return le->_fg_color;
              },
              [](ui::lineedit_ptr_t le, fvec3 c) { //
                le->_fg_color = c;
              })
          .def_property(
              "bg_color",
              [](ui::lineedit_ptr_t le) -> fvec3 { //
                return le->_bg_color;
              },
              [](ui::lineedit_ptr_t le, fvec3 c) { //
                le->_bg_color = c;
              });
  type_codec->registerStdCodec<ui::lineedit_ptr_t>(lineedit_type);
  /////////////////////////////////////////////////////////////////////////////////
  // Button
  auto button_type = //
      py::class_<ui::Button, ui::Widget, ui::button_ptr_t>(uimodule, "Button")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::button_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec3>();
                auto button       = std::make_shared<ui::Button>(name, color);
                return button;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec3>();
                auto layoutitem   = lg->makeChild<ui::Button>(name, color);
                return layoutitem.as_shared();
              })
          .def_property(
              "mode",
              [](ui::button_ptr_t btn) -> crcstring_ptr_t { //
                auto crc = std::make_shared<CrcString>(uint64_t(btn->mode()));
                return crc;
              },
              [](ui::button_ptr_t btn, crcstring_ptr_t value) { //
                btn->setMode(ui::ButtonMode(value->hashed()));
              })
          .def_property(
              "toggled",
              [](ui::button_ptr_t btn) -> bool { //
                return btn->isToggled();
              },
              [](ui::button_ptr_t btn, bool val) { //
                btn->setToggled(val);
              })
          .def_property(
              "onPressed",
              [](ui::button_ptr_t btn) -> py::object { //
                return py::none();
              },
              [](ui::button_ptr_t btn, py::object callback) { //
                if (callback.is_none()) {
                  btn->_onPressed = nullptr;
                } else {
                  auto pycb       = std::make_shared<py::object>(callback);
                  btn->_onPressed = [pycb]() { (*pycb)(); };
                }
              })
          .def_property(
              "onToggled",
              [](ui::button_ptr_t btn) -> py::object { //
                return py::none();
              },
              [](ui::button_ptr_t btn, py::object callback) { //
                if (callback.is_none()) {
                  btn->_onToggled = nullptr;
                } else {
                  auto pycb       = std::make_shared<py::object>(callback);
                  btn->_onToggled = [pycb, btn]() {
                    py::gil_scoped_acquire acquire_gil;
                    (*pycb)(btn);
                  };
                }
              })
          .def_property(
              "fg_color",
              [](ui::button_ptr_t btn) -> fvec3 { //
                return btn->_fg_color;
              },
              [](ui::button_ptr_t btn, fvec3 c) { //
                btn->_fg_color = c;
              })
          .def_property(
              "bg_color",
              [](ui::button_ptr_t btn) -> fvec3 { //
                return btn->_bg_color;
              },
              [](ui::button_ptr_t btn, fvec3 c) { //
                btn->_bg_color = c;
              })
          .def_property(
              "check_color",
              [](ui::button_ptr_t btn) -> fvec3 { //
                return btn->_check_color;
              },
              [](ui::button_ptr_t btn, fvec3 c) { //
                btn->_check_color = c;
              });
  type_codec->registerStdCodec<ui::button_ptr_t>(button_type);
  /////////////////////////////////////////////////////////////////////////////////
  // Checkbox
  auto checkbox_type = //
      py::class_<ui::Checkbox, ui::Widget, ui::checkbox_ptr_t>(uimodule, "Checkbox")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::checkbox_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec3>();
                auto checkbox     = std::make_shared<ui::Checkbox>(name, color);
                return checkbox;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec3>();
                auto layoutitem   = lg->makeChild<ui::Checkbox>(name, color);
                return layoutitem.as_shared();
              })
          .def_property(
              "toggled",
              [](ui::checkbox_ptr_t chk) -> bool { //
                return chk->isToggled();
              },
              [](ui::checkbox_ptr_t chk, bool val) { //
                chk->setToggled(val);
              })
          .def_property(
              "onToggled",
              [](ui::checkbox_ptr_t chk) -> py::object { //
                return py::none();
              },
              [](ui::checkbox_ptr_t chk, py::object callback) { //
                if (callback.is_none()) {
                  chk->_onToggled = nullptr;
                } else {
                  auto pycb       = std::make_shared<py::object>(callback);
                  chk->_onToggled = [pycb, chk]() {
                    py::gil_scoped_acquire acquire_gil;
                    (*pycb)(chk);
                  };
                }
              })
          .def_property(
              "fg_color",
              [](ui::checkbox_ptr_t chk) -> fvec3 { //
                return chk->_fg_color;
              },
              [](ui::checkbox_ptr_t chk, fvec3 c) { //
                chk->_fg_color = c;
              })
          .def_property(
              "bg_color",
              [](ui::checkbox_ptr_t chk) -> fvec3 { //
                return chk->_bg_color;
              },
              [](ui::checkbox_ptr_t chk, fvec3 c) { //
                chk->_bg_color = c;
              })
          .def_property(
              "check_color",
              [](ui::checkbox_ptr_t chk) -> fvec3 { //
                return chk->_check_color;
              },
              [](ui::checkbox_ptr_t chk, fvec3 c) { //
                chk->_check_color = c;
              });
  type_codec->registerStdCodec<ui::checkbox_ptr_t>(checkbox_type);
  /////////////////////////////////////////////////////////////////////////////////
  // IntSlider
  auto intslider_type = //
      py::class_<ui::IntSlider, ui::Widget, ui::intslider_ptr_t>(uimodule, "IntSlider")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::intslider_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec3>();
                auto min_val      = decoded_args[2].get<int>();
                auto max_val      = decoded_args[3].get<int>();
                auto value        = decoded_args[4].get<int>();
                auto slider       = std::make_shared<ui::IntSlider>(name, color, min_val, max_val, value);
                return slider;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec3>();
                auto min_val      = decoded_args[2].get<int>();
                auto max_val      = decoded_args[3].get<int>();
                auto value        = decoded_args[4].get<int>();
                auto layoutitem   = lg->makeChild<ui::IntSlider>(name, color, min_val, max_val, value);
                return layoutitem.as_shared();
              })
          .def_property(
              "value",
              [](ui::intslider_ptr_t slider) -> int { //
                return slider->value();
              },
              [](ui::intslider_ptr_t slider, int val) { //
                slider->setValue(val);
              })
          .def_property(
              "onValueChanged",
              [](ui::intslider_ptr_t slider) -> py::object { //
                return py::none();
              },
              [](ui::intslider_ptr_t slider, py::object callback) { //
                if (callback.is_none()) {
                  slider->_onValueChanged = nullptr;
                } else {
                  auto pycb               = std::make_shared<py::object>(callback);
                  slider->_onValueChanged = [pycb, slider]() {
                    py::gil_scoped_acquire acquire_gil;
                    (*pycb)(slider);
                  };
                }
              })
          .def_property(
              "fg_color",
              [](ui::intslider_ptr_t slider) -> fvec3 { //
                return slider->_fg_color;
              },
              [](ui::intslider_ptr_t slider, fvec3 c) { //
                slider->_fg_color = c;
              })
          .def_property(
              "bg_color",
              [](ui::intslider_ptr_t slider) -> fvec3 { //
                return slider->_bg_color;
              },
              [](ui::intslider_ptr_t slider, fvec3 c) { //
                slider->_bg_color = c;
              })
          .def_property(
              "fill_color",
              [](ui::intslider_ptr_t slider) -> fvec3 { //
                return slider->_fill_color;
              },
              [](ui::intslider_ptr_t slider, fvec3 c) { //
                slider->_fill_color = c;
              })
          .def("setRange", [](ui::intslider_ptr_t slider, int min_val, int max_val) { //
            slider->setRange(min_val, max_val);
          });
  type_codec->registerStdCodec<ui::intslider_ptr_t>(intslider_type);
  /////////////////////////////////////////////////////////////////////////////////
  // FloatSlider
  auto floatslider_type = //
      py::class_<ui::FloatSlider, ui::Widget, ui::floatslider_ptr_t>(uimodule, "FloatSlider")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::floatslider_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec3>();
                auto min_val      = decoded_args[2].get<float>();
                auto max_val      = decoded_args[3].get<float>();
                auto value        = decoded_args[4].get<float>();
                auto slider       = std::make_shared<ui::FloatSlider>(name, color, min_val, max_val, value);
                return slider;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec3>();
                auto min_val      = decoded_args[2].get<float>();
                auto max_val      = decoded_args[3].get<float>();
                auto value        = decoded_args[4].get<float>();
                auto layoutitem   = lg->makeChild<ui::FloatSlider>(name, color, min_val, max_val, value);
                return layoutitem.as_shared();
              })
          .def_property(
              "value",
              [](ui::floatslider_ptr_t slider) -> float { //
                return slider->value();
              },
              [](ui::floatslider_ptr_t slider, float val) { //
                slider->setValue(val);
              })
          .def_property(
              "log_mode",
              [](ui::floatslider_ptr_t slider) -> bool { //
                return slider->logMode();
              },
              [](ui::floatslider_ptr_t slider, bool log) { //
                slider->setLogMode(log);
              })
          .def_property(
              "onValueChanged",
              [](ui::floatslider_ptr_t slider) -> py::object { //
                return py::none();
              },
              [](ui::floatslider_ptr_t slider, py::object callback) { //
                if (callback.is_none()) {
                  slider->_onValueChanged = nullptr;
                } else {
                  auto pycb               = std::make_shared<py::object>(callback);
                  slider->_onValueChanged = [pycb, slider]() {
                    py::gil_scoped_acquire acquire_gil;
                    (*pycb)(slider);
                  };
                }
              })
          .def_property(
              "fg_color",
              [](ui::floatslider_ptr_t slider) -> fvec3 { //
                return slider->_fg_color;
              },
              [](ui::floatslider_ptr_t slider, fvec3 c) { //
                slider->_fg_color = c;
              })
          .def_property(
              "bg_color",
              [](ui::floatslider_ptr_t slider) -> fvec3 { //
                return slider->_bg_color;
              },
              [](ui::floatslider_ptr_t slider, fvec3 c) { //
                slider->_bg_color = c;
              })
          .def_property(
              "fill_color",
              [](ui::floatslider_ptr_t slider) -> fvec3 { //
                return slider->_fill_color;
              },
              [](ui::floatslider_ptr_t slider, fvec3 c) { //
                slider->_fill_color = c;
              })
          .def("setRange", [](ui::floatslider_ptr_t slider, float min_val, float max_val) { //
            slider->setRange(min_val, max_val);
          });
  type_codec->registerStdCodec<ui::floatslider_ptr_t>(floatslider_type);
  /////////////////////////////////////////////////////////////////////////////////
  // ComboBox
  auto combobox_type = //
      py::class_<ui::ComboBox, ui::Widget, ui::combobox_ptr_t>(uimodule, "ComboBox")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::combobox_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec3>();
                auto combo        = std::make_shared<ui::ComboBox>(name, color);
                return combo;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec3>();
                auto layoutitem   = lg->makeChild<ui::ComboBox>(name, color);
                return layoutitem.as_shared();
              })
          .def(
              "addItem",
              [](ui::combobox_ptr_t combo, std::string item) { //
                combo->addItem(item);
              })
          .def(
              "setItems",
              [](ui::combobox_ptr_t combo, py::list items) { //
                std::vector<std::string> item_vec;
                for (auto item : items) {
                  item_vec.push_back(py::cast<std::string>(item));
                }
                combo->setItems(item_vec);
              })
          .def_property(
              "selected_index",
              [](ui::combobox_ptr_t combo) -> int { //
                return combo->selectedIndex();
              },
              [](ui::combobox_ptr_t combo, int idx) { //
                combo->setSelectedIndex(idx);
              })
          .def(
              "selectedItem",
              [](ui::combobox_ptr_t combo) -> std::string { //
                return combo->selectedItem();
              })
          .def_property(
              "onSelectionChanged",
              [](ui::combobox_ptr_t combo) -> py::object { //
                return py::none();
              },
              [](ui::combobox_ptr_t combo, py::object callback) { //
                if (callback.is_none()) {
                  combo->_onSelectionChanged = nullptr;
                } else {
                  auto pycb                  = std::make_shared<py::object>(callback);
                  combo->_onSelectionChanged = [pycb, combo]() {
                    py::gil_scoped_acquire acquire_gil;
                    (*pycb)(combo);
                  };
                }
              })
          .def_property(
              "fg_color",
              [](ui::combobox_ptr_t combo) -> fvec3 { //
                return combo->_fg_color;
              },
              [](ui::combobox_ptr_t combo, fvec3 c) { //
                combo->_fg_color = c;
              })
          .def_property(
              "bg_color",
              [](ui::combobox_ptr_t combo) -> fvec3 { //
                return combo->_bg_color;
              },
              [](ui::combobox_ptr_t combo, fvec3 c) { //
                combo->_bg_color = c;
              })
          .def_property(
              "button_color",
              [](ui::combobox_ptr_t combo) -> fvec3 { //
                return combo->_button_color;
              },
              [](ui::combobox_ptr_t combo, fvec3 c) { //
                combo->_button_color = c;
              });
  type_codec->registerStdCodec<ui::combobox_ptr_t>(combobox_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto imgview_type = //
      py::class_<ui::ImageView, ui::Widget, ui::imgview_ptr_t>(uimodule, "ImageView")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::imgview_ptr_t { //
                auto decoded_args       = type_codec->decodeList(py_args);
                auto name               = decoded_args[0].get<std::string>();
                auto defcolor           = decoded_args[1].get<fvec4>();
                auto imgview            = std::make_shared<ui::ImageView>(name);
                imgview->_default_color = defcolor;
                return imgview;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::ImageView>(name);
                return layoutitem.as_shared();
              })
          .def_property(
              "default_color",
              [](ui::imgview_ptr_t imgview) -> fvec4 { //
                return imgview->_default_color;
              },
              [](ui::imgview_ptr_t imgview, fvec4 c) { //
                imgview->_default_color = c;
              })
          .def_property(
              "maintain_aspect_ratio",
              [](ui::imgview_ptr_t imgview) -> bool { //
                return imgview->_maintain_aspect_ratio;
              },
              [](ui::imgview_ptr_t imgview, bool b) { //
                imgview->_maintain_aspect_ratio = b;
              })
          .def(
              "setImage",
              [](ui::imgview_ptr_t imgview, image_ptr_t img) { //
                imgview->setImage(img);
              })
          .def("setImageProvider", [](ui::imgview_ptr_t imgview, image_provider_ptr_t imgprov) { //
            imgview->setImageProvider(imgprov);
          });
  type_codec->registerStdCodec<ui::imgview_ptr_t>(imgview_type);
  /////////////////////////////////////////////////////////////////////////////////
  pyinit_ui_layout(uimodule);
  pyinit_ui_ged(uimodule);
  pyinit_ui_box(uimodule);
}

} // namespace ork::lev2