////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/python/gil_safe_pyobj.h>
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/toolbar.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {

void pyinit_ui_toolbar(py::module& uimodule) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // ToolbarOrientation enum
  /////////////////////////////////////////////////////////////////////////////////
  py::enum_<ui::ToolbarOrientation>(uimodule, "ToolbarOrientation")
      .value("Horizontal", ui::ToolbarOrientation::Horizontal)
      .value("Vertical", ui::ToolbarOrientation::Vertical)
      .value("Auto", ui::ToolbarOrientation::Auto);

  /////////////////////////////////////////////////////////////////////////////////
  // ToolbarItem base class
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ui::ToolbarItem, ui::toolbar_item_ptr_t>(uimodule, "ToolbarItem")
      .def_readonly("id", &ui::ToolbarItem::_id)
      .def_readwrite("enabled", &ui::ToolbarItem::_enabled)
      .def_readwrite("visible", &ui::ToolbarItem::_visible)
      .def_readonly("x", &ui::ToolbarItem::_x)
      .def_readonly("y", &ui::ToolbarItem::_y)
      .def_readonly("width", &ui::ToolbarItem::_width)
      .def_readonly("height", &ui::ToolbarItem::_height);

  /////////////////////////////////////////////////////////////////////////////////
  // ToolbarButton
  /////////////////////////////////////////////////////////////////////////////////
  auto toolbar_button_type = //
      py::class_<ui::ToolbarButton, ui::ToolbarItem, ui::toolbar_button_ptr_t>(uimodule, "ToolbarButton")
          .def(py::init<const std::string&>())
          .def_property(
              "icon",
              [](ui::toolbar_button_ptr_t btn) -> image_ptr_t { return btn->_icon_image; },
              [](ui::toolbar_button_ptr_t btn, image_ptr_t img) { btn->_icon_image = img; btn->_prev_icon_image = nullptr; })
          .def_property(
              "icon_provider",
              [](ui::toolbar_button_ptr_t btn) -> image_provider_ptr_t { return btn->_icon_provider; },
              [](ui::toolbar_button_ptr_t btn, image_provider_ptr_t prov) { btn->_icon_provider = prov; })
          .def_property(
              "hover_icon",
              [](ui::toolbar_button_ptr_t btn) -> image_ptr_t { return btn->_hover_image; },
              [](ui::toolbar_button_ptr_t btn, image_ptr_t img) { btn->_hover_image = img; btn->_prev_hover_image = nullptr; })
          .def_property(
              "pressed_icon",
              [](ui::toolbar_button_ptr_t btn) -> image_ptr_t { return btn->_pressed_image; },
              [](ui::toolbar_button_ptr_t btn, image_ptr_t img) { btn->_pressed_image = img; btn->_prev_pressed_image = nullptr; })
          .def_readwrite("custom_width", &ui::ToolbarButton::_custom_width)
          .def_readwrite("tooltip", &ui::ToolbarButton::_tooltip)
          .def_readwrite("label", &ui::ToolbarButton::_label)
          .def_readwrite("toggle_mode", &ui::ToolbarButton::_toggle_mode)
          .def_readwrite("toggled", &ui::ToolbarButton::_toggled)
          .def_readonly("hovered", &ui::ToolbarButton::_hovered)
          .def_readonly("pressed", &ui::ToolbarButton::_pressed)
          .def(
              "onPressed",
              [](ui::toolbar_button_ptr_t btn, py::object callback) {
                auto safe = python::gil_safe_pyobj(callback);
                btn->_onPressed = [safe]() {
                  py::gil_scoped_acquire acquire;
                  auto fn = safe.valueAs<py::object>();
                  (*fn)();
                };
              })
          .def(
              "onToggled",
              [](ui::toolbar_button_ptr_t btn, py::object callback) {
                auto safe = python::gil_safe_pyobj(callback);
                btn->_onToggled = [safe](bool toggled) {
                  py::gil_scoped_acquire acquire;
                  auto fn = safe.valueAs<py::object>();
                  (*fn)(toggled);
                };
              })
          .def(
              "onKeyEvent",
              [](ui::toolbar_button_ptr_t btn, py::object callback) {
                btn->_onKeyEvent = [callback](int keycode) {
                  py::gil_scoped_acquire acquire;
                  callback(keycode);
                };
              })
          .def("__repr__", [](ui::toolbar_button_ptr_t btn) {
            return FormatString("<ToolbarButton id<%s>>", btn->_id.c_str());
          });

  type_codec->registerStdCodec<ui::toolbar_button_ptr_t>(toolbar_button_type);

  /////////////////////////////////////////////////////////////////////////////////
  // ToolbarSeparator
  /////////////////////////////////////////////////////////////////////////////////
  auto toolbar_separator_type = //
      py::class_<ui::ToolbarSeparator, ui::ToolbarItem, ui::toolbar_separator_ptr_t>(uimodule, "ToolbarSeparator")
          .def(py::init<const std::string&>())
          .def_readwrite("thickness", &ui::ToolbarSeparator::_thickness)
          .def_readwrite("padding", &ui::ToolbarSeparator::_padding)
          .def("__repr__", [](ui::toolbar_separator_ptr_t sep) {
            return FormatString("<ToolbarSeparator id<%s>>", sep->_id.c_str());
          });

  type_codec->registerStdCodec<ui::toolbar_separator_ptr_t>(toolbar_separator_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Toolbar widget
  /////////////////////////////////////////////////////////////////////////////////
  auto toolbar_type = //
      py::class_<ui::Toolbar, ui::Widget, ui::toolbar_ptr_t>(uimodule, "Toolbar")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::toolbar_ptr_t {
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto toolbar      = std::make_shared<ui::Toolbar>(name);
                return toolbar;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t {
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::Toolbar>(name);
                return layoutitem.as_shared();
              })
          // Item management
          .def(
              "addButton",
              [](ui::toolbar_ptr_t toolbar, const std::string& id, image_ptr_t icon, const std::string& tooltip) -> ui::toolbar_button_ptr_t {
                return toolbar->addButton(id, icon, tooltip);
              },
              py::arg("id"),
              py::arg("icon"),
              py::arg("tooltip") = "")
          .def(
              "addButtonWithProvider",
              [](ui::toolbar_ptr_t toolbar, const std::string& id, image_provider_ptr_t provider, const std::string& tooltip) -> ui::toolbar_button_ptr_t {
                return toolbar->addButtonWithProvider(id, provider, tooltip);
              },
              py::arg("id"),
              py::arg("icon_provider"),
              py::arg("tooltip") = "")
          .def(
              "addTextButton",
              [](ui::toolbar_ptr_t toolbar, const std::string& id, const std::string& label, const std::string& tooltip) -> ui::toolbar_button_ptr_t {
                return toolbar->addTextButton(id, label, tooltip);
              },
              py::arg("id"),
              py::arg("label"),
              py::arg("tooltip") = "")
          .def(
              "addSeparator",
              [](ui::toolbar_ptr_t toolbar, const std::string& id) -> ui::toolbar_separator_ptr_t {
                return toolbar->addSeparator(id);
              },
              py::arg("id") = "")
          .def("removeItem", &ui::Toolbar::removeItem)
          .def("getItem", &ui::Toolbar::getItem)
          .def("getButton", &ui::Toolbar::getButton)
          .def("clear", &ui::Toolbar::clear)
          .def_property_readonly(
              "items",
              [](ui::toolbar_ptr_t toolbar) -> py::list {
                py::list result;
                for (const auto& item : toolbar->getItems()) {
                  result.append(item);
                }
                return result;
              })
          // Orientation
          .def_property(
              "orientation",
              &ui::Toolbar::getOrientation,
              &ui::Toolbar::setOrientation)
          .def_property_readonly(
              "effective_orientation",
              &ui::Toolbar::getEffectiveOrientation)
          .def_property_readonly(
              "is_horizontal",
              &ui::Toolbar::isHorizontal)
          // Appearance
          .def_readwrite("icon_size", &ui::Toolbar::_icon_size)
          .def_readwrite("button_padding", &ui::Toolbar::_button_padding)
          .def_readwrite("item_spacing", &ui::Toolbar::_item_spacing)
          .def_readwrite("edge_padding", &ui::Toolbar::_edge_padding)
          .def_property(
              "bgcolor",
              [](ui::toolbar_ptr_t toolbar) -> fvec4 { return toolbar->_bgcolor; },
              [](ui::toolbar_ptr_t toolbar, fvec4 c) { toolbar->_bgcolor = c; })
          .def_property(
              "button_color",
              [](ui::toolbar_ptr_t toolbar) -> fvec4 { return toolbar->_button_color; },
              [](ui::toolbar_ptr_t toolbar, fvec4 c) { toolbar->_button_color = c; })
          .def_property(
              "button_hover_color",
              [](ui::toolbar_ptr_t toolbar) -> fvec4 { return toolbar->_button_hover_color; },
              [](ui::toolbar_ptr_t toolbar, fvec4 c) { toolbar->_button_hover_color = c; })
          .def_property(
              "button_pressed_color",
              [](ui::toolbar_ptr_t toolbar) -> fvec4 { return toolbar->_button_pressed_color; },
              [](ui::toolbar_ptr_t toolbar, fvec4 c) { toolbar->_button_pressed_color = c; })
          .def_property(
              "button_toggled_color",
              [](ui::toolbar_ptr_t toolbar) -> fvec4 { return toolbar->_button_toggled_color; },
              [](ui::toolbar_ptr_t toolbar, fvec4 c) { toolbar->_button_toggled_color = c; })
          .def_property(
              "separator_color",
              [](ui::toolbar_ptr_t toolbar) -> fvec4 { return toolbar->_separator_color; },
              [](ui::toolbar_ptr_t toolbar, fvec4 c) { toolbar->_separator_color = c; })
          .def_property(
              "disabled_tint",
              [](ui::toolbar_ptr_t toolbar) -> fvec4 { return toolbar->_disabled_tint; },
              [](ui::toolbar_ptr_t toolbar, fvec4 c) { toolbar->_disabled_tint = c; })
          .def_property(
              "button_border_color",
              [](ui::toolbar_ptr_t toolbar) -> fvec4 { return toolbar->_button_border_color; },
              [](ui::toolbar_ptr_t toolbar, fvec4 c) { toolbar->_button_border_color = c; })
          .def_readwrite("button_border_width", &ui::Toolbar::_button_border_width)
          .def_readwrite("draw_background", &ui::Toolbar::_draw_background)
          // Label appearance
          .def_property(
              "label_color",
              [](ui::toolbar_ptr_t toolbar) -> fvec4 { return toolbar->_label_color; },
              [](ui::toolbar_ptr_t toolbar, fvec4 c) { toolbar->_label_color = c; })
          .def_property(
              "label_toggled_color",
              [](ui::toolbar_ptr_t toolbar) -> fvec4 { return toolbar->_label_toggled_color; },
              [](ui::toolbar_ptr_t toolbar, fvec4 c) { toolbar->_label_toggled_color = c; })
          .def_readwrite("label_padding", &ui::Toolbar::_label_padding)
          // Tooltips
          .def_readwrite("show_tooltips", &ui::Toolbar::_show_tooltips)
          .def_readwrite("tooltip_delay_ms", &ui::Toolbar::_tooltip_delay_ms)
          .def("__repr__", [](ui::toolbar_ptr_t toolbar) {
            return FormatString("<Toolbar name<%s> items<%zu>>", toolbar->GetName().c_str(), toolbar->getItems().size());
          });

  type_codec->registerStdCodec<ui::toolbar_ptr_t>(toolbar_type);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
