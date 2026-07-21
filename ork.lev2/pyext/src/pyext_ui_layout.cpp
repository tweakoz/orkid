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
#include <ork/lev2/ui/dock_space.h>
#include <ork/lev2/ui/layoutsurface.h>
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
  auto gridparams_type = //
      py::class_<ui::GridParams, ui::gridparams_ptr_t>(uimodule, "GridParams")
          .def(py::init<>())
          .def_readwrite("rows", &ui::GridParams::_rows)
          .def_readwrite("cols", &ui::GridParams::_cols)
          .def_readwrite("margin", &ui::GridParams::_margin)
          .def_readwrite("h_proportions", &ui::GridParams::_h_proportions)
          .def_readwrite("v_proportions", &ui::GridParams::_v_proportions);
  type_codec->registerStdCodec<ui::gridparams_ptr_t>(gridparams_type);
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
              [](uilayout_ptr_t layout, int m) { //
                layout->setMargin(m);
                for (auto child : layout->_childlayouts) {
                  child->setMargin(m);
                }
              })
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
                return layout->childLayout(w);
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
          .def("opposingLayout", [](uiguide_ptr_t guide, uilayout_ptr_t lo) -> uilayout_ptr_t { //
            return nullptr;
          })
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
              "unsplit",
              [](uilayoutgroup_ptr_t lgrp, uilayout_ptr_t container_layout) { //
                lgrp->unsplit(container_layout);
              })
          .def(
              "validateTree",
              [](uilayoutgroup_ptr_t lgrp) -> bool { //
                return lgrp->validateTree();
              })
          .def(
              "layoutSignature",
              [](uilayoutgroup_ptr_t lgrp) -> std::string { //
                return lgrp->layoutSignature();
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
                int margin = -1;  // Default to -1 to inherit from parent LayoutGroup
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

                OrkAssert(args_parsed >= 5 && args_parsed <= 6);  // margin is optional

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

                // Assign widget to the layout (bind weak ref for liveness detection)
                new_layout->bindWidget(new_widget);
                container->addChild(new_widget, false); // this will retain the widget

                // Now that widget is assigned and added, update the layouts
                //printf("=== DUMP in binding BEFORE updateAll ===\n");
                //lgrp->dumpLayoutHierarchy();
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
                  py::list args;
                  py::object uifactory;
                  bool fill = false;
                  int margin = -1;  // -1 means inherit from parent
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
                    } else if (key == "fill") {
                      fill = py::cast<bool>(item.second);
                    } else if (key == "margin") {
                      margin = py::cast<int>(item.second);
                    }
                  }
                  OrkAssert(args_parsed == 2);
                  rval = py::cast<uilayoutitem_ptr_t>(uifactory(lgrp, args));

                  // If fill=True, anchor child to fill parent
                  if (fill && rval && rval->_layout) {
                    auto parent_layout = lgrp->_layout;
                    rval->_layout->top()->anchorTo(parent_layout->top());
                    rval->_layout->left()->anchorTo(parent_layout->left());
                    rval->_layout->bottom()->anchorTo(parent_layout->bottom());
                    rval->_layout->right()->anchorTo(parent_layout->right());

                    // Apply margin if specified, otherwise inherit from parent
                    if (margin >= 0) {
                      rval->_layout->setMargin(margin);
                    } else {
                      rval->_layout->setMargin(lgrp->_margin);
                    }

                    // Update layout to compute geometry
                    parent_layout->updateAll();
                  }
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
                  py::object h_splits = py::none();  // Optional h_splits
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
                    } else if (key == "h_splits") {
                      h_splits = py::cast<py::object>(item.second);
                      // h_splits is optional, don't increment args_parsed
                    }
                  }
                  OrkAssert(args_parsed == 4);
                  rval = uirc_factory(lgrp, rccounts, margin, args, h_splits);
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
                  int margin = -1;  // Default -1 to inherit from LayoutGroup
                  py::list args;
                  py::object uigrid_factory;
                  int args_parsed = 0;
                  std::vector<float> h_proportions;
                  std::vector<float> v_proportions;
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
                    } else if (key == "h_proportions") {
                      py::list py_hprops = py::cast<py::list>(item.second);
                      for (auto py_val : py_hprops) {
                        float fval = py::cast<float>(py_val);
                        h_proportions.push_back(fval);
                      }
                      args_parsed++;
                    } else if (key == "v_proportions") {
                      py::list py_hprops = py::cast<py::list>(item.second);
                      for (auto py_val : py_hprops) {
                        float fval = py::cast<float>(py_val);
                        v_proportions.push_back(fval);
                      }
                      args_parsed++;
                    }
                  }
                  OrkAssert(args_parsed >= 4 && args_parsed <= 6);  // margin is optional

                  // Inherit margin from LayoutGroup if not specified
                  if (margin == -1) {
                    margin = lgrp->_margin;
                  }
                  auto gp = std::make_shared<ork::ui::GridParams>();
                  gp->_h_proportions = h_proportions;
                  gp->_v_proportions = v_proportions;
                  gp->_margin        = margin;
                  gp->_cols         = width;
                  gp->_rows        = height;
                  rval = uigrid_factory(lgrp, gp, args);

                  // Apply margin to layout and all children
                  auto layout = lgrp->_layout;
                  layout->setMargin(margin);
                  for (auto child : layout->_childlayouts) {
                    child->setMargin(margin);
                  }

                }
                return rval;
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
                // Propagate _uicontext to overlay and all its children
                std::function<void(ui::Widget*)> propagate = [&](ui::Widget* widget) {
                  widget->_uicontext = lgrp->_uicontext;
                  if (auto group = dynamic_cast<ui::Group*>(widget)) {
                    for (auto& child : group->_children) {
                      propagate(child.get());
                    }
                  }
                };
                propagate(w.get());
              })
          .def_property(
              "overlay_enabled",
              [](uilayoutgroup_ptr_t lgrp) -> bool { //
                return lgrp->_overlay_enabled;
              },
              [](uilayoutgroup_ptr_t lgrp, bool enabled) { //
                lgrp->_overlay_enabled = enabled;
              })
          .def_property(
              "profiler_overlay_widget",
              [](uilayoutgroup_ptr_t lgrp) -> uiwidget_ptr_t { //
                return lgrp->_profiler_overlay_widget;
              },
              [](uilayoutgroup_ptr_t lgrp, uiwidget_ptr_t w) { //
                lgrp->_profiler_overlay_widget = w;
                std::function<void(ui::Widget*)> propagate = [&](ui::Widget* widget) {
                  widget->_uicontext = lgrp->_uicontext;
                  if (auto group = dynamic_cast<ui::Group*>(widget)) {
                    for (auto& child : group->_children) {
                      propagate(child.get());
                    }
                  }
                };
                propagate(w.get());
              })
          .def_property(
              "profiler_overlay_enabled",
              [](uilayoutgroup_ptr_t lgrp) -> bool { //
                return lgrp->_profiler_overlay_enabled;
              },
              [](uilayoutgroup_ptr_t lgrp, bool enabled) { //
                lgrp->_profiler_overlay_enabled = enabled;
              });
  type_codec->registerStdCodec<uilayoutgroup_ptr_t>(layoutgroup_type);
  /////////////////////////////////////////////////////////////////////////////////
  // DockSpace : LayoutGroup — owns a dock tree of DockPanels; reuses split() verbatim.
  auto dockspace_type = //
      py::class_<ui::DockSpace, ui::LayoutGroup, ui::dockspace_ptr_t>(uimodule, "DockSpace")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::dockspace_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                return std::make_shared<ui::DockSpace>(name);
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::DockSpace>(name);
                return layoutitem.as_shared();
              })
          .def(
              "addPanel",
              [](ui::dockspace_ptr_t dock, py::kwargs kwargs) -> ui::dockpanel_ptr_t { //
                py::object wfactory;
                py::list args;
                std::string title;
                bool closeable = false;
                int parsed = 0;
                for (auto item : kwargs) {
                  auto key = py::cast<std::string>(item.first);
                  if (key == "uiclass") {
                    auto uiclass_obj  = py::cast<py::object>(item.second);
                    OrkAssert(py::hasattr(uiclass_obj, "wfactory"));
                    wfactory = uiclass_obj.attr("wfactory");
                    parsed++;
                  } else if (key == "args") {
                    args = py::cast<py::list>(item.second);
                    parsed++;
                  } else if (key == "title") {
                    title = py::cast<std::string>(item.second);
                  } else if (key == "closeable") {
                    closeable = py::cast<bool>(item.second);
                  }
                }
                OrkAssert(parsed == 2);  // uiclass + args required
                auto content = py::cast<ui::widget_ptr_t>(wfactory(args));
                return dock->addPanel(content, title, closeable);
              })
          .def(
              "split",
              [](ui::dockspace_ptr_t dock, py::kwargs kwargs) -> ui::dockpanel_ptr_t { //
                ui::dockpanel_ptr_t target;
                uint64_t placement_token = 0;
                float proportion = 0.5f;
                int margin       = -1;
                py::object wfactory;
                py::list args;
                std::string title;
                bool closeable = false;
                int parsed = 0;
                for (auto item : kwargs) {
                  auto key = py::cast<std::string>(item.first);
                  if (key == "target") {
                    target = py::cast<ui::dockpanel_ptr_t>(item.second);
                    parsed++;
                  } else if (key == "placement") {
                    auto crcstr = py::cast<crcstring_ptr_t>(item.second);
                    placement_token = crcstr->hashed();
                    parsed++;
                  } else if (key == "proportion") {
                    proportion = py::cast<float>(item.second);
                    parsed++;
                  } else if (key == "uiclass") {
                    auto uiclass_obj  = py::cast<py::object>(item.second);
                    OrkAssert(py::hasattr(uiclass_obj, "wfactory"));
                    wfactory = uiclass_obj.attr("wfactory");
                    parsed++;
                  } else if (key == "args") {
                    args = py::cast<py::list>(item.second);
                    parsed++;
                  } else if (key == "title") {
                    title = py::cast<std::string>(item.second);
                  } else if (key == "closeable") {
                    closeable = py::cast<bool>(item.second);
                  } else if (key == "margin") {
                    margin = py::cast<int>(item.second);
                  }
                }
                OrkAssert(parsed == 5);  // target + placement + proportion + uiclass + args
                auto placement = static_cast<ui::anchor::ELayoutSplitPlacement>(placement_token);
                auto content   = py::cast<ui::widget_ptr_t>(wfactory(args));
                return dock->splitPanel(target, placement, proportion, content, title, closeable, margin);
              })
          .def(
              "moveChild",
              [](ui::dockspace_ptr_t dock, py::kwargs kwargs) { //
                ui::dockpanel_ptr_t panel, target;
                uint64_t zone_token = 0;
                int parsed = 0;
                for (auto item : kwargs) {
                  auto key = py::cast<std::string>(item.first);
                  if (key == "panel") {
                    panel = py::cast<ui::dockpanel_ptr_t>(item.second);
                    parsed++;
                  } else if (key == "to") {
                    target = py::cast<ui::dockpanel_ptr_t>(item.second);
                    parsed++;
                  } else if (key == "zone") {
                    auto crcstr = py::cast<crcstring_ptr_t>(item.second);
                    zone_token = crcstr->hashed();
                    parsed++;
                  }
                }
                OrkAssert(parsed == 3);  // panel + to + zone
                dock->moveChild(panel, target, static_cast<ui::EDockZone>(zone_token));
              })
          .def(
              "removePanel",
              [](ui::dockspace_ptr_t dock, ui::dockpanel_ptr_t panel) { //
                dock->removePanel(panel);
              })
          .def(
              "zoneHitTest",
              [](ui::dockspace_ptr_t dock, int x, int y) -> py::object { //
                auto hit = dock->zoneHitTest(x, y);
                if (not hit._valid)
                  return py::none();
                const char* z = "CENTER";
                switch (hit._zone) {
                  case ui::EDockZone::LEFT:   z = "LEFT";   break;
                  case ui::EDockZone::RIGHT:  z = "RIGHT";  break;
                  case ui::EDockZone::TOP:    z = "TOP";    break;
                  case ui::EDockZone::BOTTOM: z = "BOTTOM"; break;
                  default: break;
                }
                return py::make_tuple(hit._target, std::string(z));
              })
          .def(
              "beginPanelDrag",
              [](ui::dockspace_ptr_t dock, ui::dockpanel_ptr_t panel) { //
                dock->beginPanelDrag(panel);
              })
          .def(
              "updatePanelDrag",
              [](ui::dockspace_ptr_t dock, int x, int y) { //
                dock->updatePanelDrag(x, y);
              })
          .def(
              "endPanelDrag",
              [](ui::dockspace_ptr_t dock, int x, int y) { //
                dock->endPanelDrag(x, y);
              })
          .def_property_readonly(
              "drag_active",
              [](ui::dockspace_ptr_t dock) -> bool { //
                return dock->dragActive();
              })
          .def(
              "serializeLayout",
              [](ui::dockspace_ptr_t dock) -> py::object { //
                using L_t = ork::ui::anchor::Layout;
                std::function<py::object(L_t*)> ser = [&](L_t* L) -> py::object {
                  auto w = L->_widget;
                  if (auto tabs = dynamic_cast<ui::TabWidget*>(w)) {
                    // leaf
                    py::list ids;
                    for (auto& ch : tabs->_children)
                      ids.append(ch->GetName());
                    int ai = tabs->getActiveTab();
                    std::string active;
                    if (ai >= 0 && ai < int(tabs->_children.size()))
                      active = tabs->_children[ai]->GetName();
                    py::dict d;
                    d["leaf"]   = ids;
                    d["active"] = active;
                    return std::move(d);
                  }
                  // interior split container (2 child layouts + 1 split guide)
                  OrkAssert(L->_childlayouts.size() == 2);
                  ork::ui::anchor::Guide* sg = nullptr;
                  for (auto& g : L->_customguides) { sg = g.get(); break; }
                  bool vertical = sg ? sg->isVertical() : true;
                  float prop    = sg ? sg->getProportion() : 0.5f;
                  auto& c0 = L->_childlayouts[0];
                  auto& c1 = L->_childlayouts[1];
                  auto ctr = [vertical](const ork::ui::anchor::layout_ptr_t& c) -> int {
                    auto g = c->_widget->geometry();
                    return vertical ? (g._x + g._w / 2) : (g._y + g._h / 2);
                  };
                  L_t* a = (ctr(c0) <= ctr(c1)) ? c0.get() : c1.get();
                  L_t* b = (a == c0.get()) ? c1.get() : c0.get();
                  py::dict d;
                  d["split"]      = std::string(vertical ? "V" : "H");
                  d["proportion"] = prop;
                  d["a"]          = ser(a);
                  d["b"]          = ser(b);
                  return std::move(d);
                };
                if (dock->_layout->_childlayouts.empty())
                  return py::none();
                return ser(dock->_layout->_childlayouts[0].get());
              })
          .def(
              "setSplitProportion",
              [](ui::dockspace_ptr_t dock, ui::dockpanel_ptr_t pa, ui::dockpanel_ptr_t pb, float prop) { //
                dock->setSplitProportion(pa, pb, prop);
              })
          .def(
              "activatePanel",
              [](ui::dockspace_ptr_t dock, ui::dockpanel_ptr_t panel) { //
                dock->activatePanel(panel);
              })
          .def(
              "reorderPanel",
              [](ui::dockspace_ptr_t dock, ui::dockpanel_ptr_t panel, int index) { //
                dock->reorderPanel(panel, index);
              })
          .def(
              "allPanels",
              [](ui::dockspace_ptr_t dock) -> std::vector<ui::dockpanel_ptr_t> { //
                return dock->allPanels();
              })
          .def_property_readonly(
              "num_panels",
              [](ui::dockspace_ptr_t dock) -> int { //
                return dock->numPanels();
              });
  type_codec->registerStdCodec<ui::dockspace_ptr_t>(dockspace_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto layoutsurface_type = //
      py::class_<ui::LayoutSurface, ui::Surface, ui::layoutsurface_ptr_t>(uimodule, "LayoutSurface")
          .def(py::init<>([](std::string name, int w, int h, int margin) -> ui::layoutsurface_ptr_t {
            return std::make_shared<ui::LayoutSurface>(name, 0, 0, w, h, margin);
          }), py::arg("name"), py::arg("w") = 0, py::arg("h") = 0, py::arg("margin") = 0)
          .def_property(
              "virtualWidth",
              [](ui::layoutsurface_ptr_t surf) -> int { return surf->getVirtualWidth(); },
              [](ui::layoutsurface_ptr_t surf, int w) { surf->setVirtualSize(w, surf->getVirtualHeight()); })
          .def_property(
              "virtualHeight",
              [](ui::layoutsurface_ptr_t surf) -> int { return surf->getVirtualHeight(); },
              [](ui::layoutsurface_ptr_t surf, int h) { surf->setVirtualSize(surf->getVirtualWidth(), h); })
          .def(
              "setVirtualSize",
              [](ui::layoutsurface_ptr_t surf, int w, int h) { surf->setVirtualSize(w, h); })
          .def_property(
              "scrollX",
              [](ui::layoutsurface_ptr_t surf) -> int { return surf->getScrollX(); },
              [](ui::layoutsurface_ptr_t surf, int x) { surf->setScrollPosition(x, surf->getScrollY()); })
          .def_property(
              "scrollY",
              [](ui::layoutsurface_ptr_t surf) -> int { return surf->getScrollY(); },
              [](ui::layoutsurface_ptr_t surf, int y) { surf->setScrollPosition(surf->getScrollX(), y); })
          .def(
              "setScrollPosition",
              [](ui::layoutsurface_ptr_t surf, int x, int y) { surf->setScrollPosition(x, y); })
          .def_property_readonly(
              "layoutGroup",
              [](ui::layoutsurface_ptr_t surf) -> uilayoutgroup_ptr_t { return surf->layoutGroup(); })
          .def_property_readonly(
              "layout",
              [](ui::layoutsurface_ptr_t surf) -> uilayout_ptr_t { return surf->layout(); })
              .def_property_readonly("uicontext",
              [](ui::layoutsurface_ptr_t surf) -> ui::context_ptr_t {
                return surf->_ownedContext;
              });
  type_codec->registerStdCodec<ui::layoutsurface_ptr_t>(layoutsurface_type);

}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
