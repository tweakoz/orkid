////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/box.h>
#include <ork/lev2/ui/labelbox.h>
#include <ork/lev2/ui/textbox.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
void pyinit_ui_box(py::module& uimodule) {
  auto type_codec = python::pb11_typecodec_t::instance();
  auto box_type   = //
      py::class_<ui::Box, ui::Widget, uibox_ptr_t>(uimodule, "Box")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> uibox_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto box          = std::make_shared<ui::Box>(name, color);
                return box;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto layoutitem   = lg->makeChild<ui::Box>(name, color);
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
                int h             = rowcols.size();
                std::vector<int> rccounts;
                for (int i = 0; i < h; i++) {
                  int rc = py::cast<int>(rowcols[i]);
                  rccounts.push_back(rc);
                }
                // Set margin before calling makeWidgetsRC so it uses the correct value
                lg->_margin = m;
                lg->_layout->setMargin(m);
                auto layoutitems = lg->makeWidgetsRC<ui::Box>(rccounts, name, color);
                py::list rval;
                for (auto item : layoutitems) {
                  auto shitem = item.as_shared();
                  printf("box_type uircfactory item<%p>\n", (void*)shitem->_widget.get());
                  rval.append(shitem);
                }
                return rval;
              })
          .def_property(
              "pipeline",
              [](uibox_ptr_t box) -> lev2::fxpipeline_ptr_t { //
                return box->_pipeline_override;
              },
              [](uibox_ptr_t box, lev2::fxpipeline_ptr_t p) { //
                box->_pipeline_override = p;
              })
              .def_property(
              "color",
              [](uibox_ptr_t box) -> fvec4 { //
                return box->_color;
              },
              [](uibox_ptr_t box, fvec4 c) { //
                box->_color = c;
              })
          .def("__repr__", [](uibox_ptr_t box) {
            return FormatString("<Box name<%s> widget<%p>>", box->GetName().c_str(), (void*)box.get());
          });
  type_codec->registerStdCodec<uibox_ptr_t>(box_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto labbox_type = //
      py::class_<ui::LabelBox, ui::Widget, ui::labelbox_ptr_t>(uimodule, "LabelBox")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::labelbox_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto label        = decoded_args[2].get<std::string>();
                auto box          = std::make_shared<ui::LabelBox>(name, color, label);
                return box;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto label        = decoded_args[2].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::LabelBox>(name, color, label);
                return layoutitem.as_shared();
              })
          .def_static(
              "uigridfactory",
              [type_codec](uilayoutgroup_ptr_t lg, int grid_w, int grid_h, int m, py::list py_args) -> py::list { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto layoutitems  = lg->makeGridOfWidgets<ui::LabelBox>(grid_w, grid_h, name, color, "");
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
                int h             = rowcols.size();
                std::vector<int> rccounts;
                for (int i = 0; i < h; i++) {
                  int rc = py::cast<int>(rowcols[i]);
                  rccounts.push_back(rc);
                }
                // Set margin before calling makeWidgetsRC so it uses the correct value
                lg->_margin = m;
                lg->_layout->setMargin(m);
                auto layoutitems = lg->makeWidgetsRC<ui::LabelBox>(rccounts, name, color, "");
                py::list rval;
                for (auto item : layoutitems) {
                  auto shitem = item.as_shared();
                  printf("box_type uircfactory item<%p>\n", (void*)shitem->_widget.get());
                  rval.append(shitem);
                }
                return rval;
              })
          .def_property(
              "text",
              [](ui::labelbox_ptr_t box) -> std::string { //
                return box->_label;
              },
              [](ui::labelbox_ptr_t box, std::string txt) { //
                box->_label = txt;
              })
          .def_property(
              "halign",
              [](ui::labelbox_ptr_t box) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>((uint64_t)box->_halign);
              },
              [](ui::labelbox_ptr_t box, crcstring_ptr_t c) { //
                box->_halign = ui::ETextAlignH(c->hashed());
              })
          .def_property(
              "valign",
              [](ui::labelbox_ptr_t box) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>((uint64_t)box->_valign);
              },
              [](ui::labelbox_ptr_t box, crcstring_ptr_t c) { //
                box->_valign = ui::ETextAlignV(c->hashed());
              })
          .def("__repr__", [](ui::labelbox_ptr_t box) {
            return FormatString("<LabelBox name<%s> widget<%p>>", box->GetName().c_str(), (void*)box.get());
          });
  type_codec->registerStdCodec<ui::labelbox_ptr_t>(labbox_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto textbox_type = //
      py::class_<ui::TextBox, ui::Widget, ui::textbox_ptr_t>(uimodule, "TextBox")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::textbox_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto text         = decoded_args[2].get<std::string>();
                auto box          = std::make_shared<ui::TextBox>(name, color, text);
                return box;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto text         = decoded_args[2].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::TextBox>(name, color, text);
                auto as_tbox = std::dynamic_pointer_cast<ui::TextBox>(layoutitem._widget);
                as_tbox->setText(text);
                return layoutitem.as_shared();
              })
          .def_static(
              "uigridfactory",
              [type_codec](uilayoutgroup_ptr_t lg, int grid_w, int grid_h, int m, py::list py_args) -> py::list { //
                std::string text;
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                if( decoded_args.size()>=3 ){
                  if (auto as_str = decoded_args[2].tryAs<std::string>()) {
                    text = as_str.value();
                  }
                }
                auto layoutitems  = lg->makeGridOfWidgets<ui::TextBox>(grid_w, grid_h, name, color, text);
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
                int h             = rowcols.size();
                std::vector<int> rccounts;
                for (int i = 0; i < h; i++) {
                  int rc = py::cast<int>(rowcols[i]);
                  rccounts.push_back(rc);
                }
                // Set margin before calling makeWidgetsRC so it uses the correct value
                lg->_margin = m;
                lg->_layout->setMargin(m);
                auto layoutitems = lg->makeWidgetsRC<ui::TextBox>(rccounts, name, color, "");
                py::list rval;
                for (auto item : layoutitems) {
                  auto shitem = item.as_shared();
                  printf("box_type uircfactory item<%p>\n", (void*)shitem->_widget.get());
                  rval.append(shitem);
                }
                return rval;
              })
          .def(
              "setText",
              [](ui::textbox_ptr_t box, std::string txt) { //
                box->setText(txt);
              })
          .def_property(
              "halign",
              [](ui::textbox_ptr_t box) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>((uint64_t)box->_halign);
              },
              [](ui::textbox_ptr_t box, crcstring_ptr_t c) { //
                box->_halign = ui::ETextAlignH(c->hashed());
              })
          .def_property(
              "valign",
              [](ui::textbox_ptr_t box) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>((uint64_t)box->_valign);
              },
              [](ui::textbox_ptr_t box, crcstring_ptr_t c) { //
                box->_valign = ui::ETextAlignV(c->hashed());
              })
          .def_property(
              "font",
              [](ui::textbox_ptr_t box) -> font_ptr_t { //
                return box->_font;
              },
              [](ui::textbox_ptr_t box, font_ptr_t font) { //
                box->_font = font;
              })
          .def_property(
              "textcolor",
              [](ui::textbox_ptr_t box) -> fvec4 { //
                return box->_textcolor;
              },
              [](ui::textbox_ptr_t box, fvec4 clr) { //
                box->_textcolor = clr;
              })
          .def_property(
              "bgcolor",
              [](ui::textbox_ptr_t box) -> fvec4 { //
                return box->_color;
              },
              [](ui::textbox_ptr_t box, fvec4 clr) { //
                box->_color = clr;
              })
              .def("onMousePush", [](ui::textbox_ptr_t tbox, py::object on_mouse_push) { //
                tbox->_onMousePush = [=](ui::event_constptr_t ev) -> ui::HandlerResult {
                  py::gil_scoped_acquire acquire;
                  py::object rv = on_mouse_push(ev);
                  return py::cast<ui::HandlerResult>(rv);
                };
              })
              .def("onMouseRelease", [](ui::textbox_ptr_t tbox, py::object on_mouse_release) { //
                tbox->_onMouseRelease = [=](ui::event_constptr_t ev) -> ui::HandlerResult {
                  py::gil_scoped_acquire acquire;
                  py::object rv = on_mouse_release(ev);
                  return py::cast<ui::HandlerResult>(rv);
                };
              })
              .def("onMouseMove", [](ui::textbox_ptr_t tbox, py::object on_mouse_move) { //
                tbox->_onMouseMove = [=](ui::event_constptr_t ev) -> ui::HandlerResult {
                  py::gil_scoped_acquire acquire;
                  py::object rv = on_mouse_move(ev);
                  return py::cast<ui::HandlerResult>(rv);
                };
              })
              .def("onMouseDrag", [](ui::textbox_ptr_t tbox, py::object on_mouse_drag) { //
                tbox->_onMouseDrag = [=](ui::event_constptr_t ev) -> ui::HandlerResult {
                  py::gil_scoped_acquire acquire;
                  py::object rv = on_mouse_drag(ev);
                  return py::cast<ui::HandlerResult>(rv);
                };
              })
              .def("onKeyDown", [](ui::textbox_ptr_t tbox, py::object on_key_down) { //
                tbox->_onKeyDown = [=](ui::event_constptr_t ev) -> ui::HandlerResult {
                  py::gil_scoped_acquire acquire;
                  py::object rv = on_key_down(ev);
                  return py::cast<ui::HandlerResult>(rv);
                };
              })
              .def("onKeyUp", [](ui::textbox_ptr_t tbox, py::object on_key_up) { //
                tbox->_onKeyUp = [=](ui::event_constptr_t ev) -> ui::HandlerResult {
                  py::gil_scoped_acquire acquire;
                  py::object rv = on_key_up(ev);
                  return py::cast<ui::HandlerResult>(rv);
                };
              })
          .def("__repr__", [](ui::textbox_ptr_t box) {
            return FormatString("<TextBox name<%s> widget<%p>>", box->GetName().c_str(), (void*)box.get());
          });
  type_codec->registerStdCodec<ui::textbox_ptr_t>(textbox_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto evtestbox_type = //
      py::class_<ui::EvTestBox, ui::Widget, uievtestbox_ptr_t>(uimodule, "EvTestBox")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> uievtestbox_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto box          = std::make_shared<ui::EvTestBox>(name, color);
                return box;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto layoutitem   = lg->makeChild<ui::EvTestBox>(name, color);
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
              })
              .def_property("blendingBG",
              [](uievtestbox_ptr_t box) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>((uint64_t)box->_blendingBG);
              },
              [](uievtestbox_ptr_t box, crcstring_ptr_t c) { //
                box->_blendingBG = lev2::BlendingMacro(c->hashed());
              })
              .def_property("blendingFG",
              [](uievtestbox_ptr_t box) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>((uint64_t)box->_blendingFG);
              },
              [](uievtestbox_ptr_t box, crcstring_ptr_t c) { //
                box->_blendingFG = lev2::BlendingMacro(c->hashed());
              })
              .def_property("normal_color",
              [](uievtestbox_ptr_t box) -> fvec4 { //
                return box->_colorNormal;
              },
              [](uievtestbox_ptr_t box, fvec4 c) { //
                box->_colorNormal = c;
              })
              .def_property("click_color",
              [](uievtestbox_ptr_t box) -> fvec4 { //
                return box->_colorClick;
              },
              [](uievtestbox_ptr_t box, fvec4 c) { //
                box->_colorClick = c;
              })
              .def_property("doubleclick_color",
              [](uievtestbox_ptr_t box) -> fvec4 { //
                return box->_colorDoubleClick;
              },
              [](uievtestbox_ptr_t box, fvec4 c) { //
                box->_colorDoubleClick = c;
              })
              .def_property("drag_color",
              [](uievtestbox_ptr_t box) -> fvec4 { //
                return box->_colorDrag;
              },
              [](uievtestbox_ptr_t box, fvec4 c) { //
                box->_colorDrag = c;
              })
              .def_property("keydown_color",
              [](uievtestbox_ptr_t box) -> fvec4 { //
                return box->_colorKeyDown;
              },
              [](uievtestbox_ptr_t box, fvec4 c) { //
                box->_colorKeyDown = c;
              })
              .def_property("font_color",
              [](uievtestbox_ptr_t box) -> fvec4 { //
                return box->_fontColor;
              },
              [](uievtestbox_ptr_t box, fvec4 c) { //
                box->_fontColor = c;
              });
              type_codec->registerStdCodec<uievtestbox_ptr_t>(evtestbox_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto lambdabox_type = //
      py::class_<ui::LambdaBox, ui::Widget, uilambdabox_ptr_t>(uimodule, "LambdaBox")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> uilambdabox_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto box          = std::make_shared<ui::LambdaBox>(name, color);
                return box;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto color        = decoded_args[1].get<fvec4>();
                auto layoutitem   = lg->makeChild<ui::LambdaBox>(name, color);
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
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2