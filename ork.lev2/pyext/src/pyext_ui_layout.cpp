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
  auto type_codec = python::typecodec_t::instance();

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
          });
  type_codec->registerStdCodec<uilayoutitem_ptr_t>(litem_type);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
