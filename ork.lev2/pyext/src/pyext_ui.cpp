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
#include <ork/lev2/ui/alignmentgroup.h>
#include <ork/lev2/ui/split.h>
#include <ork/lev2/ui/lineedit.h>
#include <ork/lev2/ui/button.h>
#include <ork/lev2/ui/imagebutton.h>
#include <ork/lev2/ui/checkbox.h>
#include <ork/lev2/ui/slider.h>
#include <ork/lev2/ui/combobox.h>
#include <ork/lev2/ui/coloredit.h>
#include <ork/lev2/ui/imgview.h>
#include <ork/lev2/ui/graphview.h>
#include <ork/lev2/ui/logger_group.h>
#include <ork/lev2/ui/logger_ui_backend.h>
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/lev2/ui/popups.inl>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/lev2/gfx/image.h>
#include <ork/util/logger.h>
#include <ork/profiling.inl>
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_ui_ged(py::module& module_ui);
void pyinit_ui_layout(py::module& module_ui);
void pyinit_ui_box(py::module& module_ui);
void pyinit_ui_style(py::module& module_ui);
void pyinit_ui_dynagrid(py::module& module_ui);
void pyinit_ui_sdfshape(py::module& module_ui);
void pyinit_ui_outliner(py::module& module_ui);
void pyinit_ui_property_sheet(py::module& module_ui);

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
  uimodule.def(
      "popupFolderDialogAsync",
      [](std::string title,        //
         std::string default_path, //
         py::function callback) {  //
        // Validate callback
        if (callback.is_none()) {
          throw std::runtime_error("popupFolderDialogAsync: callback cannot be None");
        }

        // Store Python callback in shared_ptr for thread-safe capture
        auto callback_ptr = std::make_shared<py::function>(callback);

        // Enqueue blocking tinyfd call to background thread
        py::gil_scoped_release release;
        opq::concurrentQueue()->enqueue([title, default_path, callback_ptr]() mutable {
          std::string result = ui::popupFolderDialog(title, default_path);
          py::gil_scoped_acquire acquire;
          (*callback_ptr)(result);
          callback_ptr = nullptr;
        });
      });
  /////////////////////////////////////////////////////////////////////////////////
  auto uicontext_type = //
      py::class_<ui::Context, ui::context_ptr_t>(uimodule, "Context")
          .def(py::init<>())
          .def_property_readonly("hasKeyboardFocus", [](ui::context_ptr_t uictx) -> bool { return uictx->hasKeyboardFocus(); })
          .def("hasMouseFocus", [](ui::context_ptr_t uictx, uiwidget_ptr_t w) -> bool { return uictx->hasMouseFocus(w.get()); })
          .def("dumpWidgets", [](ui::context_ptr_t uictx, std::string label) { uictx->dumpWidgets(label); })
          .def("isKeyDown", [](ui::context_ptr_t uictx, int keycode) -> bool { return uictx->isKeyDown(keycode); })
          .def_property(
              "debug_event_routing",
              [](ui::context_ptr_t uictx) -> bool { return uictx->_debug_event_routing; },
              [](ui::context_ptr_t uictx, bool val) { uictx->_debug_event_routing = val; })
          .def_property(
              "theme_engine",
              [](ui::context_ptr_t uictx) -> ui::themeengine_ptr_t { return uictx->_theme_engine; },
              [](ui::context_ptr_t uictx, ui::themeengine_ptr_t engine) { uictx->_theme_engine = engine; });
  ;
  type_codec->registerStdCodec<ui::context_ptr_t>(uicontext_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto uievent_type = //
      py::class_<ui::Event, ui::event_ptr_t>(uimodule, "Event")
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
  auto drwev_type = py::class_<ui::DrawEvent, uidrawevent_ptr_t>(uimodule, "DrawEvent")          //
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
                widget->_uservars->makeValueForKey<py::object>("_hold_ev_callback", callback);
                widget->_evhandler = [widget](ui::event_constptr_t ev) -> ui::HandlerResult {
                  ui::HandlerResult rval;
                  EASY_BLOCK("pyui::evh1", profiler::colors::Red);
                  py::gil_scoped_acquire acquire_gil;
                  EASY_END_BLOCK;
                  EASY_BLOCK("pyui::evh2", profiler::colors::Red);
                  auto cb = widget->_uservars->typedValueForKey<py::object>("_hold_ev_callback").value();
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
              "fixed_width",
              [](uiwidget_ptr_t widget) -> int { //
                return widget->_fixed_width;
              },
              [](uiwidget_ptr_t widget, int fw) { //
                widget->_fixed_width = fw;
              })
          .def_property(
              "fixed_height",
              [](uiwidget_ptr_t widget) -> int { //
                return widget->_fixed_height;
              },
              [](uiwidget_ptr_t widget, int fh) { //
                widget->_fixed_height = fh;
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
          .def_property_readonly(
              "size",
              [](uiwidget_ptr_t widget) -> fvec2 { //
                return fvec2(float(widget->width()), float(widget->height()));
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
          .def(
              "getUserVar",
              [type_codec](uiwidget_ptr_t widget, std::string key) -> py::object { //
                py::object rval;
                if (widget->_uservars->hasKey(key)) {
                  rval = type_codec->encode(widget->_uservars->valueForKey(key));
                }
                return rval;
              })

          .def_property(
              "label_font",
              [](uiwidget_ptr_t widget) -> font_ptr_t { //
                return widget->_label_font;
              },
              [](uiwidget_ptr_t widget, font_ptr_t f) { //
                widget->_label_font = f;
              })
          .def_property_readonly(
              "uservars",
              [](uiwidget_ptr_t widget) -> varmap::varmap_ptr_t { //
                return widget->_uservars;
              });
  type_codec->registerStdCodec<uiwidget_ptr_t>(widget_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto group_type = //
      py::class_<ui::Group, ui::Widget, uigroup_ptr_t>(uimodule, "Group")
          .def("updateLayout", [](uigroup_ptr_t grp) { grp->DoLayout(); })
          .def_property(
              "margin",
              [](uigroup_ptr_t grid) -> int { //
                return grid->margin();
              },
              [](uigroup_ptr_t grid, int m) { //
                grid->setMargin(m);
              })
          .def("makeChild2", [](uigroup_ptr_t grp, py::kwargs kwargs) -> ui::widget_ptr_t { //
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
              grp->addChild(rval);
            }
            return rval;
          });
  type_codec->registerStdCodec<uigroup_ptr_t>(group_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto surface_type = //
      py::class_<ui::Surface, ui::Group, uisurface_ptr_t>(uimodule, "Surface")
          .def(
              "onPostRender",
              [](uisurface_ptr_t surface, py::object callback) { //
                OrkAssert(py::hasattr(callback, "__call__"));
                surface->_uservars->makeValueForKey<py::object>("_hold_postrender_callback", callback);
                surface->_postRenderCallback = [surface]() {
                  py::gil_scoped_acquire acquire_gil;
                  auto cb = surface->_uservars->typedValueForKey<py::object>("_hold_postrender_callback");
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
  // GraphSeries - data storage for graph plots
  /////////////////////////////////////////////////////////////////////////////////
  auto graphseries_type = //
      py::class_<ui::GraphSeries, ui::graphseries_ptr_t>(uimodule, "GraphSeries")
          .def("addSample", &ui::GraphSeries::addSample)
          .def("clearSamples", &ui::GraphSeries::clearSamples)
          .def("setMaxSamples", &ui::GraphSeries::setMaxSamples)
          .def("sampleCount", &ui::GraphSeries::sampleCount)
          .def("getSample", &ui::GraphSeries::getSample)
          .def_readwrite("name", &ui::GraphSeries::_name)
          .def_readwrite("color", &ui::GraphSeries::_color)
          .def_readwrite("visible", &ui::GraphSeries::_visible)
          .def_readwrite("min_value", &ui::GraphSeries::_min_value)
          .def_readwrite("max_value", &ui::GraphSeries::_max_value)
          .def_readwrite("window_size", &ui::GraphSeries::_window_size)
          .def_readwrite("vertical_scale", &ui::GraphSeries::_vertical_scale);
  type_codec->registerStdCodec<ui::graphseries_ptr_t>(graphseries_type);
  /////////////////////////////////////////////////////////////////////////////////
  // GraphChannel - contains multiple series
  /////////////////////////////////////////////////////////////////////////////////
  auto graphchannel_type = //
      py::class_<ui::GraphChannel, ui::graphchannel_ptr_t>(uimodule, "GraphChannel")
          .def("addSeries", &ui::GraphChannel::addSeries)
          .def("removeSeries", &ui::GraphChannel::removeSeries)
          .def("getSeries", &ui::GraphChannel::getSeries)
          .def_readwrite("name", &ui::GraphChannel::_name)
          .def_readwrite("color", &ui::GraphChannel::_color)
          .def_readwrite("visible", &ui::GraphChannel::_visible);
  type_codec->registerStdCodec<ui::graphchannel_ptr_t>(graphchannel_type);
  /////////////////////////////////////////////////////////////////////////////////
  // GraphView - widget for plotting time-series data
  /////////////////////////////////////////////////////////////////////////////////
  auto graphview_type = //
      py::class_<ui::GraphView, ui::Surface, ui::graphview_ptr_t>(uimodule, "GraphView")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::graphview_ptr_t { //
                auto graphview = std::make_shared<ui::GraphView>();
                return graphview;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto layoutitem = lg->makeChild<ui::GraphView>();
                return layoutitem.as_shared();
              })
          .def_static(
              "uigridfactory",
              [type_codec](uilayoutgroup_ptr_t lg, ui::gridparams_ptr_t gp, py::list py_args) -> py::list { //
                auto layoutitems = lg->makeGridOfWidgets<ui::GraphView>(gp);
                py::list rval;
                for (auto item : layoutitems) {
                  rval.append(item.as_shared());
                }
                return rval;
              })
          .def("channel", &ui::GraphView::channel)
          .def_readwrite("clear_color", &ui::GraphView::_clearColor);
  type_codec->registerStdCodec<ui::graphview_ptr_t>(graphview_type);
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
              [type_codec](uilayoutgroup_ptr_t lg, ui::gridparams_ptr_t gp, py::list py_args) -> py::list { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitems  = lg->makeGridOfWidgets<ui::SceneGraphViewport>(gp, name);
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
              [](uisgviewport_ptr_t sgview, lev2::scenegraph::scene_ptr_t sg) { //
                return sgview->bindSceneGraph(sg);
              })
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
              })
          //////////////////////////////////
          // Camera event handler - for EzUiCam and similar
          .def_property(
              "camera_evhandler",
              [](uisgviewport_ptr_t sgview) -> py::object { //
                return py::none();
              },
              [](uisgviewport_ptr_t sgview, py::object callback) { //
                sgview->_uservars->makeValueForKey<py::object>("_hold_camera_ev_callback", callback);
                sgview->_camera_evhandler = [sgview](ui::event_constptr_t ev) -> ui::HandlerResult {
                  ui::HandlerResult rval;
                  py::gil_scoped_acquire acquire_gil;
                  auto cb = sgview->_uservars->typedValueForKey<py::object>("_hold_camera_ev_callback").value();
                  if (cb) {
                    auto pyrval = cb(ev);
                    if (pyrval) {
                      rval = py::cast<ui::HandlerResult>(pyrval);
                    }
                  }
                  return rval;
                };
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
          .def(
              "makeChild",
              [](ui::tabwidget_ptr_t tabs, py::kwargs kwargs) -> ui::widget_ptr_t { //
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
          .def(
              "setActiveTab",
              [](ui::tabwidget_ptr_t tabs, int index) { //
                tabs->setActiveTab(index);
              })
          .def(
              "setActiveTabByName",
              [](ui::tabwidget_ptr_t tabs, const std::string& name) { //
                tabs->setActiveTabByName(name);
              })
          .def(
              "getActiveTab",
              [](ui::tabwidget_ptr_t tabs) -> int { //
                return tabs->getActiveTab();
              })
          .def(
              "getTabCount",
              [](ui::tabwidget_ptr_t tabs) -> int { //
                return tabs->getTabCount();
              })
          .def_property(
              "tabbar_background_color",
              [](ui::tabwidget_ptr_t tabs) -> fvec4 { //
                return tabs->_tabBarBackground;
              },
              [](ui::tabwidget_ptr_t tabs, fvec4 c) { //
                tabs->_tabBarBackground = c;
              })
          .def_property(
              "content_background",
              [](ui::tabwidget_ptr_t tabs) -> fvec4 { //
                return tabs->_contentBackground;
              },
              [](ui::tabwidget_ptr_t tabs, fvec4 c) { //
                tabs->_contentBackground = c;
              })
          .def_property(
              "draw_tabs",
              [](ui::tabwidget_ptr_t tabs) -> bool { //
                return tabs->getShowTabs();
              },
              [](ui::tabwidget_ptr_t tabs, bool show) { //
                tabs->setShowTabs(show);
              })
          .def_property(
              "draw_background",
              [](ui::tabwidget_ptr_t tabs) -> bool { //
                return tabs->_draw_background;
              },
              [](ui::tabwidget_ptr_t tabs, bool b) { //
                tabs->_draw_background = b;
              })
          .def_property(
              "font",
              [](ui::tabwidget_ptr_t tabs) -> lev2::font_ptr_t { //
                return tabs->_tab_font;
              },
              [](ui::tabwidget_ptr_t tabs, lev2::font_ptr_t f) { //
                tabs->_tab_font = f;
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
              "item_height",
              [](ui::vpack_ptr_t vpack) -> int { //
                return vpack->_item_height;
              },
              [](ui::vpack_ptr_t vpack, int h) { //
                vpack->_item_height = h;
              })
          .def_property(
              "uniform",
              [](ui::vpack_ptr_t vpack) -> bool { //
                return vpack->_uniform;
              },
              [](ui::vpack_ptr_t vpack, bool b) { //
                vpack->_uniform = b;
              })
          .def_property(
              "fill",
              [](ui::vpack_ptr_t vpack) -> bool { //
                return vpack->_fill;
              },
              [](ui::vpack_ptr_t vpack, bool b) { //
                vpack->_fill = b;
              })
          .def_property(
              "bg_color",
              [](ui::vpack_ptr_t vpack) -> fvec4 { //
                return vpack->_bgcolor;
              },
              [](ui::vpack_ptr_t vpack, fvec4 b) { //
                vpack->_bgcolor = b;
              })
          .def_property(
              "draw_background",
              [](ui::vpack_ptr_t vpack) -> bool { return vpack->_draw_background; },
              [](ui::vpack_ptr_t vpack, bool val) { vpack->_draw_background = val; });
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
              })
          .def_property(
              "bg_color",
              [](ui::hpack_ptr_t hpack) -> fvec4 { //
                return hpack->_bgcolor;
              },
              [](ui::hpack_ptr_t hpack, fvec4 b) { //
                hpack->_bgcolor = b;
              })
          .def_property(
              "draw_background",
              [](ui::hpack_ptr_t hpack) -> bool { return hpack->_draw_background; },
              [](ui::hpack_ptr_t hpack, bool val) { hpack->_draw_background = val; });
  type_codec->registerStdCodec<ui::hpack_ptr_t>(hpack_type);
  /////////////////////////////////////////////////////////////////////////////////
  // AlignmentGroup
  auto alignmentgroup_type = //
      py::class_<ui::AlignmentGroup, ui::Group, ui::alignmentgroup_ptr_t>(uimodule, "AlignmentGroup")
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::AlignmentGroup>(name);
                return layoutitem.as_shared();
              })
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::alignmentgroup_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto group        = std::make_shared<ui::AlignmentGroup>(name);
                return group;
              })
          .def(
              "makeChild",
              [](ui::alignmentgroup_ptr_t group, py::kwargs kwargs) -> ui::widget_ptr_t { //
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
                  group->addChild(rval);
                }
                return rval;
              })
          .def_property(
              "alignment",
              [](ui::alignmentgroup_ptr_t group) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>(uint64_t(group->_alignment));
              },
              [](ui::alignmentgroup_ptr_t group, crcstring_ptr_t align) { //
                group->_alignment = ui::Alignment(align->hashed());
              })
          .def_property(
              "width_proportional",
              [](ui::alignmentgroup_ptr_t group) -> float { return group->_width_proportional; },
              [](ui::alignmentgroup_ptr_t group, float val) { group->_width_proportional = val; })
          .def_property(
              "height_proportional",
              [](ui::alignmentgroup_ptr_t group) -> float { return group->_height_proportional; },
              [](ui::alignmentgroup_ptr_t group, float val) { group->_height_proportional = val; })
          .def_property(
              "min_width_pixels",
              [](ui::alignmentgroup_ptr_t group) -> int { return group->_min_width_pixels; },
              [](ui::alignmentgroup_ptr_t group, int val) { group->_min_width_pixels = val; })
          .def_property(
              "max_width_pixels",
              [](ui::alignmentgroup_ptr_t group) -> int { return group->_max_width_pixels; },
              [](ui::alignmentgroup_ptr_t group, int val) { group->_max_width_pixels = val; })
          .def_property(
              "min_height_pixels",
              [](ui::alignmentgroup_ptr_t group) -> int { return group->_min_height_pixels; },
              [](ui::alignmentgroup_ptr_t group, int val) { group->_min_height_pixels = val; })
          .def_property(
              "max_height_pixels",
              [](ui::alignmentgroup_ptr_t group) -> int { return group->_max_height_pixels; },
              [](ui::alignmentgroup_ptr_t group, int val) { group->_max_height_pixels = val; })
          .def_property(
              "maintain_aspect_ratio",
              [](ui::alignmentgroup_ptr_t group) -> float { return group->_maintain_aspect_ratio; },
              [](ui::alignmentgroup_ptr_t group, float val) { group->_maintain_aspect_ratio = val; })
          .def_property(
              "bg_color",
              [](ui::alignmentgroup_ptr_t group) -> fvec4 { return group->_bgcolor; },
              [](ui::alignmentgroup_ptr_t group, fvec4 color) { group->_bgcolor = color; })
          .def_property(
              "draw_background",
              [](ui::alignmentgroup_ptr_t group) -> bool { return group->_draw_background; },
              [](ui::alignmentgroup_ptr_t group, bool val) { group->_draw_background = val; });
  type_codec->registerStdCodec<ui::alignmentgroup_ptr_t>(alignmentgroup_type);
  /////////////////////////////////////////////////////////////////////////////////
  // HorizontalSplit
  auto hsplit_type = //
      py::class_<ui::HorizontalSplit, ui::Group, ui::hsplit_ptr_t>(uimodule, "HorizontalSplit")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::hsplit_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto split        = std::make_shared<ui::HorizontalSplit>(name);
                return split;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::HorizontalSplit>(name);
                return layoutitem.as_shared();
              })
          .def(
              "makeChild",
              [](ui::hsplit_ptr_t hsplit, py::kwargs kwargs) -> ui::widget_ptr_t { //
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
                  hsplit->addChild(rval);
                }
                return rval;
              })
          .def_property(
              "split_ratio",
              [](ui::hsplit_ptr_t split) -> float { //
                return split->_split_ratio;
              },
              [](ui::hsplit_ptr_t split, float ratio) { //
                split->_split_ratio = ratio;
                // split->ReLayout();
              });
  type_codec->registerStdCodec<ui::hsplit_ptr_t>(hsplit_type);
  /////////////////////////////////////////////////////////////////////////////////
  // VerticalSplit
  auto vsplit_type = //
      py::class_<ui::VerticalSplit, ui::Group, ui::vsplit_ptr_t>(uimodule, "VerticalSplit")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::vsplit_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto split        = std::make_shared<ui::VerticalSplit>(name);
                return split;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::VerticalSplit>(name);
                return layoutitem.as_shared();
              })
          .def(
              "makeChild",
              [](ui::vsplit_ptr_t vsplit, py::kwargs kwargs) -> ui::widget_ptr_t { //
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
                  vsplit->addChild(rval);
                }
                return rval;
              })
          .def_property(
              "split_ratio",
              [](ui::vsplit_ptr_t split) -> float { //
                return split->_split_ratio;
              },
              [](ui::vsplit_ptr_t split, float ratio) { //
                split->_split_ratio = ratio;
                // split->ReLayout();
              });
  type_codec->registerStdCodec<ui::vsplit_ptr_t>(vsplit_type);
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
          .def(
              "setUpTexture",
              [](ui::button_ptr_t btn, lev2::texture_ptr_t tex) { //
                btn->setUpTexture(tex);
              })
          .def(
              "setDownTexture",
              [](ui::button_ptr_t btn, lev2::texture_ptr_t tex) { //
                btn->setDownTexture(tex);
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
                  btn->_onPressed = [pycb, btn]() {
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
              "down_color",
              [](ui::button_ptr_t btn) -> fvec3 { //
                return btn->_down_color;
              },
              [](ui::button_ptr_t btn, fvec3 c) { //
                btn->_down_color = c;
              });
  type_codec->registerStdCodec<ui::button_ptr_t>(button_type);
  /////////////////////////////////////////////////////////////////////////////////
  // ImageButton
  auto imagebutton_type = //
      py::class_<ui::ImageButton, ui::Widget, ui::imagebutton_ptr_t>(uimodule, "ImageButton")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::imagebutton_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto button       = std::make_shared<ui::ImageButton>(name);
                return button;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::ImageButton>(name);
                return layoutitem.as_shared();
              })
          .def_property(
              "inactive_image",
              [](ui::imagebutton_ptr_t btn) -> py::object { //
                if (btn->_inactive_image_provider) {
                  return py::cast(btn->_inactive_image_provider);
                } else if (btn->_inactive_image) {
                  return py::cast(btn->_inactive_image);
                }
                return py::none();
              },
              [](ui::imagebutton_ptr_t btn, py::object obj) { //
                if (obj.is_none()) {
                  btn->setInactiveImage(nullptr);
                  btn->setInactiveImageProvider(nullptr);
                } else {
                  // Try image_ptr_t first
                  try {
                    auto img = obj.cast<lev2::image_ptr_t>();
                    btn->setInactiveImage(img);
                    btn->setInactiveImageProvider(nullptr);
                    return;
                  } catch (...) {
                  }
                  // Try image_provider_ptr_t
                  try {
                    auto prov = obj.cast<lev2::image_provider_ptr_t>();
                    btn->setInactiveImageProvider(prov);
                    return;
                  } catch (...) {
                  }
                }
              })
          .def_property(
              "active_released_image",
              [](ui::imagebutton_ptr_t btn) -> py::object { //
                if (btn->_active_released_image_provider) {
                  return py::cast(btn->_active_released_image_provider);
                } else if (btn->_active_released_image) {
                  return py::cast(btn->_active_released_image);
                }
                return py::none();
              },
              [](ui::imagebutton_ptr_t btn, py::object obj) { //
                if (obj.is_none()) {
                  btn->setActiveReleasedImage(nullptr);
                  btn->setActiveReleasedImageProvider(nullptr);
                } else {
                  // Try image_ptr_t first
                  try {
                    auto img = obj.cast<lev2::image_ptr_t>();
                    btn->setActiveReleasedImage(img);
                    btn->setActiveReleasedImageProvider(nullptr);
                    return;
                  } catch (...) {
                  }
                  // Try image_provider_ptr_t
                  try {
                    auto prov = obj.cast<lev2::image_provider_ptr_t>();
                    btn->setActiveReleasedImageProvider(prov);
                    return;
                  } catch (...) {
                  }
                }
              })
          .def_property(
              "active_pressed_image",
              [](ui::imagebutton_ptr_t btn) -> py::object { //
                if (btn->_active_pressed_image_provider) {
                  return py::cast(btn->_active_pressed_image_provider);
                } else if (btn->_active_pressed_image) {
                  return py::cast(btn->_active_pressed_image);
                }
                return py::none();
              },
              [](ui::imagebutton_ptr_t btn, py::object obj) { //
                if (obj.is_none()) {
                  btn->setActivePressedImage(nullptr);
                  btn->setActivePressedImageProvider(nullptr);
                } else {
                  // Try image_ptr_t first
                  try {
                    auto img = obj.cast<lev2::image_ptr_t>();
                    btn->setActivePressedImage(img);
                    btn->setActivePressedImageProvider(nullptr);
                    return;
                  } catch (...) {
                  }
                  // Try image_provider_ptr_t
                  try {
                    auto prov = obj.cast<lev2::image_provider_ptr_t>();
                    btn->setActivePressedImageProvider(prov);
                    return;
                  } catch (...) {
                  }
                }
              })
          .def_property(
              "inactive_blend_mode",
              [](ui::imagebutton_ptr_t btn) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>(uint64_t(btn->_inactive_blend_mode));
              },
              [](ui::imagebutton_ptr_t btn, crcstring_ptr_t bm) { //
                btn->_inactive_blend_mode = lev2::BlendingMacro(bm->hashed());
              })
          .def_property(
              "active_released_blend_mode",
              [](ui::imagebutton_ptr_t btn) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>(uint64_t(btn->_active_released_blend_mode));
              },
              [](ui::imagebutton_ptr_t btn, crcstring_ptr_t bm) { //
                btn->_active_released_blend_mode = lev2::BlendingMacro(bm->hashed());
              })
          .def_property(
              "active_pressed_blend_mode",
              [](ui::imagebutton_ptr_t btn) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>(uint64_t(btn->_active_pressed_blend_mode));
              },
              [](ui::imagebutton_ptr_t btn, crcstring_ptr_t bm) { //
                btn->_active_pressed_blend_mode = lev2::BlendingMacro(bm->hashed());
              })
          .def_property(
              "preserve_aspect_ratio",
              [](ui::imagebutton_ptr_t btn) -> bool { //
                return btn->_preserve_aspect_ratio;
              },
              [](ui::imagebutton_ptr_t btn, bool val) { //
                btn->_preserve_aspect_ratio = val;
              })
          .def_property(
              "bgcolor",
              [](ui::imagebutton_ptr_t btn) -> fvec4 { //
                return btn->_bgcolor;
              },
              [](ui::imagebutton_ptr_t btn, fvec4 c) { //
                btn->_bgcolor = c;
              })
          .def_property(
              "onPressed",
              [](ui::imagebutton_ptr_t btn) -> py::object { //
                return py::none();
              },
              [](ui::imagebutton_ptr_t btn, py::object callback) { //
                if (callback.is_none()) {
                  btn->_onPressed = nullptr;
                } else {
                  auto pycb       = std::make_shared<py::object>(callback);
                  btn->_onPressed = [pycb, btn]() {
                    py::gil_scoped_acquire acquire_gil;
                    (*pycb)(btn);
                  };
                }
              });
  type_codec->registerStdCodec<ui::imagebutton_ptr_t>(imagebutton_type);
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
          .def_property(
              "update_on_drag",
              [](ui::floatslider_ptr_t slider) -> bool { return slider->_update_on_drag; },
              [](ui::floatslider_ptr_t slider, bool b) { slider->_update_on_drag = b; })
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
  // ColorEdit
  auto coloredit_type = //
      py::class_<ui::ColorEdit, ui::Widget, ui::coloredit_ptr_t>(uimodule, "ColorEdit")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::coloredit_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto coloredit    = std::make_shared<ui::ColorEdit>(name, color);
                return coloredit;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto layoutitem   = lg->makeChild<ui::ColorEdit>(name, color);
                return layoutitem.as_shared();
              })
          .def_property(
              "currentColor",
              [](ui::coloredit_ptr_t ce) -> fvec4 { //
                return ce->_currentColor;
              },
              [](ui::coloredit_ptr_t ce, fvec4 c) { //
                ce->_currentColor = c;
                auto hsv          = c.xyz().convertRgbToHsv();
                ce->_currentColorFullBright.setHSV(hsv.x, hsv.y, 1.0);
                ce->_hue        = hsv.x;
                ce->_saturation = hsv.y;
                ce->_intensity  = hsv.z;
              })
          .def_property(
              "originalColor",
              [](ui::coloredit_ptr_t ce) -> fvec4 { //
                return ce->_originalColor;
              },
              [](ui::coloredit_ptr_t ce, fvec4 c) { //
                ce->_originalColor = c;
              })
          .def_property(
              "onColorChanged",
              [](ui::coloredit_ptr_t ce) -> py::object { //
                return py::none();
              },
              [type_codec](ui::coloredit_ptr_t ce, py::object callback) { //
                if (not callback.is_none()) {
                  auto pycb           = std::make_shared<py::object>(callback);
                  ce->_onColorChanged = [pycb, type_codec](fvec4 newcolor) {
                    py::gil_scoped_acquire acquire_gil;
                    auto encoded = type_codec->encode(newcolor);
                    (*pycb)(encoded);
                  };
                }
              });
  type_codec->registerStdCodec<ui::coloredit_ptr_t>(coloredit_type);
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
                fvec4 defcolor;
                if (decoded_args.size() > 1) {
                  defcolor = decoded_args[1].get<fvec4>();
                }
                auto layoutitem                          = lg->makeChild<ui::ImageView>(name);
                layoutitem.typedWidget()->_default_color = defcolor;
                return layoutitem.as_shared();
              })
          .def_static(
              "uigridfactory",
              [type_codec](uilayoutgroup_ptr_t lg, ui::gridparams_ptr_t gp, py::list py_args) -> py::list { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                fvec4 defcolor;
                if (decoded_args.size() > 1) {
                  defcolor = decoded_args[1].get<fvec4>();
                }
                auto layoutitems = lg->makeGridOfWidgets<ui::ImageView>(gp, name);
                py::list rval;
                for (auto& item : layoutitems) {
                  item.typedWidget()->_default_color = defcolor;
                  rval.append(item.as_shared());
                }
                return rval;
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
          .def_property(
              "generate_mipmaps",
              [](ui::imgview_ptr_t imgview) -> bool { //
                return imgview->_generate_mipmaps;
              },
              [](ui::imgview_ptr_t imgview, bool b) { //
                imgview->_generate_mipmaps = b;
              })
          .def_property(
              "fs_antialias",
              [](ui::imgview_ptr_t imgview) -> bool { //
                return imgview->_fs_antialias;
              },
              [](ui::imgview_ptr_t imgview, bool b) { //
                imgview->_fs_antialias = b;
              })
          .def(
              "setImage",
              [](ui::imgview_ptr_t imgview, image_ptr_t img) { //
                imgview->setImage(img);
              })
          .def(
              "setImageProvider",
              [](ui::imgview_ptr_t imgview, image_provider_ptr_t imgprov) { //
                imgview->setImageProvider(imgprov);
              })
          .def_property(
              "texture",
              [](ui::imgview_ptr_t imgview) -> lev2::texture_ptr_t { return imgview->_texture; },
              [](ui::imgview_ptr_t imgview, lev2::texture_ptr_t tex) { imgview->setTexture(tex); })
          .def_property(
              "primitive",
              [](ui::imgview_ptr_t imgview) -> meshutil::rigidprim_V12N12B12T8C4_ptr_t { return imgview->_img_mesh; },
              [](ui::imgview_ptr_t imgview, meshutil::rigidprim_V12N12B12T8C4_ptr_t p) { imgview->_img_mesh = p; })
          .def_property(
              "pipeline",
              [](ui::imgview_ptr_t imgview) -> lev2::fxpipeline_ptr_t { return imgview->_pipeline_override; },
              [](ui::imgview_ptr_t imgview, lev2::fxpipeline_ptr_t p) { imgview->_pipeline_override = p; })
          .def_property(
              "invert_aspect",
              [](ui::imgview_ptr_t imgview) -> bool { return imgview->_invert_aspect; },
              [](ui::imgview_ptr_t imgview, bool p) { imgview->_invert_aspect = p; })
          .def_property(
              "image_rot_180",
              [](ui::imgview_ptr_t imgview) -> bool { return imgview->_image_rot_180; },
              [](ui::imgview_ptr_t imgview, bool p) { imgview->_image_rot_180 = p; })
          .def_property(
              "flip_x",
              [](ui::imgview_ptr_t imgview) -> bool { return imgview->_image_flip_x; },
              [](ui::imgview_ptr_t imgview, bool p) { imgview->_image_flip_x = p; })
          .def_property(
              "flip_y",
              [](ui::imgview_ptr_t imgview) -> bool { return imgview->_image_flip_y; },
              [](ui::imgview_ptr_t imgview, bool p) { imgview->_image_flip_y = p; })
          .def_property(
              "image",
              [](ui::imgview_ptr_t imgview) -> py::object {
                if (imgview->_imgprovider) {
                  return py::cast(imgview->_imgprovider);
                } else if (imgview->_active_image) {
                  return py::cast(imgview->_active_image);
                }
                return py::none();
              },
              [](ui::imgview_ptr_t imgview, py::object obj) {
                if (obj.is_none()) {
                  imgview->setImage(nullptr);
                  imgview->setImageProvider(nullptr);
                  imgview->setTextureProvider(nullptr);
                } else {
                  // Try image_ptr_t first
                  try {
                    auto img = obj.cast<lev2::image_ptr_t>();
                    imgview->setImage(img);
                    return;
                  } catch (...) {
                  }
                  // Try image_provider_ptr_t
                  try {
                    auto prov = obj.cast<lev2::image_provider_ptr_t>();
                    imgview->setImageProvider(prov);
                    return;
                  } catch (...) {
                  }
                  // Try texture_provider_ptr_t (GPU-direct)
                  try {
                    auto texprov = obj.cast<lev2::texture_provider_ptr_t>();
                    imgview->setTextureProvider(texprov);
                    return;
                  } catch (...) {
                  }
                }
              });
  type_codec->registerStdCodec<ui::imgview_ptr_t>(imgview_type);
  /////////////////////////////////////////////////////////////////////////////////
  // LoggerUIBackend - routes log messages to UI widgets
  /////////////////////////////////////////////////////////////////////////////////
  auto loggerbackend_type = //
      py::class_<ui::LoggerUIBackend, ui::loggeruibackend_ptr_t>(uimodule, "LoggerUIBackend")
          .def_static(
              "create",
              []() -> logger_backend_ptr_t { //
                return ui::LoggerUIBackend::create();
              })
          .def(
              "registerGroup",
              [](ui::loggeruibackend_ptr_t backend, ui::loggergroup_ptr_t group) { //
                printf("LoggerUIBackend::registerGroup() called\n");
                printf("  backend ptr<%p>\n", (void*)backend.get());
                printf("  group <%p>\n", (void*)group.get());
                // backend->registerGroup(group);
              })
          .def("unregisterGroup", &ui::LoggerUIBackend::unregisterGroup);
  type_codec->registerStdCodec<ui::loggeruibackend_ptr_t>(loggerbackend_type);
  /////////////////////////////////////////////////////////////////////////////////
  // LoggerGroup - UI widget for displaying logs, status, and performance data
  /////////////////////////////////////////////////////////////////////////////////
  auto loggergroup_type = //
      py::class_<ui::LoggerGroup, ui::Group, ui::loggergroup_ptr_t>(uimodule, "LoggerGroup")
          .def_static(
              "create",
              [](const std::string& name, py::list py_channels) -> ui::loggergroup_ptr_t { //
                std::set<std::string> channels;
                for (auto item : py_channels) {
                  channels.insert(item.cast<std::string>());
                }
                return ui::LoggerGroup::create(name, channels);
              })
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::loggergroup_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                std::set<std::string> channels;

                // Handle second argument as Python list
                if (decoded_args.size() > 1 && py_args.size() > 1) {
                  py::list channel_list = py_args[1];
                  for (auto item : channel_list) {
                    channels.insert(item.cast<std::string>());
                  }
                }
                return ui::LoggerGroup::create(name, channels);
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                std::set<std::string> channels;

                // Handle second argument as Python list
                if (decoded_args.size() > 1 && py_args.size() > 1) {
                  py::list channel_list = py_args[1];
                  for (auto item : channel_list) {
                    channels.insert(item.cast<std::string>());
                  }
                }

                auto logger_group = ui::LoggerGroup::create(name, channels);
                lg->addChild(logger_group);

                // Create layout item manually since LoggerGroup isn't created via makeChild
                auto layoutitem     = std::make_shared<ui::LayoutItem<ui::LoggerGroup>>();
                layoutitem->_widget = logger_group;
                layoutitem->_layout = lg->_layout->childLayout(logger_group.get());
                return layoutitem;
              })
          .def("addChannel", &ui::LoggerGroup::addChannel)
          .def("removeChannel", &ui::LoggerGroup::removeChannel)
          .def("hasChannel", &ui::LoggerGroup::hasChannel)
          .def("setActiveTabByName", &ui::LoggerGroup::setActiveTabByName)
          .def("processQueuedMessages", &ui::LoggerGroup::processQueuedMessages)
          .def(
              "registerOnBackend",
              [](ui::loggergroup_ptr_t group, logger_backend_ptr_t backend) { ui::LoggerGroup::registerOnBackend(group, backend); })
          .def(
              "unregisterFromBackend",
              [](ui::loggergroup_ptr_t group, logger_backend_ptr_t backend) {
                ui::LoggerGroup::unregisterFromBackend(group, backend);
              })
          .def_property(
              "background_color",
              [](ui::loggergroup_ptr_t group) -> fvec4 { //
                return group->_background_color;
              },
              [](ui::loggergroup_ptr_t group, fvec4 c) { //
                group->_background_color = c;
              })
          .def_property_readonly(
              "tab_widget",
              [](ui::loggergroup_ptr_t group) -> ui::tabwidget_ptr_t { //
                return group->_tab_widget;
              })
          .def_property(
              "normalize_series",
              [](ui::loggergroup_ptr_t group) -> bool { //
                return group->_normalize_series;
              },
              [](ui::loggergroup_ptr_t group, bool normalize) { //
                group->_normalize_series = normalize;
              });
  type_codec->registerStdCodec<ui::loggergroup_ptr_t>(loggergroup_type);
  /////////////////////////////////////////////////////////////////////////////////
  pyinit_ui_layout(uimodule);
  pyinit_ui_ged(uimodule);
  pyinit_ui_box(uimodule);
  pyinit_ui_style(uimodule);
  pyinit_ui_dynagrid(uimodule);
  pyinit_ui_sdfshape(uimodule);
  pyinit_ui_outliner(uimodule);
  pyinit_ui_property_sheet(uimodule);
}

} // namespace ork::lev2
