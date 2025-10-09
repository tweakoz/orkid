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

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
using namespace ged;
///////////////////////////////////////////////////////////////////////////////

void pyinit_ui_layout(py::module& uimodule) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto layout_type = //
      py::class_<ui::anchor::Layout, uilayout_ptr_t>(uimodule, "Layout")
          //////////////////////////////////
          //.def(py::init<ui::Widget*>())
          //////////////////////////////////
          .def_property_readonly(
              "top",
              [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
                return layout->top();
              })
          //////////////////////////////////
          .def_property_readonly(
              "bottom",
              [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
                return layout->bottom();
              })
          //////////////////////////////////
          .def_property_readonly(
              "left",
              [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
                return layout->left();
              })
          //////////////////////////////////
          .def_property_readonly(
              "right",
              [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
                return layout->right();
              })
          //////////////////////////////////
          .def_property_readonly(
              "centerH",
              [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
                return layout->centerH();
              })
          //////////////////////////////////
          .def_property_readonly(
              "centerV",
              [](uilayout_ptr_t layout) -> uiguide_ptr_t { //
                return layout->centerV();
              })
          //////////////////////////////////
          .def_property(
              "margin",
              [](uilayout_ptr_t layout) -> int { //
                return layout->_margin;
              },
              [](uilayout_ptr_t layout, int m) { layout->setMargin(m); })
          //////////////////////////////////
          .def(
              "updateAll",
              [](uilayout_ptr_t layout) { //
                return layout->updateAll();
              })
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
                layout->centerIn(other_layout.get());
              })
          //////////////////////////////////
          .def(
              "childLayout",
              [](uilayout_ptr_t layout, uiwidget_ptr_t w) -> uilayout_ptr_t { //
                return layout->childLayout(w.get());
              })
          //////////////////////////////////
          .def(
              "removeChild",
              [](uilayout_ptr_t layout, uilayout_ptr_t ch) { //
                layout->removeChild(ch);
              })
          //////////////////////////////////
          .def(
              "fill",
              [](uilayout_ptr_t layout, uilayout_ptr_t other) { //
                layout->fill(other.get());
              })
          //////////////////////////////////
          .def(
              "dump",
              [](uilayout_ptr_t layout) { //
                layout->dump();
              })
          //////////////////////////////////
          .def_property(
              "locked",
              [](uilayout_ptr_t layout) -> bool { //
                return layout->_locked;
              },
              [](uilayout_ptr_t layout, bool locked) { //
                layout->_locked = locked;
              });
  //////////////////////////////////
  type_codec->registerStdCodec<uilayout_ptr_t>(layout_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto guide_type = //
      py::class_<ui::anchor::Guide, uiguide_ptr_t>(uimodule, "Guide")
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
          //////////////////////////////////
          .def_property(
              "proportion",
              [](uiguide_ptr_t guide) -> float { //
                return guide->_proportion;
              },
              [](uiguide_ptr_t guide, float prop) { //
                guide->_proportion = prop;
              })
          //////////////////////////////////
          .def(
              "anchorTo",
              [](uiguide_ptr_t guide, uiguide_ptr_t other_guide) { //
                guide->anchorTo(other_guide);
              })
          //////////////////////////////////
          .def_property(
              "locked",
              [](uiguide_ptr_t guide) -> bool { //
                return guide->_locked;
              },
              [](uiguide_ptr_t guide, bool locked) { //
                guide->_locked = locked;
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
          })
          .def("__repr__", [](uilayoutitem_ptr_t item) {
            return FormatString("<LayoutItem widget<%p> layout<%p>>", (void*)item->_widget.get(), (void*)item->_layout.get());
          });
  type_codec->registerStdCodec<uilayoutitem_ptr_t>(litem_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto layoutgroup_type = //
      py::class_<ui::LayoutGroup, ui::Group, uilayoutgroup_ptr_t>(uimodule, "LayoutGroup")
          .def_static("create", [](std::string name) -> uilayoutgroup_ptr_t { //
            return std::make_shared<ui::LayoutGroup>(name);
          })
          .def_property(
              "clearColorStd",
              [](uilayoutgroup_ptr_t lgrp) -> fvec4 { //
                return lgrp->_clearColorStd;
              },
              [](uilayoutgroup_ptr_t lgrp, fvec4 c) { //
                lgrp->_clearColorStd = c;
              })
          .def_property(
              "clearColorGuide",
              [](uilayoutgroup_ptr_t lgrp) -> fvec4 { //
                return lgrp->_clearColorGuide;
              },
              [](uilayoutgroup_ptr_t lgrp, fvec4 c) { //
                lgrp->_clearColorGuide = c;
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
                  auto litem = lgrp->makeChild<ui::EvTestBox>(name, color_normal);
                  rval       = litem.as_shared();
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
                  for (auto item : rval) {
                    auto litem = py::cast<uilayoutitem_ptr_t>(item);
                    printf("layoutgroup_type makeRowsColumns item<%p> w<%p>\n", (void*)litem.get(), (void*)litem->_widget.get());
                  }
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
                auto layout = lgrp->_layout;
                layout->setMargin(m);
                for (auto child : layout->_childlayouts) {
                  child->setMargin(m);
                }
              });
  type_codec->registerStdCodec<uilayoutgroup_ptr_t>(layoutgroup_type);


}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
