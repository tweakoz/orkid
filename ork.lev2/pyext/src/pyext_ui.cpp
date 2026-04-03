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
#include <ork/lev2/ui/f32edit.h>
#include <ork/lev2/ui/intedit.h>
#include <ork/lev2/ui/button.h>
#include <ork/lev2/ui/imagebutton.h>
#include <ork/lev2/ui/checkbox.h>
#include <ork/lev2/ui/slider.h>
#include <ork/lev2/ui/combobox.h>
#include <ork/lev2/ui/coloredit.h>
#include <ork/lev2/ui/colorswatch.h>
#include <ork/lev2/ui/imgview.h>
#include <ork/lev2/ui/graphview.h>
#include <ork/lev2/ui/profilerview.h>
#include <ork/kernel/profiler.h>
#include <ork/lev2/ui/transformcurveeditor.h>
#include <ork/lev2/ui/logger_group.h>
#include <ork/lev2/ui/logger_ui_backend.h>
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/lev2/ui/popups.inl>
#include <ork/lev2/ui/prim_canvas.h>
#include <ork/lev2/ui/dockable_panel.h>
#include <ork/lev2/ui/border_frame.h>
#include <ork/lev2/ui/scroll_container.h>
#include <ork/lev2/ui/collapsable.h>
#include <ork/lev2/ui/dropdown_menu.h>
#include <ork/lev2/ui/choicelist_widget.h>
#include <ork/kernel/slashnode.h>
#include <ork/python/gil_safe_pyobj.h>
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
void pyinit_ui_filesystem(py::module& module_ui);
void pyinit_ui_toolbar(py::module& module_ui);

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
          .def_property_readonly("keyboard_focus_widget", [](ui::context_ptr_t uictx) -> uiwidget_ptr_t {
            auto w = uictx->keyboardFocusWidget().lock();
            return w ? std::const_pointer_cast<ui::Widget>(w) : nullptr;
          })
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
              [](ui::context_ptr_t uictx, ui::themeengine_ptr_t engine) { uictx->_theme_engine = engine; })
          .def_property(
              "top",
              [](ui::context_ptr_t uictx) -> ui::group_ptr_t { return uictx->_top; },
              [](ui::context_ptr_t uictx, ui::group_ptr_t top) {
                uictx->_top = top;
                if (top) {
                  top->_uicontext = uictx.get();
                }
              })
          .def(
              "pushOverlay",
              [](ui::context_ptr_t uictx, ui::widget_ptr_t widget, int x, int y, int w, int h,
                 bool dismiss_on_click_outside) {
                uictx->pushOverlay(widget, x, y, w, h, dismiss_on_click_outside);
              },
              py::arg("widget"), py::arg("x"), py::arg("y"), py::arg("w"), py::arg("h"),
              py::arg("dismiss_on_click_outside") = true)
          .def(
              "createOverlayWidget",
              [](ui::context_ptr_t uictx, py::object uiclass, py::list args) -> ui::widget_ptr_t {
                auto wfactory = uiclass.attr("wfactory");
                auto widget = py::cast<ui::widget_ptr_t>(wfactory(args));
                widget->_uicontext = uictx.get();
                if (auto group = dynamic_cast<ui::Group*>(widget.get())) {
                  group->_doOnParentChanged(nullptr);
                }
                return widget;
              },
              py::arg("uiclass"), py::arg("args"))
          .def("popOverlay", [](ui::context_ptr_t uictx) { uictx->popOverlay(); })
          .def("dismissAllOverlays", [](ui::context_ptr_t uictx) { uictx->dismissAllOverlays(); })
          .def("hasOverlays", [](ui::context_ptr_t uictx) -> bool { return uictx->hasOverlays(); })
          .def_property(
              "app_preview_handler",
              [](ui::context_ptr_t uictx) -> py::object { return py::none(); },
              [](ui::context_ptr_t uictx, py::object callback) {
                if (callback.is_none()) {
                  uictx->_appPreviewHandler = nullptr;
                } else {
                  uictx->_appPreviewHandler = [callback](ui::event_constptr_t ev) -> ui::HandlerResult {
                    ui::HandlerResult rval;
                    py::gil_scoped_acquire acquire;
                    try {
                      auto pyrval = callback(ev);
                      if (py::isinstance<ui::HandlerResult>(pyrval)) {
                        rval = py::cast<ui::HandlerResult>(pyrval);
                      }
                    } catch (const py::error_already_set& e) {
                      printf("app_preview_handler error: %s\n", e.what());
                    }
                    return rval;
                  };
                }
              });
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
          .def_property(
              "name",
              [](uiwidget_ptr_t widget) -> std::string { //
                return widget->GetName();
              },
              [](uiwidget_ptr_t widget, const std::string& name) { //
                widget->SetName(name);
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
              "clip_events",
              [](uiwidget_ptr_t widget) -> bool { //
                return widget->_clipEvents;
              },
              [](uiwidget_ptr_t widget, bool x) { //
                widget->_clipEvents = x;
              })
          .def_property(
              "enable",
              [](uiwidget_ptr_t widget) -> bool { //
                return widget->_enable;
              },
              [](uiwidget_ptr_t widget, bool x) { //
                widget->_enable = x;
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
              })
          .def_property_readonly(
              "has_keyboard_focus",
              [](uiwidget_ptr_t widget) -> bool { //
                if (widget->_uicontext) {
                  auto focus = widget->_uicontext->keyboardFocusWidget().lock();
                  return focus && focus.get() == widget.get();
                }
                return false;
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
          .def(
              "localToRoot",
              [](uiwidget_ptr_t widget, int lx, int ly) -> std::tuple<int, int> { //
                int rx, ry;
                widget->LocalToRoot(lx, ly, rx, ry);
                return std::make_tuple(rx, ry);
              },
              py::arg("lx"),
              py::arg("ly"),
              "Convert local coordinates to root (window) coordinates")
          .def(
              "rootToLocal",
              [](uiwidget_ptr_t widget, int rx, int ry) -> std::tuple<int, int> { //
                int lx, ly;
                widget->RootToLocal(rx, ry, lx, ly);
                return std::make_tuple(lx, ly);
              },
              py::arg("rx"),
              py::arg("ry"),
              "Convert root (window) coordinates to local coordinates")
          .def(
              "gpuInit",
              [](uiwidget_ptr_t widget, ctx_t ctx) { //
                widget->gpuInit(ctx.get());
              },
              py::arg("ctx"),
              "Initialize GPU resources for this widget");
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
              [](uisurface_ptr_t surface, fvec3 c) { surface->_clearColor = c; })
          //////////////////////////////////
          .def_property(
              "supersample",
              [](uisurface_ptr_t surface) -> int { return surface->_supersample; },
              [](uisurface_ptr_t surface, int ss) { surface->_supersample = ss; });
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
          .def("addSample", &ui::GraphSeries::addSample, py::arg("value"), py::arg("update_label") = true)
          .def("clearSamples", &ui::GraphSeries::clearSamples)
          .def("setMaxSamples", &ui::GraphSeries::setMaxSamples)
          .def("sampleCount", &ui::GraphSeries::sampleCount)
          .def("getSample", &ui::GraphSeries::getSample)
          .def("setSample", &ui::GraphSeries::setSample)
          .def_readwrite("name", &ui::GraphSeries::_name)
          .def_readwrite("color", &ui::GraphSeries::_color)
          .def_readwrite("visible", &ui::GraphSeries::_visible)
          .def_readwrite("min_value", &ui::GraphSeries::_min_value)
          .def_readwrite("max_value", &ui::GraphSeries::_max_value)
          .def_readwrite("window_size", &ui::GraphSeries::_window_size)
          .def_readwrite("vertical_scale", &ui::GraphSeries::_vertical_scale)
          .def_readwrite("currentValue", &ui::GraphSeries::_currentValue)
          .def_readwrite("use_fixed_range", &ui::GraphSeries::_use_fixed_range)
          .def_readwrite("fixed_min", &ui::GraphSeries::_fixed_min)
          .def_readwrite("fixed_max", &ui::GraphSeries::_fixed_max)
          .def("setFixedRange", [](ui::graphseries_ptr_t s, float min_val, float max_val) {
            s->_use_fixed_range = true;
            s->_fixed_min = min_val;
            s->_fixed_max = max_val;
          });
  type_codec->registerStdCodec<ui::graphseries_ptr_t>(graphseries_type);
  /////////////////////////////////////////////////////////////////////////////////
  // GraphChannel - contains multiple series
  /////////////////////////////////////////////////////////////////////////////////
  auto graphchannel_type = //
      py::class_<ui::GraphChannel, ui::graphchannel_ptr_t>(uimodule, "GraphChannel")
          .def("addSeries", &ui::GraphChannel::addSeries)
          .def("removeSeries", &ui::GraphChannel::removeSeries)
          .def("getSeries", &ui::GraphChannel::getSeries)
          .def("setSeriesOrder", &ui::GraphChannel::setSeriesOrder)
          .def_readwrite("name", &ui::GraphChannel::_name)
          .def_readwrite("color", &ui::GraphChannel::_color)
          .def_readwrite("visible", &ui::GraphChannel::_visible)
          .def_readwrite("stacked", &ui::GraphChannel::_stacked)
          .def_readwrite("min_series_height", &ui::GraphChannel::_min_series_height)
          .def_readwrite("lane_bgcolor", &ui::GraphChannel::_lane_bgcolor)
          .def_readwrite("lane_outline", &ui::GraphChannel::_lane_outline)
          .def_readwrite("lane_outline_color", &ui::GraphChannel::_lane_outline_color)
          .def_readwrite("bottom_margin", &ui::GraphChannel::_bottom_margin)
          .def_readwrite("max_event_samples", &ui::GraphChannel::_max_event_samples)
          .def("addEvent", &ui::GraphChannel::addEvent)
          .def("commitEventFrame", &ui::GraphChannel::commitEventFrame)
          .def("setEventTexture", &ui::GraphChannel::setEventTexture)
          .def("setEventImage", &ui::GraphChannel::setEventImage)
          .def("addHLine", &ui::GraphChannel::addHLine, py::arg("value"), py::arg("color"), py::arg("label") = "")
          .def("clearHLines", &ui::GraphChannel::clearHLines);
  type_codec->registerStdCodec<ui::graphchannel_ptr_t>(graphchannel_type);
  /////////////////////////////////////////////////////////////////////////////////
  // PrimCanvas - forward declaration (methods added later)
  /////////////////////////////////////////////////////////////////////////////////
  auto primcanvas_type = //
      py::class_<ui::PrimCanvas, ui::Surface, ui::prim_canvas_ptr_t>(uimodule, "PrimCanvas");
  type_codec->registerStdCodec<ui::prim_canvas_ptr_t>(primcanvas_type);
  /////////////////////////////////////////////////////////////////////////////////
  // GraphView - widget for plotting time-series data
  /////////////////////////////////////////////////////////////////////////////////
  auto graphview_type = //
      py::class_<ui::GraphView, ui::PrimCanvas, ui::graphview_ptr_t>(uimodule, "GraphView")
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
          .def_readwrite("clear_color", &ui::GraphView::_bg_color)
          .def_readwrite("show_stats", &ui::GraphView::_show_stats)
          .def_readwrite("min_band_pixels", &ui::GraphView::_min_band_pixels)
          .def_readwrite("paused", &ui::GraphView::_paused);
  type_codec->registerStdCodec<ui::graphview_ptr_t>(graphview_type);
  /////////////////////////////////////////////////////////////////////////////////
  // ProfilerView
  /////////////////////////////////////////////////////////////////////////////////
  auto profilerview_type = //
      py::class_<ui::ProfilerView, ui::Widget, ui::profilerview_ptr_t>(uimodule, "ProfilerView")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::profilerview_ptr_t {
                return std::make_shared<ui::ProfilerView>();
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t {
                auto layoutitem = lg->makeChild<ui::ProfilerView>();
                return layoutitem.as_shared();
              })
          .def_static(
              "uigridfactory",
              [type_codec](uilayoutgroup_ptr_t lg, ui::gridparams_ptr_t gp, py::list py_args) -> py::list {
                auto layoutitems = lg->makeGridOfWidgets<ui::ProfilerView>(gp);
                py::list rval;
                for (auto item : layoutitems) {
                  rval.append(item.as_shared());
                }
                return rval;
              })
          .def("addChannel", [](ui::ProfilerView* v, const std::string& name) { v->addChannel(name); })
          .def_readwrite("clear_color", &ui::ProfilerView::_bg_color);
  type_codec->registerStdCodec<ui::profilerview_ptr_t>(profilerview_type);
  /////////////////////////////////////////////////////////////////////////////////
  uimodule.def(
      "profiler_add_event",
      [](const std::string& channel_name, const std::string& series_name) {
#ifdef ORK_PROFILER_ENABLE
        // This is doing a fully lookup through the shared_mutex every call.
        // If we ever need a way to call this hundrends of times a frame this needs to change.
        auto* es = Profiler::acquireSeries<EventProfilerSeries>(
            channel_name.c_str(), CrcString(channel_name.c_str()).hashed(),
            series_name.c_str(), CrcString(series_name.c_str()).hashed());
        es->addEvent();
#endif
      },
      py::arg("channel_name"),
      py::arg("series_name"));
  /////////////////////////////////////////////////////////////////////////////////
  uimodule.def(
      "profiler_sample_begin",
      [](const std::string& channel_name, const std::string& series_name) {
#ifdef ORK_PROFILER_ENABLE
        auto* ss = Profiler::acquireSeries<SampleProfilerSeries>(
            channel_name.c_str(), CrcString(channel_name.c_str()).hashed(),
            series_name.c_str(), CrcString(series_name.c_str()).hashed());
        ss->sampleBegin();
#endif
      },
      py::arg("channel_name"),
      py::arg("series_name"));
  /////////////////////////////////////////////////////////////////////////////////
  uimodule.def(
      "profiler_sample_end",
      [](const std::string& channel_name, const std::string& series_name) {
#ifdef ORK_PROFILER_ENABLE
        auto* ss = Profiler::acquireSeries<SampleProfilerSeries>(
            channel_name.c_str(), CrcString(channel_name.c_str()).hashed(),
            series_name.c_str(), CrcString(series_name.c_str()).hashed());
        ss->sampleEnd();
#endif
      },
      py::arg("channel_name"),
      py::arg("series_name"));
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
          .def_property(
              "supersample",
              [](uisgviewport_ptr_t sgview) -> int { return sgview->_supersample; },
              [](uisgviewport_ptr_t sgview, int ss) { sgview->_supersample = ss; })
          //////////////////////////////////
          .def_property(
              "temporal_frames",
              [](uisgviewport_ptr_t sgview) -> int { return sgview->_temporalFrames; },
              [](uisgviewport_ptr_t sgview, int tf) { sgview->_temporalFrames = tf; })
          //////////////////////////////////
          // Bind ManipController directly (C++ event routing)
          .def(
              "bindManipController",
              [](uisgviewport_ptr_t sgview, lev2::editor::manipcontroller_ptr_t mc) { //
                sgview->bindManipController(mc);
              })
          //////////////////////////////////
          // Manipulation event handler - for ManipController
          .def_property(
              "manip_evhandler",
              [](uisgviewport_ptr_t sgview) -> py::object { //
                return py::none();
              },
              [](uisgviewport_ptr_t sgview, py::object callback) { //
                sgview->_uservars->makeValueForKey<py::object>("_hold_manip_ev_callback", callback);
                sgview->_manip_evhandler = [sgview](ui::event_constptr_t ev) -> ui::HandlerResult {
                  ui::HandlerResult rval;
                  py::gil_scoped_acquire acquire_gil;
                  auto cb = sgview->_uservars->typedValueForKey<py::object>("_hold_manip_ev_callback").value();
                  if (cb) {
                    auto pyrval = cb(ev);
                    if (pyrval) {
                      rval = py::cast<ui::HandlerResult>(pyrval);
                    }
                  }
                  return rval;
                };
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
              })
          //////////////////////////////////
          .def_property(
              "onPreRender",
              [](uisgviewport_ptr_t sgview) -> py::object { //
                return py::none();
              },
              [](uisgviewport_ptr_t sgview, py::object callback) { //
                if (callback.is_none()) {
                  sgview->_preRenderCallback = nullptr;
                } else {
                  auto pycb = ork::python::gil_safe_pyobj(callback);
                  sgview->_preRenderCallback = [pycb](lev2::Context* ctx) {
                    py::gil_scoped_acquire acquire_gil;
                    auto fn = pycb.valueAs<py::function>();
                    if (fn) {
                      auto pyctx = python::unmanaged_ptr<lev2::Context>(ctx);
                      (*fn)(pyctx);
                    }
                  };
                }
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
              })
          .def_property(
              "sort_tabs",
              [](ui::tabwidget_ptr_t tabs) -> bool { //
                return tabs->_sort_tabs;
              },
              [](ui::tabwidget_ptr_t tabs, bool b) { //
                tabs->setSortTabs(b);
              })
          .def(
              "setTabCloseable",
              [](ui::tabwidget_ptr_t tabs, ui::widget_ptr_t tab, bool closeable) {
                tabs->setTabCloseable(tab, closeable);
              })
          .def(
              "isTabCloseable",
              [](ui::tabwidget_ptr_t tabs, ui::widget_ptr_t tab) -> bool {
                return tabs->isTabCloseable(tab);
              })
          .def(
              "onTabClose",
              [](ui::tabwidget_ptr_t tabs, py::object callback) {
                auto safe = python::gil_safe_pyobj(callback);
                tabs->_onTabClose = [safe](ui::widget_ptr_t tab) {
                  py::gil_scoped_acquire acquire;
                  auto fn = safe.valueAs<py::object>();
                  (*fn)(tab);
                };
              })
          .def(
              "removeTab",
              [](ui::tabwidget_ptr_t tabs, ui::widget_ptr_t tab) {
                tabs->_closeable_tabs.erase(tab);
                tabs->_per_tab_style_tags.erase(tab);
                if (tabs->getActiveTab() >= 0 && tabs->_children[tabs->getActiveTab()] == tab) {
                  tabs->setActiveTab(0);
                }
                tabs->removeChild(tab);
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
              "fill_widget",
              [](ui::vpack_ptr_t vpack) -> ui::widget_ptr_t { //
                return vpack->_fill_widget;
              },
              [](ui::vpack_ptr_t vpack, ui::widget_ptr_t w) { //
                vpack->_fill_widget = w;
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
              [](ui::vpack_ptr_t vpack, bool val) { vpack->_draw_background = val; })
          .def_property(
              "propagate_on_parent_change",
              [](ui::vpack_ptr_t vpack) -> bool { return vpack->_propagate_on_parent_change; },
              [](ui::vpack_ptr_t vpack, bool val) { vpack->_propagate_on_parent_change = val; });
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
              "fill_widget",
              [](ui::hpack_ptr_t hpack) -> ui::widget_ptr_t { //
                return hpack->_fill_widget;
              },
              [](ui::hpack_ptr_t hpack, ui::widget_ptr_t w) { //
                hpack->_fill_widget = w;
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
              [](ui::hpack_ptr_t hpack, bool val) { hpack->_draw_background = val; })
          .def(
              "addChild",
              [](ui::hpack_ptr_t hpack, ui::widget_ptr_t child, bool relayout) { //
                hpack->addChild(child, relayout);
              },
              py::arg("child"),
              py::arg("relayout") = true)
          .def(
              "removeChild",
              [](ui::hpack_ptr_t hpack, ui::widget_ptr_t child, bool relayout) { //
                hpack->removeChild(child, relayout);
              },
              py::arg("child"),
              py::arg("relayout") = true);
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
              })
          .def_property(
              "input_color",
              [](ui::lineedit_ptr_t le) -> fvec4 { //
                return le->_input_color;
              },
              [](ui::lineedit_ptr_t le, fvec4 c) { //
                le->_input_color = c;
                le->_input_color_set = true;
              })
          .def(
              "onTextChanged",
              [](ui::lineedit_ptr_t le, py::object callback) { //
                if (callback.is_none()) {
                  le->_onTextChanged = nullptr;
                } else {
                  le->_onTextChanged = [callback](const std::string& text) {
                    py::gil_scoped_acquire acquire;
                    callback(text);
                  };
                }
              })
          .def(
              "onTextCommitted",
              [](ui::lineedit_ptr_t le, py::object callback) { //
                if (callback.is_none()) {
                  le->_onTextCommitted = nullptr;
                } else {
                  auto safe = python::gil_safe_pyobj(callback);
                  le->_onTextCommitted = [safe](const std::string& text) {
                    py::gil_scoped_acquire acquire;
                    auto fn = safe.valueAs<py::object>();
                    (*fn)(text);
                  };
                }
              });
  type_codec->registerStdCodec<ui::lineedit_ptr_t>(lineedit_type);
  /////////////////////////////////////////////////////////////////////////////////
  // F32Edit
  auto f32edit_type = //
      py::class_<ui::F32Edit, ui::Widget, ui::f32edit_ptr_t>(uimodule, "F32Edit")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::f32edit_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto label        = decoded_args[1].get<std::string>();
                float value       = decoded_args.size() > 2 ? decoded_args[2].get<float>() : 0.0f;
                float minval      = decoded_args.size() > 3 ? decoded_args[3].get<float>() : -1e30f;
                float maxval      = decoded_args.size() > 4 ? decoded_args[4].get<float>() : 1e30f;
                auto edit         = std::make_shared<ui::F32Edit>(name, label, value, minval, maxval);
                return edit;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto label        = decoded_args[1].get<std::string>();
                float value       = decoded_args.size() > 2 ? decoded_args[2].get<float>() : 0.0f;
                float minval      = decoded_args.size() > 3 ? decoded_args[3].get<float>() : -1e30f;
                float maxval      = decoded_args.size() > 4 ? decoded_args[4].get<float>() : 1e30f;
                auto layoutitem   = lg->makeChild<ui::F32Edit>(name, label, value, minval, maxval);
                return layoutitem.as_shared();
              })
          .def_property(
              "value",
              [](ui::f32edit_ptr_t edit) -> float { //
                return edit->getValue();
              },
              [](ui::f32edit_ptr_t edit, float val) { //
                edit->setValue(val);
              })
          .def_property(
              "min",
              [](ui::f32edit_ptr_t edit) -> float { //
                return edit->getMin();
              },
              [](ui::f32edit_ptr_t edit, float val) { //
                edit->setMin(val);
              })
          .def_property(
              "max",
              [](ui::f32edit_ptr_t edit) -> float { //
                return edit->getMax();
              },
              [](ui::f32edit_ptr_t edit, float val) { //
                edit->setMax(val);
              })
          .def_property(
              "label",
              [](ui::f32edit_ptr_t edit) -> std::string { //
                return edit->getLabel();
              },
              [](ui::f32edit_ptr_t edit, std::string l) { //
                edit->setLabel(l);
              })
          .def_property(
              "precision",
              [](ui::f32edit_ptr_t edit) -> int { //
                return edit->_precision;
              },
              [](ui::f32edit_ptr_t edit, int p) { //
                edit->_precision = p;
              })
          .def_property(
              "fg_color",
              [](ui::f32edit_ptr_t edit) -> fvec3 { //
                return edit->_fg_color;
              },
              [](ui::f32edit_ptr_t edit, fvec3 c) { //
                edit->_fg_color = c;
              })
          .def_property(
              "bg_color",
              [](ui::f32edit_ptr_t edit) -> fvec3 { //
                return edit->_bg_color;
              },
              [](ui::f32edit_ptr_t edit, fvec3 c) { //
                edit->_bg_color = c;
              })
          .def_property(
              "input_color",
              [](ui::f32edit_ptr_t edit) -> fvec4 { //
                return edit->_input_color;
              },
              [](ui::f32edit_ptr_t edit, fvec4 c) { //
                edit->_input_color = c;
                edit->_input_color_set = true;
              })
          .def(
              "onValueChanged",
              [](ui::f32edit_ptr_t edit, py::object callback) { //
                if (callback.is_none()) {
                  edit->_onValueChanged = nullptr;
                } else {
                  edit->_onValueChanged = [callback](float val) {
                    py::gil_scoped_acquire acquire;
                    callback(val);
                  };
                }
              })
          .def(
              "onValueCommitted",
              [](ui::f32edit_ptr_t edit, py::object callback) { //
                if (callback.is_none()) {
                  edit->_onValueCommitted = nullptr;
                } else {
                  edit->_onValueCommitted = [callback](float val) {
                    py::gil_scoped_acquire acquire;
                    callback(val);
                  };
                }
              })
          .def_property(
              "drag_rate",
              [](ui::f32edit_ptr_t edit) -> float { //
                return edit->_drag_rate;
              },
              [](ui::f32edit_ptr_t edit, float rate) { //
                edit->_drag_rate = rate;
              })
          .def_property(
              "drag_rate_scalar",
              [](ui::f32edit_ptr_t edit) -> float { //
                return edit->_drag_rate_scalar;
              },
              [](ui::f32edit_ptr_t edit, float scalar) { //
                edit->_drag_rate_scalar = scalar;
              })
          .def_property_readonly(
              "is_dragging",
              [](ui::f32edit_ptr_t edit) -> bool { //
                return edit->_dragging;
              });
  type_codec->registerStdCodec<ui::f32edit_ptr_t>(f32edit_type);
  /////////////////////////////////////////////////////////////////////////////////
  // IntEdit
  auto intedit_type = //
      py::class_<ui::IntEdit, ui::Widget, ui::intedit_ptr_t>(uimodule, "IntEdit")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::intedit_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto label        = decoded_args[1].get<std::string>();
                int value         = decoded_args.size() > 2 ? decoded_args[2].get<int>() : 0;
                int minval        = decoded_args.size() > 3 ? decoded_args[3].get<int>() : INT_MIN;
                int maxval        = decoded_args.size() > 4 ? decoded_args[4].get<int>() : INT_MAX;
                auto edit         = std::make_shared<ui::IntEdit>(name, label, value, minval, maxval);
                return edit;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto label        = decoded_args[1].get<std::string>();
                int value         = decoded_args.size() > 2 ? decoded_args[2].get<int>() : 0;
                int minval        = decoded_args.size() > 3 ? decoded_args[3].get<int>() : INT_MIN;
                int maxval        = decoded_args.size() > 4 ? decoded_args[4].get<int>() : INT_MAX;
                auto layoutitem   = lg->makeChild<ui::IntEdit>(name, label, value, minval, maxval);
                return layoutitem.as_shared();
              })
          .def_property(
              "value",
              [](ui::intedit_ptr_t edit) -> int { //
                return edit->getValue();
              },
              [](ui::intedit_ptr_t edit, int val) { //
                edit->setValue(val);
              })
          .def_property(
              "min",
              [](ui::intedit_ptr_t edit) -> int { //
                return edit->getMin();
              },
              [](ui::intedit_ptr_t edit, int val) { //
                edit->setMin(val);
              })
          .def_property(
              "max",
              [](ui::intedit_ptr_t edit) -> int { //
                return edit->getMax();
              },
              [](ui::intedit_ptr_t edit, int val) { //
                edit->setMax(val);
              })
          .def_property(
              "label",
              [](ui::intedit_ptr_t edit) -> std::string { //
                return edit->getLabel();
              },
              [](ui::intedit_ptr_t edit, std::string l) { //
                edit->setLabel(l);
              })
          .def_property(
              "fg_color",
              [](ui::intedit_ptr_t edit) -> fvec3 { //
                return edit->_fg_color;
              },
              [](ui::intedit_ptr_t edit, fvec3 c) { //
                edit->_fg_color = c;
              })
          .def_property(
              "bg_color",
              [](ui::intedit_ptr_t edit) -> fvec3 { //
                return edit->_bg_color;
              },
              [](ui::intedit_ptr_t edit, fvec3 c) { //
                edit->_bg_color = c;
              })
          .def_property(
              "input_color",
              [](ui::intedit_ptr_t edit) -> fvec4 { //
                return edit->_input_color;
              },
              [](ui::intedit_ptr_t edit, fvec4 c) { //
                edit->_input_color = c;
                edit->_input_color_set = true;
              })
          .def(
              "onValueChanged",
              [](ui::intedit_ptr_t edit, py::object callback) { //
                if (callback.is_none()) {
                  edit->_onValueChanged = nullptr;
                } else {
                  edit->_onValueChanged = [callback](int val) {
                    py::gil_scoped_acquire acquire;
                    callback(val);
                  };
                }
              })
          .def(
              "onValueCommitted",
              [](ui::intedit_ptr_t edit, py::object callback) { //
                if (callback.is_none()) {
                  edit->_onValueCommitted = nullptr;
                } else {
                  edit->_onValueCommitted = [callback](int val) {
                    py::gil_scoped_acquire acquire;
                    callback(val);
                  };
                }
              });
  type_codec->registerStdCodec<ui::intedit_ptr_t>(intedit_type);
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
              })
          .def_property(
              "hover_color",
              [](ui::button_ptr_t btn) -> fvec3 { //
                return btn->_hover_color;
              },
              [](ui::button_ptr_t btn, fvec3 c) { //
                btn->_hover_color = c;
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
              "hover_color",
              [](ui::imagebutton_ptr_t btn) -> fvec4 { //
                return btn->_hover_color;
              },
              [](ui::imagebutton_ptr_t btn, fvec4 c) { //
                btn->_hover_color = c;
              })
          .def_property(
              "pressed_color",
              [](ui::imagebutton_ptr_t btn) -> fvec4 { //
                return btn->_pressed_color;
              },
              [](ui::imagebutton_ptr_t btn, fvec4 c) { //
                btn->_pressed_color = c;
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
  // ColorSwatch - inline color preview widget
  auto colorswatch_type = //
      py::class_<ui::ColorSwatch, ui::Widget, ui::colorswatch_ptr_t>(uimodule, "ColorSwatch")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::colorswatch_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                fvec4 color       = fvec4(0.5f, 0.5f, 0.5f, 1.0f);
                if (decoded_args.size() > 1) {
                  color = decoded_args[1].get<fvec4>();
                }
                auto swatch = std::make_shared<ui::ColorSwatch>(name, color);
                return swatch;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                fvec4 color       = fvec4(0.5f, 0.5f, 0.5f, 1.0f);
                if (decoded_args.size() > 1) {
                  color = decoded_args[1].get<fvec4>();
                }
                auto layoutitem = lg->makeChild<ui::ColorSwatch>(name, color);
                return layoutitem.as_shared();
              })
          .def_property(
              "color",
              [](ui::colorswatch_ptr_t sw) -> fvec4 { return sw->color(); },
              [](ui::colorswatch_ptr_t sw, fvec4 c) { sw->setColor(c); })
          .def_property(
              "border_color",
              [](ui::colorswatch_ptr_t sw) -> fvec4 { return sw->_border_color; },
              [](ui::colorswatch_ptr_t sw, fvec4 c) { sw->_border_color = c; })
          .def_property(
              "hover_border_color",
              [](ui::colorswatch_ptr_t sw) -> fvec4 { return sw->_hover_border_color; },
              [](ui::colorswatch_ptr_t sw, fvec4 c) { sw->_hover_border_color = c; })
          .def_property(
              "border_width",
              [](ui::colorswatch_ptr_t sw) -> int { return sw->_border_width; },
              [](ui::colorswatch_ptr_t sw, int w) { sw->_border_width = w; })
          .def_property(
              "corner_radius",
              [](ui::colorswatch_ptr_t sw) -> int { return sw->_corner_radius; },
              [](ui::colorswatch_ptr_t sw, int r) { sw->_corner_radius = r; })
          .def_property(
              "show_hex",
              [](ui::colorswatch_ptr_t sw) -> bool { return sw->_show_hex; },
              [](ui::colorswatch_ptr_t sw, bool b) { sw->_show_hex = b; })
          .def_property(
              "onClick",
              [](ui::colorswatch_ptr_t sw) -> py::object { return py::none(); },
              [](ui::colorswatch_ptr_t sw, py::object callback) {
                if (not callback.is_none()) {
                  auto pycb     = std::make_shared<py::object>(callback);
                  sw->_onClick = [pycb, sw]() {
                    py::gil_scoped_acquire acquire_gil;
                    (*pycb)(sw);
                  };
                }
              })
          .def_property(
              "onColorChanged",
              [](ui::colorswatch_ptr_t sw) -> py::object { return py::none(); },
              [type_codec](ui::colorswatch_ptr_t sw, py::object callback) {
                if (not callback.is_none()) {
                  auto pycb             = std::make_shared<py::object>(callback);
                  sw->_onColorChanged = [pycb, type_codec](fvec4 newcolor) {
                    py::gil_scoped_acquire acquire_gil;
                    auto encoded = type_codec->encode(newcolor);
                    (*pycb)(encoded);
                  };
                }
              });
  type_codec->registerStdCodec<ui::colorswatch_ptr_t>(colorswatch_type);
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
              "crosshair_enabled",
              [](ui::imgview_ptr_t imgview) -> bool { return imgview->_crosshair_enabled; },
              [](ui::imgview_ptr_t imgview, bool p) { imgview->_crosshair_enabled = p; })
          .def_property(
              "crosshair_pos",
              [](ui::imgview_ptr_t imgview) -> fvec2 { return imgview->_crosshair_pos; },
              [](ui::imgview_ptr_t imgview, fvec2 p) { imgview->_crosshair_pos = p; })
          .def_property(
              "crosshair_color",
              [](ui::imgview_ptr_t imgview) -> fvec4 { return imgview->_crosshair_color; },
              [](ui::imgview_ptr_t imgview, fvec4 c) { imgview->_crosshair_color = c; })
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
  // PrimCanvas - GPU-accelerated canvas with SSBO-based primitives
  /////////////////////////////////////////////////////////////////////////////////

  // QuadData - per-quad instance data
  auto quaddata_type = //
      py::class_<ui::QuadData, ui::quaddata_ptr_t>(uimodule, "QuadData")
          .def(py::init<>([]() { return std::make_shared<ui::QuadData>(); }))
          .def_readwrite("pos_size", &ui::QuadData::pos_size)
          .def_readwrite("uv_rect", &ui::QuadData::uv_rect)
          .def_readwrite("color", &ui::QuadData::color)
          .def_readwrite("extra", &ui::QuadData::extra)
          .def(
              "setPosition",
              [](ui::quaddata_ptr_t qd, float x, float y) {
                qd->pos_size.x = x;
                qd->pos_size.y = y;
              })
          .def(
              "setSize",
              [](ui::quaddata_ptr_t qd, float w, float h) {
                qd->pos_size.z = w;
                qd->pos_size.w = h;
              })
          .def(
              "setColor",
              [](ui::quaddata_ptr_t qd, fvec4 c) { qd->color = c; })
          .def(
              "setRotation",
              [](ui::quaddata_ptr_t qd, float radians) { qd->extra.x = radians; })
          .def(
              "setCornerRadius",
              [](ui::quaddata_ptr_t qd, float radius) { qd->extra.y = radius; })
          .def(
              "setUV",
              [](ui::quaddata_ptr_t qd, float u0, float v0, float u1, float v1) {
                qd->uv_rect = fvec4(u0, v0, u1, v1);
              });
  type_codec->registerStdCodec<ui::quaddata_ptr_t>(quaddata_type);

  // Primitive - base class for all canvas primitives
  auto primitive_type = //
      py::class_<ui::Primitive, ui::primitive_ptr_t>(uimodule, "Primitive");
  type_codec->registerStdCodec<ui::primitive_ptr_t>(primitive_type);

  // QuadPrimitive - batch of quads sharing pipeline/texture
  auto quadprimitive_type = //
      py::class_<ui::QuadPrimitive, ui::Primitive, ui::quadprimitive_ptr_t>(uimodule, "QuadPrimitive")
          .def(py::init<>([](fxpipeline_ptr_t pipeline, texture_ptr_t texture) {
            return std::make_shared<ui::QuadPrimitive>(pipeline, texture);
          }), py::arg("pipeline"), py::arg("texture") = nullptr)
          .def_readonly("pipeline", &ui::QuadPrimitive::_pipeline)
          .def_readonly("texture", &ui::QuadPrimitive::_texture)
          .def_property_readonly("quadCount", [](ui::quadprimitive_ptr_t prim) { return prim->_quads.size(); })
          .def(
              "addQuad",
              [](ui::quadprimitive_ptr_t prim, ui::quaddata_ptr_t qd) {
                prim->_quads.push_back(qd);
              })
          .def(
              "quad",
              [](ui::quadprimitive_ptr_t prim, size_t index) -> ui::quaddata_ptr_t {
                return prim->_quads[index];
              })
          .def(
              "clearQuads",
              [](ui::quadprimitive_ptr_t prim) {
                prim->_quads.clear();
              });
  type_codec->registerStdCodec<ui::quadprimitive_ptr_t>(quadprimitive_type);

  // VertexData - per-vertex data for triangle primitives
  auto vertexdata_type = //
      py::class_<ui::VertexData, ui::vertexdata_ptr_t>(uimodule, "VertexData")
          .def(py::init<>([]() { return std::make_shared<ui::VertexData>(); }))
          .def_readwrite("position", &ui::VertexData::position)
          .def_readwrite("uv", &ui::VertexData::uv)
          .def_readwrite("color", &ui::VertexData::color)
          .def_readwrite("extra", &ui::VertexData::extra)
          .def(
              "setPosition",
              [](ui::vertexdata_ptr_t vd, float x, float y) {
                vd->position.x = x;
                vd->position.y = y;
              })
          .def(
              "setUV",
              [](ui::vertexdata_ptr_t vd, float u, float v) {
                vd->uv.x = u;
                vd->uv.y = v;
              })
          .def(
              "setColor",
              [](ui::vertexdata_ptr_t vd, fvec4 c) { vd->color = c; });
  type_codec->registerStdCodec<ui::vertexdata_ptr_t>(vertexdata_type);

  // TriStripPrimitive - triangle strip
  auto tristripprimitive_type = //
      py::class_<ui::TriStripPrimitive, ui::Primitive, ui::tristripprimitive_ptr_t>(uimodule, "TriStripPrimitive")
          .def(py::init<>([](fxpipeline_ptr_t pipeline, texture_ptr_t texture) {
            return std::make_shared<ui::TriStripPrimitive>(pipeline, texture);
          }), py::arg("pipeline"), py::arg("texture") = nullptr)
          .def_readonly("pipeline", &ui::TriStripPrimitive::_pipeline)
          .def_readonly("texture", &ui::TriStripPrimitive::_texture)
          .def_property_readonly("vertexCount", [](ui::tristripprimitive_ptr_t prim) { return prim->_vertices.size(); })
          .def(
              "addVertex",
              [](ui::tristripprimitive_ptr_t prim, ui::vertexdata_ptr_t vd) {
                prim->_vertices.push_back(vd);
              })
          .def(
              "vertex",
              [](ui::tristripprimitive_ptr_t prim, size_t index) -> ui::vertexdata_ptr_t {
                return prim->_vertices[index];
              })
          .def(
              "clearVertices",
              [](ui::tristripprimitive_ptr_t prim) {
                prim->_vertices.clear();
              });
  type_codec->registerStdCodec<ui::tristripprimitive_ptr_t>(tristripprimitive_type);

  // TriListPrimitive - triangle list
  auto trilistprimitive_type = //
      py::class_<ui::TriListPrimitive, ui::Primitive, ui::trilistprimitive_ptr_t>(uimodule, "TriListPrimitive")
          .def(py::init<>([](fxpipeline_ptr_t pipeline, texture_ptr_t texture) {
            return std::make_shared<ui::TriListPrimitive>(pipeline, texture);
          }), py::arg("pipeline"), py::arg("texture") = nullptr)
          .def_readonly("pipeline", &ui::TriListPrimitive::_pipeline)
          .def_readonly("texture", &ui::TriListPrimitive::_texture)
          .def_property_readonly("vertexCount", [](ui::trilistprimitive_ptr_t prim) { return prim->_vertices.size(); })
          .def(
              "addVertex",
              [](ui::trilistprimitive_ptr_t prim, ui::vertexdata_ptr_t vd) {
                prim->_vertices.push_back(vd);
              })
          .def(
              "vertex",
              [](ui::trilistprimitive_ptr_t prim, size_t index) -> ui::vertexdata_ptr_t {
                return prim->_vertices[index];
              })
          .def(
              "clearVertices",
              [](ui::trilistprimitive_ptr_t prim) {
                prim->_vertices.clear();
              })
          .def(
              "debugAddress",
              [](ui::trilistprimitive_ptr_t prim) {
                return reinterpret_cast<uintptr_t>(prim.get());
              });
  type_codec->registerStdCodec<ui::trilistprimitive_ptr_t>(trilistprimitive_type);

  // SpritePrimitive - static sprite template with quads in local space
  auto spriteprimitive_type = //
      py::class_<ui::SpritePrimitive, ui::Primitive, ui::spriteprimitive_ptr_t>(uimodule, "SpritePrimitive")
          .def(py::init<>([](fxpipeline_ptr_t pipeline, texture_ptr_t texture) {
            return std::make_shared<ui::SpritePrimitive>(pipeline, texture);
          }), py::arg("pipeline"), py::arg("texture") = nullptr)
          .def_readonly("pipeline", &ui::SpritePrimitive::_pipeline)
          .def_readonly("texture", &ui::SpritePrimitive::_texture)
          .def_property_readonly("quadCount", [](ui::spriteprimitive_ptr_t prim) { return prim->_quads.size(); })
          .def(
              "addQuad",
              [](ui::spriteprimitive_ptr_t prim, ui::quaddata_ptr_t qd) {
                prim->_quads.push_back(qd);
              })
          .def(
              "quad",
              [](ui::spriteprimitive_ptr_t prim, size_t index) -> ui::quaddata_ptr_t {
                return prim->_quads[index];
              })
          .def(
              "clearQuads",
              [](ui::spriteprimitive_ptr_t prim) {
                prim->_quads.clear();
              });
  type_codec->registerStdCodec<ui::spriteprimitive_ptr_t>(spriteprimitive_type);

  // SpriteInstance - lightweight instance referencing a SpritePrimitive
  auto spriteinstance_type = //
      py::class_<ui::SpriteInstance, ui::Primitive, ui::spriteinstance_ptr_t>(uimodule, "SpriteInstance")
          .def(py::init<>([](ui::spriteprimitive_ptr_t sprite) {
            return std::make_shared<ui::SpriteInstance>(sprite);
          }), py::arg("sprite") = nullptr)
          .def_readwrite("sprite", &ui::SpriteInstance::_sprite)
          .def_readwrite("transform", &ui::SpriteInstance::_transform)
          .def_readwrite("tint", &ui::SpriteInstance::_tint)
          .def_readwrite("visible", &ui::SpriteInstance::_visible)
          .def("setPosition", &ui::SpriteInstance::setPosition)
          .def("setRotation", &ui::SpriteInstance::setRotation)
          .def("setScale", py::overload_cast<float, float>(&ui::SpriteInstance::setScale))
          .def("setUniformScale", py::overload_cast<float>(&ui::SpriteInstance::setScale))
          .def("setTransform", &ui::SpriteInstance::setTransform);
  type_codec->registerStdCodec<ui::spriteinstance_ptr_t>(spriteinstance_type);

  // TextItem - single text entry within a TextPrimitive
  auto textitem_type = //
      py::class_<ui::TextItem>(uimodule, "TextItem")
          .def(py::init<>())
          .def_readwrite("text", &ui::TextItem::text)
          .def_readwrite("position", &ui::TextItem::position);

  // TextPrimitive - collection of text sharing font/color
  auto textprimitive_type = //
      py::class_<ui::TextPrimitive, ui::Primitive, ui::textprimitive_ptr_t>(uimodule, "TextPrimitive")
          .def(py::init<>([](font_ptr_t font, fvec4 color) {
            return std::make_shared<ui::TextPrimitive>(font, color);
          }), py::arg("font"), py::arg("color") = fvec4(1, 1, 1, 1))
          .def_readonly("font", &ui::TextPrimitive::_font)
          .def_readonly("color", &ui::TextPrimitive::_color)
          .def_readwrite("clickable", &ui::TextPrimitive::_clickable)
          .def_property_readonly("itemCount", [](ui::textprimitive_ptr_t prim) { return prim->_items.size(); })
          .def("item", [](ui::textprimitive_ptr_t prim, size_t idx) -> ui::TextItem& {
            return prim->_items.at(idx);
          }, py::return_value_policy::reference_internal)
          .def(
              "addItem",
              [](ui::textprimitive_ptr_t prim, std::string text, fvec2 position) {
                ui::TextItem item;
                item.text = text;
                item.position = position;
                prim->_items.push_back(item);
              })
          .def(
              "clearItems",
              [](ui::textprimitive_ptr_t prim) {
                prim->_items.clear();
              });
  type_codec->registerStdCodec<ui::textprimitive_ptr_t>(textprimitive_type);

  // PrimCanvasLayer - a layer containing primitives
  auto primcanvaslayer_type = //
      py::class_<ui::PrimCanvasLayer, ui::primcanvaslayer_ptr_t>(uimodule, "PrimCanvasLayer")
          .def(py::init<const std::string&>(), py::arg("name") = "layer")
          .def_readwrite("name", &ui::PrimCanvasLayer::_name)
          .def_readwrite("enabled", &ui::PrimCanvasLayer::_enabled)
          .def("clear", &ui::PrimCanvasLayer::clear)
          .def("addPrimitive", &ui::PrimCanvasLayer::addPrimitive)
          .def("removePrimitive", &ui::PrimCanvasLayer::removePrimitive)
          .def("primitive", &ui::PrimCanvasLayer::primitive)
          .def("primitiveCount", &ui::PrimCanvasLayer::primitiveCount)
          .def_property("transform",
              &ui::PrimCanvasLayer::transform,
              &ui::PrimCanvasLayer::setTransform);
  type_codec->registerStdCodec<ui::primcanvaslayer_ptr_t>(primcanvaslayer_type);

  // PrimCanvas - methods (type registered earlier for GraphView base class)
  primcanvas_type
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::prim_canvas_ptr_t {
                auto decoded_args = type_codec->decodeList(py_args);
                auto name = decoded_args[0].get<std::string>();
                auto canvas = std::make_shared<ui::PrimCanvas>(name);
                return canvas;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t {
                auto decoded_args = type_codec->decodeList(py_args);
                auto name = decoded_args[0].get<std::string>();
                auto layoutitem = lg->makeChild<ui::PrimCanvas>(name);
                return layoutitem.as_shared();
              })
          // Layer management
          .def("createLayer", &ui::PrimCanvas::createLayer, py::arg("name") = "layer")
          .def("addLayer", &ui::PrimCanvas::addLayer)
          .def("removeLayer", &ui::PrimCanvas::removeLayer)
          .def("clearLayers", &ui::PrimCanvas::clearLayers)
          .def("layer", &ui::PrimCanvas::layer)
          .def("layerByName", &ui::PrimCanvas::layerByName)
          .def("layerCount", &ui::PrimCanvas::layerCount)
          .def("markDirty", &ui::PrimCanvas::markDirty)
          // Metrics
          .def_property_readonly("total_primitive_count", &ui::PrimCanvas::totalPrimitiveCount)
          .def_property_readonly("total_quad_count", &ui::PrimCanvas::totalQuadCount)
          .def_property_readonly("ssbo_cpu_size", &ui::PrimCanvas::ssboCpuSize)
          .def_property_readonly("ssbo_gpu_capacity", &ui::PrimCanvas::ssboGpuCapacity)
          .def_property_readonly("ssbo_rebuild_count", &ui::PrimCanvas::ssboRebuildCount)
          .def("gpuInit", [](ui::prim_canvas_ptr_t canvas, ctx_t ctx) {
            canvas->gpuInit(ctx.get());
          })
          .def_property_readonly("pipelineSolid", &ui::PrimCanvas::pipelineSolid)
          .def_property_readonly("pipelineTextured", &ui::PrimCanvas::pipelineTextured)
          .def_property_readonly("pipelineVtxSolid", &ui::PrimCanvas::pipelineVtxSolid)
          .def_property_readonly("pipelineVtxTextured", &ui::PrimCanvas::pipelineVtxTextured)
          .def_property_readonly("pipelineSpriteSolid", &ui::PrimCanvas::pipelineSpriteSolid)
          .def_property_readonly("pipelineSpriteTextured", &ui::PrimCanvas::pipelineSpriteTextured)
          .def_property(
              "bg_color",
              [](ui::prim_canvas_ptr_t canvas) -> fvec4 { return canvas->_bg_color; },
              [](ui::prim_canvas_ptr_t canvas, fvec4 c) { canvas->_bg_color = c; })
          .def_property(
              "draw_background",
              [](ui::prim_canvas_ptr_t canvas) -> bool { return canvas->_draw_background; },
              [](ui::prim_canvas_ptr_t canvas, bool b) { canvas->_draw_background = b; })
          .def("exportSvg", [](ui::prim_canvas_ptr_t canvas, const std::string& path) {
            canvas->exportSvg(path);
          })
          .def_property(
              "supersample",
              [](ui::prim_canvas_ptr_t canvas) -> int { return canvas->_supersample; },
              [](ui::prim_canvas_ptr_t canvas, int ss) { canvas->_supersample = std::clamp(ss, 0, 5); })
          .def_property(
              "desiredWidth",
              [](ui::prim_canvas_ptr_t canvas) -> int { return canvas->_desired_width; },
              [](ui::prim_canvas_ptr_t canvas, int w) { canvas->_desired_width = w; })
          .def_property(
              "desiredHeight",
              [](ui::prim_canvas_ptr_t canvas) -> int { return canvas->_desired_height; },
              [](ui::prim_canvas_ptr_t canvas, int h) { canvas->_desired_height = h; })
          .def_property(
              "onUiEvent",
              [](ui::prim_canvas_ptr_t canvas) -> py::object { return py::none(); },
              [](ui::prim_canvas_ptr_t canvas, py::object callback) {
                if (not callback.is_none()) {
                  auto pycb = std::make_shared<py::object>(callback);
                  canvas->_onUiEvent = [pycb](ui::event_constptr_t ev) -> ui::HandlerResult {
                    py::gil_scoped_acquire acquire_gil;
                    py::object result = (*pycb)(ev);
                    if (py::isinstance<ui::HandlerResult>(result)) {
                      return result.cast<ui::HandlerResult>();
                    }
                    return ui::HandlerResult();
                  };
                }
              })
          .def_property(
              "onPreRender",
              [](ui::prim_canvas_ptr_t canvas) -> py::object { return py::none(); },
              [](ui::prim_canvas_ptr_t canvas, py::object callback) {
                if (not callback.is_none()) {
                  auto pycb = std::make_shared<py::object>(callback);
                  canvas->_onPreRender = [pycb]() {
                    py::gil_scoped_acquire acquire_gil;
                    (*pycb)();
                  };
                } else {
                  canvas->_onPreRender = nullptr;
                }
              });
  /////////////////////////////////////////////////////////////////////////////////
  // DockablePanel - container with titlebar showing child's name
  auto dockablepanel_type = //
      py::class_<ui::DockablePanel, ui::Group, ui::dockablepanel_ptr_t>(uimodule, "DockablePanel")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::dockablepanel_ptr_t {
                auto decoded_args = type_codec->decodeList(py_args);
                auto name = decoded_args[0].get<std::string>();
                auto panel = std::make_shared<ui::DockablePanel>(name);
                return panel;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t {
                auto decoded_args = type_codec->decodeList(py_args);
                auto name = decoded_args[0].get<std::string>();
                auto layoutitem = lg->makeChild<ui::DockablePanel>(name);
                return layoutitem.as_shared();
              })
          .def_property(
              "child",
              [](ui::dockablepanel_ptr_t panel) -> ui::widget_ptr_t {
                return panel->child();
              },
              [](ui::dockablepanel_ptr_t panel, ui::widget_ptr_t child) {
                panel->setChild(child);
              })
          .def_property(
              "titlebar_height",
              [](ui::dockablepanel_ptr_t panel) -> int { return panel->_titlebar_height; },
              [](ui::dockablepanel_ptr_t panel, int h) { panel->_titlebar_height = h; })
          .def_property(
              "titlebar_color",
              [](ui::dockablepanel_ptr_t panel) -> fvec4 { return panel->_titlebar_color; },
              [](ui::dockablepanel_ptr_t panel, fvec4 c) { panel->_titlebar_color = c; })
          .def_property(
              "title_color",
              [](ui::dockablepanel_ptr_t panel) -> fvec4 { return panel->_title_color; },
              [](ui::dockablepanel_ptr_t panel, fvec4 c) { panel->_title_color = c; })
          .def_property(
              "border_color",
              [](ui::dockablepanel_ptr_t panel) -> fvec4 { return panel->_border_color; },
              [](ui::dockablepanel_ptr_t panel, fvec4 c) { panel->_border_color = c; })
          .def(
              "createChild",
              [](ui::dockablepanel_ptr_t panel, py::kwargs kwargs) -> ui::widget_ptr_t {
                ui::widget_ptr_t rval;
                if (kwargs) {
                  py::list args;
                  py::object wfactory;
                  int args_parsed = 0;
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "uiclass") {
                      auto uiclass_obj = py::cast<py::object>(item.second);
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
                  panel->setChild(rval);
                }
                return rval;
              })
          .def_readwrite("title_override", &ui::DockablePanel::_title_override)
          .def_readwrite("title_center", &ui::DockablePanel::_title_center);
  type_codec->registerStdCodec<ui::dockablepanel_ptr_t>(dockablepanel_type);
  /////////////////////////////////////////////////////////////////////////////////
  // BorderFrame - container that draws a solid border around a single child
  auto borderframe_type = //
      py::class_<ui::BorderFrame, ui::Group, ui::borderframe_ptr_t>(uimodule, "BorderFrame")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::borderframe_ptr_t {
                auto decoded_args = type_codec->decodeList(py_args);
                auto name = decoded_args[0].get<std::string>();
                auto frame = std::make_shared<ui::BorderFrame>(name);
                return frame;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t {
                auto decoded_args = type_codec->decodeList(py_args);
                auto name = decoded_args[0].get<std::string>();
                auto layoutitem = lg->makeChild<ui::BorderFrame>(name);
                return layoutitem.as_shared();
              })
          .def_property(
              "child",
              [](ui::borderframe_ptr_t frame) -> ui::widget_ptr_t {
                return frame->child();
              },
              [](ui::borderframe_ptr_t frame, ui::widget_ptr_t child) {
                frame->setChild(child);
              })
          .def_property(
              "border_width",
              [](ui::borderframe_ptr_t frame) -> int { return frame->_border_width; },
              [](ui::borderframe_ptr_t frame, int w) { frame->_border_width = w; })
          .def_property(
              "border_color",
              [](ui::borderframe_ptr_t frame) -> fvec4 { return frame->_border_color; },
              [](ui::borderframe_ptr_t frame, fvec4 c) { frame->_border_color = c; })
          .def_property(
              "border_outer_color",
              [](ui::borderframe_ptr_t frame) -> fvec4 { return frame->_border_outer_color; },
              [](ui::borderframe_ptr_t frame, fvec4 c) { frame->_border_outer_color = c; })
          .def_property(
              "border_inner_color",
              [](ui::borderframe_ptr_t frame) -> fvec4 { return frame->_border_inner_color; },
              [](ui::borderframe_ptr_t frame, fvec4 c) { frame->_border_inner_color = c; })
          .def_property(
              "border_edge_width",
              [](ui::borderframe_ptr_t frame) -> int { return frame->_border_edge_width; },
              [](ui::borderframe_ptr_t frame, int w) { frame->_border_edge_width = w; })
          .def(
              "createChild",
              [](ui::borderframe_ptr_t frame, py::kwargs kwargs) -> ui::widget_ptr_t {
                ui::widget_ptr_t rval;
                if (kwargs) {
                  py::list args;
                  py::object wfactory;
                  int args_parsed = 0;
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "uiclass") {
                      auto uiclass_obj = py::cast<py::object>(item.second);
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
                  frame->setChild(rval);
                }
                return rval;
              });
  type_codec->registerStdCodec<ui::borderframe_ptr_t>(borderframe_type);
  /////////////////////////////////////////////////////////////////////////////////
  // ScrollContainer
  auto scrollcontainer_type = //
      py::class_<ui::ScrollContainer, ui::Group, ui::scroll_container_ptr_t>(uimodule, "ScrollContainer")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::scroll_container_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto container    = std::make_shared<ui::ScrollContainer>(name);
                return container;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::ScrollContainer>(name);
                return layoutitem.as_shared();
              })
          .def(
              "setChild",
              [](ui::scroll_container_ptr_t sc, ui::widget_ptr_t child) { //
                sc->setChild(child);
              })
          .def_property_readonly(
              "child",
              [](ui::scroll_container_ptr_t sc) -> ui::widget_ptr_t { //
                return sc->getChild();
              })
          .def_property(
              "scroll_mode",
              [](ui::scroll_container_ptr_t sc) -> int { //
                return static_cast<int>(sc->scrollMode());
              },
              [](ui::scroll_container_ptr_t sc, int mode) { //
                sc->setScrollMode(static_cast<ui::ScrollMode>(mode));
              })
          .def_property(
              "scroll_offset_x",
              [](ui::scroll_container_ptr_t sc) -> int { return sc->scrollOffsetX(); },
              [](ui::scroll_container_ptr_t sc, int v) { sc->setScrollOffsetX(v); })
          .def_property(
              "scroll_offset_y",
              [](ui::scroll_container_ptr_t sc) -> int { return sc->scrollOffsetY(); },
              [](ui::scroll_container_ptr_t sc, int v) { sc->setScrollOffsetY(v); })
          .def_property(
              "scroll_speed",
              [](ui::scroll_container_ptr_t sc) -> int { return sc->_vscroller._scroll_speed; },
              [](ui::scroll_container_ptr_t sc, int v) { sc->_vscroller._scroll_speed = v; sc->_hscroller._scroll_speed = v; })
          .def_property(
              "bg_color",
              [](ui::scroll_container_ptr_t sc) -> fvec4 { return sc->_bg_color; },
              [](ui::scroll_container_ptr_t sc, fvec4 c) { sc->_bg_color = c; })
          .def_property(
              "draw_background",
              [](ui::scroll_container_ptr_t sc) -> bool { return sc->_draw_background; },
              [](ui::scroll_container_ptr_t sc, bool v) { sc->_draw_background = v; })
          .def_property(
              "draw_scroll_indicator",
              [](ui::scroll_container_ptr_t sc) -> bool { return sc->_draw_scroll_indicator; },
              [](ui::scroll_container_ptr_t sc, bool v) { sc->_draw_scroll_indicator = v; })
          .def_property(
              "scroll_indicator_color",
              [](ui::scroll_container_ptr_t sc) -> fvec4 { return sc->_vscroller._indicator_color; },
              [](ui::scroll_container_ptr_t sc, fvec4 c) { sc->_vscroller._indicator_color = c; sc->_hscroller._indicator_color = c; })
          .def_property(
              "scroll_indicator_width",
              [](ui::scroll_container_ptr_t sc) -> int { return sc->_vscroller._indicator_width; },
              [](ui::scroll_container_ptr_t sc, int v) { sc->_vscroller._indicator_width = v; sc->_hscroller._indicator_width = v; })
          .def_property(
              "scroll_indicator_margin",
              [](ui::scroll_container_ptr_t sc) -> int { return sc->_vscroller._indicator_margin; },
              [](ui::scroll_container_ptr_t sc, int v) { sc->_vscroller._indicator_margin = v; sc->_hscroller._indicator_margin = v; })
          .def_property_readonly(
              "content_width",
              [](ui::scroll_container_ptr_t sc) -> int { return sc->contentWidth(); })
          .def_property_readonly(
              "content_height",
              [](ui::scroll_container_ptr_t sc) -> int { return sc->contentHeight(); })
          .def_property_readonly(
              "max_scroll_x",
              [](ui::scroll_container_ptr_t sc) -> int { return sc->maxScrollX(); })
          .def_property_readonly(
              "max_scroll_y",
              [](ui::scroll_container_ptr_t sc) -> int { return sc->maxScrollY(); })
          .def("scrollToTop", [](ui::scroll_container_ptr_t sc) { sc->scrollToTop(); })
          .def("scrollToBottom", [](ui::scroll_container_ptr_t sc) { sc->scrollToBottom(); })
          .def("scrollToLeft", [](ui::scroll_container_ptr_t sc) { sc->scrollToLeft(); })
          .def("scrollToRight", [](ui::scroll_container_ptr_t sc) { sc->scrollToRight(); });
  type_codec->registerStdCodec<ui::scroll_container_ptr_t>(scrollcontainer_type);
  /////////////////////////////////////////////////////////////////////////////////
  // ScrollMode enum
  py::enum_<ui::ScrollMode>(uimodule, "ScrollMode")
      .value("Y", ui::ScrollMode::Y)
      .value("X", ui::ScrollMode::X)
      .value("XY", ui::ScrollMode::XY);
  /////////////////////////////////////////////////////////////////////////////////
  // Collapsable
  auto collapsable_type = //
      py::class_<ui::Collapsable, ui::Widget, ui::collapsable_ptr_t>(uimodule, "Collapsable")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::collapsable_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto collapsable  = std::make_shared<ui::Collapsable>(name);
                return collapsable;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::Collapsable>(name);
                return layoutitem.as_shared();
              })
          .def(
              "setChild",
              [](ui::collapsable_ptr_t col, ui::widget_ptr_t child) { //
                col->setChild(child);
              })
          .def_property_readonly(
              "child",
              [](ui::collapsable_ptr_t col) -> ui::widget_ptr_t { //
                return col->getChild();
              })
          .def_property(
              "expanded",
              [](ui::collapsable_ptr_t col) -> bool { return col->isExpanded(); },
              [](ui::collapsable_ptr_t col, bool v) { col->setExpanded(v); })
          .def("toggle", [](ui::collapsable_ptr_t col) { col->toggle(); })
          .def_property(
              "header_height",
              [](ui::collapsable_ptr_t col) -> int { return col->_header_height; },
              [](ui::collapsable_ptr_t col, int v) { col->_header_height = v; })
          .def_property(
              "header_bg_color",
              [](ui::collapsable_ptr_t col) -> fvec4 { return col->_header_bg_color; },
              [](ui::collapsable_ptr_t col, fvec4 c) { col->_header_bg_color = c; })
          .def_property(
              "header_fg_color",
              [](ui::collapsable_ptr_t col) -> fvec4 { return col->_header_fg_color; },
              [](ui::collapsable_ptr_t col, fvec4 c) { col->_header_fg_color = c; })
          .def_property(
              "triangle_color",
              [](ui::collapsable_ptr_t col) -> fvec4 { return col->_triangle_color; },
              [](ui::collapsable_ptr_t col, fvec4 c) { col->_triangle_color = c; })
          .def_property(
              "content_bg_color",
              [](ui::collapsable_ptr_t col) -> fvec4 { return col->_content_bg_color; },
              [](ui::collapsable_ptr_t col, fvec4 c) { col->_content_bg_color = c; })
          .def_property(
              "draw_content_background",
              [](ui::collapsable_ptr_t col) -> bool { return col->_draw_content_background; },
              [](ui::collapsable_ptr_t col, bool v) { col->_draw_content_background = v; })
          .def_property(
              "content_margin",
              [](ui::collapsable_ptr_t col) -> int { return col->_content_margin; },
              [](ui::collapsable_ptr_t col, int v) { col->_content_margin = v; })
          .def_property(
              "indent_width",
              [](ui::collapsable_ptr_t col) -> int { return col->_indent_width; },
              [](ui::collapsable_ptr_t col, int v) { col->_indent_width = v; })
          .def_property(
              "onToggle",
              [](ui::collapsable_ptr_t col) -> py::object { //
                return py::none();
              },
              [](ui::collapsable_ptr_t col, py::object callback) { //
                if (callback.is_none()) {
                  col->_onToggle = nullptr;
                } else {
                  auto pycb      = std::make_shared<py::object>(callback);
                  col->_onToggle = [pycb, col](bool expanded) {
                    py::gil_scoped_acquire acquire_gil;
                    (*pycb)(col, expanded);
                  };
                }
              });
  type_codec->registerStdCodec<ui::collapsable_ptr_t>(collapsable_type);
  /////////////////////////////////////////////////////////////////////////////////
  // SlashNode (read-only)
  py::class_<SlashNode, slashnode_ptr_t>(uimodule, "SlashNode")
      .def_property_readonly("name", [](slashnode_ptr_t node) -> std::string { return node->nodeName(); })
      .def_property_readonly("numChildren", [](slashnode_ptr_t node) -> int { return node->numChildren(); })
      .def_property_readonly(
          "children",
          [](slashnode_ptr_t node) -> py::dict {
            py::dict d;
            for (auto& [k, v] : node->children()) {
              d[py::str(k)] = v;
            }
            return d;
          })
      .def_property_readonly("isLeaf", [](slashnode_ptr_t node) -> bool { return node->isLeaf(); })
      .def_property_readonly("path", [](slashnode_ptr_t node) -> std::string { return node->pathAsString(); });
  /////////////////////////////////////////////////////////////////////////////////
  // SlashTree
  py::class_<SlashTree, slashtree_ptr_t>(uimodule, "SlashTree")
      .def(py::init<>())
      .def(
          "addNode",
          [](slashtree_ptr_t tree, std::string path) -> slashnode_ptr_t { //
            return tree->addNode(path.c_str(), nullptr);
          })
      .def_property_readonly("root", [](slashtree_ptr_t tree) -> slashnode_constptr_t { return tree->root(); });
  /////////////////////////////////////////////////////////////////////////////////
  // ChoicelistWidget
  auto choicelist_widget_type = //
      py::class_<ui::ChoicelistWidget, ui::Widget, ui::choicelist_widget_ptr_t>(uimodule, "ChoicelistWidget")
          .def(py::init<const std::string&, const std::string&>(),
               py::arg("name"),
               py::arg("current_value") = "")
          .def_readwrite("current_value", &ui::ChoicelistWidget::_current_value)
          .def_readwrite("bg_color", &ui::ChoicelistWidget::_bg_color)
          .def_readwrite("fg_color", &ui::ChoicelistWidget::_fg_color)
          .def(
              "setChoices",
              [](ui::choicelist_widget_ptr_t w, std::vector<std::string> choices) {
                w->_getChoices = [choices]() { return choices; };
              })
          .def_property(
              "onChoiceSelected",
              [](ui::choicelist_widget_ptr_t w) -> py::object { return py::none(); },
              [](ui::choicelist_widget_ptr_t w, py::object callback) {
                if (callback.is_none()) {
                  w->_onChoiceSelected = nullptr;
                } else {
                  auto pycb = std::make_shared<py::object>(callback);
                  w->_onChoiceSelected = [pycb](const std::string& value) {
                    py::gil_scoped_acquire acquire;
                    (*pycb)(value);
                  };
                }
              });
  type_codec->registerStdCodec<ui::choicelist_widget_ptr_t>(choicelist_widget_type);
  /////////////////////////////////////////////////////////////////////////////////
  // DropdownMenu
  auto dropdown_menu_type = //
      py::class_<ui::DropdownMenu, ui::Widget, ui::dropdown_menu_ptr_t>(uimodule, "DropdownMenu")
          .def(
              py::init<const std::string&, slashnode_constptr_t>(),
              py::arg("name"),
              py::arg("node"))
          .def_property(
              "onSelected",
              [](ui::dropdown_menu_ptr_t menu) -> py::object { //
                return py::none();
              },
              [](ui::dropdown_menu_ptr_t menu, py::object callback) { //
                if (callback.is_none()) {
                  menu->_onSelected = nullptr;
                } else {
                  auto pycb          = std::make_shared<py::object>(callback);
                  menu->_onSelected = [pycb](std::string value) {
                    py::gil_scoped_acquire acquire_gil;
                    (*pycb)(value);
                  };
                }
              })
          .def(
              "computeSize",
              [](ui::dropdown_menu_ptr_t menu) -> fvec2 { //
                return menu->computeSize();
              })
          .def_static(
              "buildTreeFromPaths",
              [](std::vector<std::string> paths) -> slashtree_ptr_t { //
                return ui::DropdownMenu::buildTreeFromPaths(paths);
              })
          .def_readwrite("sort_alphabetically", &ui::DropdownMenu::_sort_alphabetically)
          .def_static(
              "show",
              [](ui::context_ptr_t ctx,
                 std::vector<std::string> paths,
                 int x, int y,
                 py::object on_selected,
                 bool sort_alphabetically) {
                // Build tree from paths
                auto tree = ui::DropdownMenu::buildTreeFromPaths(paths);
                auto root = tree->root();

                // Extract top-level keys in path order
                std::vector<std::string> item_order;
                for (auto& path : paths) {
                  std::string key = path;
                  // Strip leading slashes
                  while (!key.empty() && key[0] == '/') key = key.substr(1);
                  // Take first path component
                  auto pos = key.find('/');
                  if (pos != std::string::npos) key = key.substr(0, pos);
                  if (!key.empty() && std::find(item_order.begin(), item_order.end(), key) == item_order.end()) {
                    item_order.push_back(key);
                  }
                }

                // Create root dropdown menu
                auto menu = std::make_shared<ui::DropdownMenu>("dropdown_root", root);
                menu->_item_order = item_order;
                menu->_sort_alphabetically = sort_alphabetically;
                menu->_buildItems();

                // Set selection callback
                if (!on_selected.is_none()) {
                  auto pycb = ork::python::gil_safe_pyobj(on_selected);
                  menu->_onSelected = [pycb](std::string value) {
                    py::gil_scoped_acquire acquire_gil;
                    auto fn = pycb.valueAs<py::function>();
                    (*fn)(value);
                  };
                }

                // Compute size and push as overlay
                auto sz = menu->computeSize();

                // Subscribe to ticks for highlight animation
                ctx->subscribeToTicks(menu.get(), [menu](ui::updatedata_ptr_t updata) {
                  float abstime = updata->_abstime;
                  menu->_hl_color.x = 0.4f + (0.3f * sinf(abstime * 3.0f));
                  menu->_hl_color.y = 0.4f + (0.3f * sinf(abstime * 3.1f));
                  menu->_hl_color.z = 0.6f + (0.3f * sinf(abstime * 3.2f));
                });

                ctx->pushOverlay(menu, x, y, int(sz.x), int(sz.y));
              },
              py::arg("context"),
              py::arg("paths"),
              py::arg("x"),
              py::arg("y"),
              py::arg("on_selected") = py::none(),
              py::arg("sort_alphabetically") = false);
  type_codec->registerStdCodec<ui::dropdown_menu_ptr_t>(dropdown_menu_type);
  /////////////////////////////////////////////////////////////////////////////////
  // TransformCurveEditor
  auto transformcurveeditor_type = //
      py::class_<ui::TransformCurveEditor, ui::Widget, ui::transformcurveeditor_ptr_t>(uimodule, "TransformCurveEditor")
          .def_static(
              "create",
              [](const std::string& name, math::transformcurve_ptr_t curve) -> ui::transformcurveeditor_ptr_t {
                return std::make_shared<ui::TransformCurveEditor>(name, curve);
              },
              py::arg("name"), py::arg("curve"))
          .def_property(
              "onClose",
              [](ui::transformcurveeditor_ptr_t ed) -> py::object { return py::none(); },
              [](ui::transformcurveeditor_ptr_t ed, py::object callback) {
                if (callback.is_none()) {
                  ed->_onClose = nullptr;
                } else {
                  auto safe = python::gil_safe_pyobj(callback);
                  ed->_onClose = [safe]() {
                    py::gil_scoped_acquire acquire_gil;
                    auto fn = safe.valueAs<py::object>();
                    (*fn)();
                  };
                }
              })
          .def_property(
              "onCurveChanged",
              [](ui::transformcurveeditor_ptr_t ed) -> py::object { return py::none(); },
              [](ui::transformcurveeditor_ptr_t ed, py::object callback) {
                if (callback.is_none()) {
                  ed->_onCurveChanged = nullptr;
                } else {
                  auto safe = python::gil_safe_pyobj(callback);
                  ed->_onCurveChanged = [safe]() {
                    py::gil_scoped_acquire acquire_gil;
                    auto fn = safe.valueAs<py::object>();
                    (*fn)();
                  };
                }
              })
          .def("autoFitRanges", [](ui::transformcurveeditor_ptr_t ed) { ed->autoFitRanges(); })
          .def_property(
              "selectedPointIndex",
              [](ui::transformcurveeditor_ptr_t ed) -> int { return ed->_selectedPointIndex; },
              [](ui::transformcurveeditor_ptr_t ed, int idx) { ed->_selectedPointIndex = idx; });
  type_codec->registerStdCodec<ui::transformcurveeditor_ptr_t>(transformcurveeditor_type);
  /////////////////////////////////////////////////////////////////////////////////
  pyinit_ui_layout(uimodule);
  pyinit_ui_ged(uimodule);
  pyinit_ui_box(uimodule);
  pyinit_ui_style(uimodule);
  pyinit_ui_dynagrid(uimodule);
  pyinit_ui_sdfshape(uimodule);
  pyinit_ui_outliner(uimodule);
  pyinit_ui_property_sheet(uimodule);
  pyinit_ui_filesystem(uimodule);
  pyinit_ui_toolbar(uimodule);
}

} // namespace ork::lev2
