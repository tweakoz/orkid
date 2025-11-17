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
              "offsetHorizontalGuide",
              [](uilayout_ptr_t layout, uiguide_ptr_t base, int offset,bool locked) -> uiguide_ptr_t { //
                auto guide = layout->offsetHorizontalGuide(base, offset);
                if( locked ) guide->lock();
                return guide;
              }, py::arg("base"), py::arg("offset"), py::arg("locked")=false)
          //////////////////////////////////
          .def(
              "offsetVerticalGuide",
              [](uilayout_ptr_t layout, uiguide_ptr_t base, int offset,bool locked) -> uiguide_ptr_t { //
                auto guide = layout->offsetVerticalGuide(base, offset);
                if( locked ) guide->lock();
                return guide;
              }, py::arg("base"), py::arg("offset"), py::arg("locked")=false)
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
              "setProportionalRect",
              [](uilayout_ptr_t layout, uilayout_ptr_t parent, float x, float y, float w, float h, bool locked) { //
                layout->setProportionalRect(parent.get(), x, y, w, h, locked);
              },
              py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("w"), py::arg("h"), py::arg("locked") = false)
          //////////////////////////////////
          .def(
              "setFixedRect",
              [](uilayout_ptr_t layout, uilayout_ptr_t parent, int x, int y, int w, int h, bool locked) { //
                layout->setFixedRect(parent.get(), x, y, w, h, locked);
              },
              py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("w"), py::arg("h"), py::arg("locked") = false)
          //////////////////////////////////
          .def(
              "setRect",
              [](uilayout_ptr_t layout, uiguide_ptr_t top, uiguide_ptr_t left,
                 uiguide_ptr_t right, uiguide_ptr_t bottom) { //
                layout->setRect(top, left, right, bottom);
              },
              py::arg("top") = nullptr, py::arg("left") = nullptr,
              py::arg("right") = nullptr, py::arg("bottom") = nullptr)
          //////////////////////////////////
          .def(
              "dump",
              [](uilayout_ptr_t layout) { //
                layout->dump();
              })
          //////////////////////////////////
          .def(
              "findGuideBetween",
              [](uilayout_ptr_t layout, uilayout_ptr_t layout_a, uilayout_ptr_t layout_b) -> uiguide_ptr_t { //
                return layout->findGuideBetween(layout_a, layout_b);
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
          //////////////////////////////////
          // Programmatic control API
          //////////////////////////////////
          .def("lock", [](uiguide_ptr_t guide) { guide->lock(); })
          .def("unlock", [](uiguide_ptr_t guide) { guide->unlock(); })
          .def("clamp", [](uiguide_ptr_t guide) { guide->clamp(); })
          .def("unclamp", [](uiguide_ptr_t guide) { guide->unclamp(); })
          //////////////////////////////////
          // Query API (properties)
          //////////////////////////////////
          .def_property(
              "proportion",
              [](uiguide_ptr_t guide) -> float { //
                return guide->getProportion();
              },
              [](uiguide_ptr_t guide, float prop) { //
                guide->setProportion(prop);
              })
          .def_property(
              "fixed",
              [](uiguide_ptr_t guide) -> int { //
                return guide->getFixed();
              },
              [](uiguide_ptr_t guide, int fixed) { //
                guide->setFixed(fixed);
              })
          .def_property(
              "clamped",
              [](uiguide_ptr_t guide) -> bool { //
                return guide->isClamped();
              },
              [](uiguide_ptr_t guide, bool c) { //
                if (c) guide->clamp(); else guide->unclamp();
              })
          .def_property_readonly(
              "type",
              [](uiguide_ptr_t guide) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>(static_cast<uint64_t>(guide->getType()));
              })
          .def_property_readonly(
              "locked",
              [](uiguide_ptr_t guide) -> bool { //
                return guide->isLocked();
              })
          .def_property_readonly(
              "edge",
              [](uiguide_ptr_t guide) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>(static_cast<uint64_t>(guide->getEdge()));
              })
          .def_property_readonly(
              "margin",
              [](uiguide_ptr_t guide) -> int { //
                return guide->getMargin();
              })
          .def_property_readonly(
              "is_vertical",
              [](uiguide_ptr_t guide) -> bool { //
                return guide->isVertical();
              })
          .def_property_readonly(
              "is_horizontal",
              [](uiguide_ptr_t guide) -> bool { //
                return guide->isHorizontal();
              })
          //////////////////////////////////
          // Existing methods
          //////////////////////////////////
          .def(
              "anchorTo",
              [](uiguide_ptr_t guide, uiguide_ptr_t other_guide) { //
                guide->anchorTo(other_guide);
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
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> uilayoutgroup_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto lg          = std::make_shared<ui::LayoutGroup>(name);
                return lg;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::LayoutGroup>(name);
                return layoutitem.as_shared();
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
              "findGuideBetween",
              [](uilayoutgroup_ptr_t lgrp, uilayout_ptr_t layout_a, uilayout_ptr_t layout_b) -> uiguide_ptr_t { //
                return lgrp->findGuideBetween(layout_a, layout_b);
              })
          .def(
              "split",
              [](uilayoutgroup_ptr_t lgrp, py::kwargs kwargs) -> uilayoutitem_ptr_t { //
                uilayout_ptr_t target_layout;
                float proportion = 0.5f;
                uint64_t placement_token = 0;
                int margin = 0;
                py::list args;
                py::object wfactory;
                int args_parsed = 0;

                for (auto item : kwargs) {
                  auto key = py::cast<std::string>(item.first);
                  if (key == "layout") {
                    target_layout = py::cast<uilayout_ptr_t>(item.second);
                    args_parsed++;
                  } else if (key == "proportion") {
                    proportion = py::cast<float>(item.second);
                    args_parsed++;
                  } else if (key == "placement") {
                    auto crcstr = py::cast<crcstring_ptr_t>(item.second);
                    placement_token = crcstr->hashed();
                    args_parsed++;
                  } else if (key == "margin") {
                    margin = py::cast<int>(item.second);
                    args_parsed++;
                  } else if (key == "uiclass") {
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

                OrkAssert(args_parsed == 6);

                // Cast token to enum
                auto placement_enum = static_cast<ui::anchor::ELayoutSplitPlacement>(placement_token);

                // Call C++ method - creates container and new layout, returns LayoutItem
                // The LayoutItem contains: _widget = container, _layout = new child layout
                auto layout_item = lgrp->split(target_layout, proportion, placement_enum, margin);

                // Get the container (which is a LayoutGroup) and the new layout
                auto container = std::dynamic_pointer_cast<ui::LayoutGroup>(layout_item->_widget);
                auto new_layout = layout_item->_layout;

                // Create the widget via wfactory (returns raw widget without layout)
                auto new_widget = py::cast<ui::widget_ptr_t>(wfactory(args));

                // Assign widget to the layout
                new_layout->_widget = new_widget.get();
                container->addChild(new_widget, false); // this will retain the widget

                // Now that widget is assigned and added, update the layouts
                printf("=== DUMP in binding BEFORE updateAll ===\n");
                lgrp->dumpLayoutHierarchy();
                new_layout->updateAll();

                // Return the layout item with the new widget
                layout_item->_widget = new_widget;
                return layout_item;
              })
          .def(
              "dumpLayoutHierarchy",
              [](uilayoutgroup_ptr_t lgrp) { //
                lgrp->dumpLayoutHierarchy();
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

                  // Apply margin to layout and all children
                  auto layout = lgrp->_layout;
                  layout->setMargin(margin);
                  for (auto child : layout->_childlayouts) {
                    child->setMargin(margin);
                  }
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
              })
              .def_property("clear",
              [](uilayoutgroup_ptr_t lgrp) -> bool { //
                return lgrp->_clear;
              },
              [](uilayoutgroup_ptr_t lgrp, bool b) { //
                lgrp->_clear = b;
              })
          .def_property(
              "overlay_widget",
              [](uilayoutgroup_ptr_t lgrp) -> uiwidget_ptr_t { //
                return lgrp->_overlay_widget;
              },
              [](uilayoutgroup_ptr_t lgrp, uiwidget_ptr_t w) { //
                lgrp->_overlay_widget = w;
                if (w) {
                  lgrp->addChild(w);
                }
              })
          .def_property(
              "overlay_enabled",
              [](uilayoutgroup_ptr_t lgrp) -> bool { //
                return lgrp->_overlay_enabled;
              },
              [](uilayoutgroup_ptr_t lgrp, bool enabled) { //
                lgrp->_overlay_enabled = enabled;
              });
  type_codec->registerStdCodec<uilayoutgroup_ptr_t>(layoutgroup_type);


}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
