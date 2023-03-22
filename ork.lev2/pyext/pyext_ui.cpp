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
#include <ork/lev2/ui/viewport_scenegraph.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/box.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_ui(py::module& module_lev2) {
  auto uimodule   = module_lev2.def_submodule("ui", "ui operations");
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto uicontext_type = //
      py::class_<ui::Context, ui::context_ptr_t>(module_lev2, "UiContext")
          .def_property_readonly("hasKeyboardFocus", [](ui::context_ptr_t uictx) -> bool { return uictx->hasKeyboardFocus(); })
          .def("hasMouseFocus", [](ui::context_ptr_t uictx, uiwidget_ptr_t w) -> bool { return uictx->hasMouseFocus(w.get()); });
  type_codec->registerStdCodec<ui::context_ptr_t>(uicontext_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto uievent_type = //
      py::class_<ui::Event, ui::event_ptr_t>(module_lev2, "UiEvent")
          .def(
              "clone",                                    //
              [](ui::event_ptr_t ev) -> ui::event_ptr_t { //
                auto cloned_event = std::make_shared<ui::Event>();
                *cloned_event     = *ev;
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
      py::class_<ui::HandlerResult>(uimodule, "UiHandlerResult").def(py::init<>());
  type_codec->registerStdCodec<ui::HandlerResult>(evhandlerrestult_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto widget_type = //
      py::class_<ui::Widget, uiwidget_ptr_t>(uimodule, "UiWidget")
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
          });
  type_codec->registerStdCodec<uiwidget_ptr_t>(widget_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto group_type = //
      py::class_<ui::Group, ui::Widget, uigroup_ptr_t>(uimodule, "UiGroup");
  type_codec->registerStdCodec<uigroup_ptr_t>(group_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto layoutgroup_type = //
      py::class_<ui::LayoutGroup, ui::Group, uilayoutgroup_ptr_t>(uimodule, "UiLayoutGroup")
          .def_property_readonly(
              "layout",
              [](uilayoutgroup_ptr_t lgrp) -> uilayout_ptr_t { //
                return lgrp->_layout;
              })
          .def(
              "layoutAndAddChild",
              [](uilayoutgroup_ptr_t lgrp, uiwidget_ptr_t w) -> uilayout_ptr_t { //
                return lgrp->layoutAndAddChild(w);
              })
          .def("makeGrid", [](uilayoutgroup_ptr_t lgrp, py::kwargs kwargs) -> py::list { //
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
          });
  type_codec->registerStdCodec<uilayoutgroup_ptr_t>(layoutgroup_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto surface_type = //
      py::class_<ui::Surface, ui::Group, uisurface_ptr_t>(uimodule, "UiSurface")
          .def(
              "onPostRender",
              [](uisurface_ptr_t surface, py::object callback) { //
                 OrkAssert(py::hasattr(callback, "__call__"));
                  surface->_postRenderCallback = [callback](){
                      py::gil_scoped_acquire acquire_gil;
                      callback();
                  };
             })
          //////////////////////////////////
          .def("decoupleFromUiSize",
                [](uisurface_ptr_t surface, int w, int h) { //
                return surface->decoupleFromUiSize(w,h);
              })
          //////////////////////////////////
          .def_property(
              "aspect_from_rtgroup",
              [](uisurface_ptr_t surface) -> bool { //
                return surface->_aspect_from_rtgroup;
              },
              [](uisurface_ptr_t surface, bool x)  { //
                return surface->_aspect_from_rtgroup = x;
              }
              )
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
      py::class_<ui::Viewport, ui::Surface, uiviewport_ptr_t>(uimodule, "UiViewport");
  type_codec->registerStdCodec<uiviewport_ptr_t>(viewport_type);

  /////////////////////////////////////////////////////////////////////////////////
  auto decodeUIargs = [type_codec](py::list py_args) -> std::vector<svar64_t> {
    std::vector<svar64_t> decoded_args;
    for (auto list_item : py_args) {
      auto item_val = py::cast<py::object>(list_item);
      svar64_t val  = type_codec->decode(item_val);
      decoded_args.push_back(val);
    }
    return decoded_args;
  };
  /////////////////////////////////////////////////////////////////////////////////
  auto box_type = //
      py::class_<ui::Box, ui::Widget, uibox_ptr_t>(uimodule, "UiBox")
          .def_static(
              "uigridfactory", [decodeUIargs](uilayoutgroup_ptr_t lg, int grid_w, int grid_h, int m, py::list py_args) -> py::list { //
                auto decoded_args = decodeUIargs(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto layoutitems  = lg->makeGridOfWidgets<ui::Box>(grid_w, grid_h, name, color);
                py::list rval;
                for (auto item : layoutitems) {
                  auto shared_item     = std::make_shared<ui::LayoutItemBase>();
                  shared_item->_widget = item._widget;
                  shared_item->_layout = item._layout;
                  rval.append(shared_item);
                }
                return rval;
              });
  type_codec->registerStdCodec<uibox_ptr_t>(box_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto evtestbox_type = //
      py::class_<ui::EvTestBox, ui::Widget, uievtestbox_ptr_t>(uimodule, "UiEvTestBox")
          .def_static(
              "uigridfactory", [decodeUIargs](uilayoutgroup_ptr_t lg, int grid_w, int grid_h, int m, py::list py_args) -> py::list { //
                auto decoded_args = decodeUIargs(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto layoutitems  = lg->makeGridOfWidgets<ui::EvTestBox>(grid_w, grid_h, name, color);
                py::list rval;
                for (auto item : layoutitems) {
                  auto shared_item     = std::make_shared<ui::LayoutItemBase>();
                  shared_item->_widget = item._widget;
                  shared_item->_layout = item._layout;
                  rval.append(shared_item);
                }
                return rval;
              });
  type_codec->registerStdCodec<uievtestbox_ptr_t>(evtestbox_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto lambdabox_type = //
      py::class_<ui::LambdaBox, ui::Widget, uilambdabox_ptr_t>(uimodule, "UiLambdaBox")
          .def_static(
              "uigridfactory",
              [decodeUIargs](uilayoutgroup_ptr_t lg, int grid_w, int grid_h, int m, py::list py_args) -> py::list { //
                auto decoded_args = decodeUIargs(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto layoutitems  = lg->makeGridOfWidgets<ui::LambdaBox>(grid_w, grid_h, name, color);
                py::list rval;
                for (auto item : layoutitems) {
                  auto shared_item     = std::make_shared<ui::LayoutItemBase>();
                  printf( "item._widget<%p>\n", (void*) item._widget.get() );
                  shared_item->_widget = item._widget;
                  shared_item->_layout = item._layout;
                  rval.append(shared_item);
                }
                return rval;
              })
          .def("onPressed", [](uilambdabox_ptr_t lbox, py::object on_pressed) { //
            lbox->_onPressed = [on_pressed,lbox]() {
              printf( "pressing lbox<%p>\n", (void*) lbox.get() );
              py::gil_scoped_acquire acquire;
              on_pressed();
            };
          });
  type_codec->registerStdCodec<uilambdabox_ptr_t>(lambdabox_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto sgviewport_type = //
      py::class_<ui::SceneGraphViewport, ui::Viewport, uisgviewport_ptr_t>(uimodule, "UiSceneGraphViewport")
          .def_static(
              "uigridfactory",
              [decodeUIargs](uilayoutgroup_ptr_t lg, int grid_w, int grid_h, int m, py::list py_args) -> py::list { //
                auto decoded_args = decodeUIargs(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitems  = lg->makeGridOfWidgets<ui::SceneGraphViewport>(grid_w, grid_h, name);
                py::list rval;
                for (auto item : layoutitems) {
                  auto shared_item     = std::make_shared<ui::LayoutItemBase>();
                  printf( "item._widget<%p>\n", (void*) item._widget.get() );
                  shared_item->_widget = item._widget;
                  shared_item->_layout = item._layout;
                  rval.append(shared_item);
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
              [](uisgviewport_ptr_t sgview, lev2::scenegraph::scene_ptr_t sg) 
                { return sgview->_scenegraph = sg; 
                })
          //////////////////////////////////
          .def_property(
              "cameraName",
              [](uisgviewport_ptr_t sgview) -> std::string { //
                return sgview->_cameraname;
              },
              [](uisgviewport_ptr_t sgview, std::string camname) 
                { return sgview->_cameraname = camname; 
                });
  type_codec->registerStdCodec<uisgviewport_ptr_t>(sgviewport_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto layout_type = //
      py::class_<ui::anchor::Layout, uilayout_ptr_t>(uimodule, "UiLayout")
          //////////////////////////////////
          .def_property(
              "top",
              [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
                return layout->top();
              },
              [](uilayout_ptr_t layout, uiguide_ptr_t g) { layout->_top = g; })
          //////////////////////////////////
          .def_property(
              "bottom",
              [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
                return layout->bottom();
              },
              [](uilayout_ptr_t layout, uiguide_ptr_t g) { layout->_bottom = g; })
          //////////////////////////////////
          .def_property(
              "left",
              [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
                return layout->left();
              },
              [](uilayout_ptr_t layout, uiguide_ptr_t g) { layout->_left = g; })
          //////////////////////////////////
          .def_property(
              "right",
              [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
                return layout->right();
              },
              [](uilayout_ptr_t layout, uiguide_ptr_t g) { layout->_right = g; })
          //////////////////////////////////
          .def_property(
              "centerH",
              [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
                return layout->centerH();
              },
              [](uilayout_ptr_t layout, uiguide_ptr_t g) { layout->_centerH = g; })
          //////////////////////////////////
          .def_property(
              "centerV",
              [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
                return layout->centerV();
              },
              [](uilayout_ptr_t layout, uiguide_ptr_t g) { layout->_centerV = g; })
          //////////////////////////////////
          .def_property(
              "margin",
              [](uilayout_ptr_t layout) -> int { //
                return layout->_margin;
              },
              [](uilayout_ptr_t layout, int m) { layout->setMargin(m); })
          //////////////////////////////////
          .def(
              "proportionalHorizontalGuide",
              [](uilayout_ptr_t layout, float prop) -> uiguide_ptr_t { //
                return layout->proportionalHorizontalGuide(prop);
              })
          //////////////////////////////////
          .def(
              "proportionalVerticalGuide",
              [](uilayout_ptr_t layout, float prop) -> uiguide_ptr_t { //
                return layout->proportionalVerticalGuide(prop);
              })
          //////////////////////////////////
          .def(
              "fixedHorizontalGuide",
              [](uilayout_ptr_t layout, int fixed) -> uiguide_ptr_t { //
                return layout->fixedHorizontalGuide(fixed);
              })
          //////////////////////////////////
          .def(
              "fixedVerticalGuide",
              [](uilayout_ptr_t layout, int fixed) -> uiguide_ptr_t { //
                return layout->fixedVerticalGuide(fixed);
              })
          //////////////////////////////////
          .def(
              "centerIn",
              [](uilayout_ptr_t layout, uilayout_ptr_t other_layout) { //
                return layout->centerIn(other_layout.get());
              })
          //////////////////////////////////
          .def(
              "childLayout",
              [](uilayout_ptr_t layout, uiwidget_ptr_t w) -> uilayout_ptr_t { //
                return layout->childLayout(w.get());
              })
          //////////////////////////////////
          .def("fill", [](uilayout_ptr_t layout, uilayout_ptr_t other) { //
            return layout->fill(other.get());
          });
  //////////////////////////////////
  type_codec->registerStdCodec<uilayout_ptr_t>(layout_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto guide_type = //
      py::class_<ui::anchor::Guide, uiguide_ptr_t>(uimodule, "UiGuide")
          .def_property_readonly(
              "margin",
              [](uiguide_ptr_t guide) -> int { //
                return guide->_margin;
              })
          .def_property_readonly(
              "sign",
              [](uiguide_ptr_t guide) -> int { //
                return guide->_sign;
              })
          .def_property_readonly(
              "fixed",
              [](uiguide_ptr_t guide) -> int { //
                return guide->_fixed;
              })
          .def_property_readonly("proportion", [](uiguide_ptr_t guide) -> float { //
            return guide->_proportion;
          });
  type_codec->registerStdCodec<uiguide_ptr_t>(guide_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto litem_type = //
      py::class_<ui::LayoutItemBase, uilayoutitem_ptr_t>(uimodule, "LayoutItem")
          .def_property_readonly(
              "widget",
              [](uilayoutitem_ptr_t item) -> uiwidget_ptr_t { //
                return item->_widget;
              })
          .def_property_readonly("layout", [](uilayoutitem_ptr_t item) -> uilayout_ptr_t { //
            return item->_layout;
          });
  type_codec->registerStdCodec<uilayoutitem_ptr_t>(litem_type);
  /////////////////////////////////////////////////////////////////////////////////
}

} // namespace ork::lev2