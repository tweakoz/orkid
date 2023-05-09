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
  auto type_codec = python::TypeCodec::instance();

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
          .def(
              "removeChild",
              [](uilayout_ptr_t layout, uilayout_ptr_t ch) { //
                layout->removeChild(ch);
              })
          //////////////////////////////////
          .def("fill", [](uilayout_ptr_t layout, uilayout_ptr_t other) { //
            return layout->fill(other.get());
          })
          //////////////////////////////////
          .def("dump", [](uilayout_ptr_t layout) { //
            return layout->dump();
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
  }

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
