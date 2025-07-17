////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/util/ncui.h>
#include <ork/util/ncui_perfviz.h>
#include <ork/kernel/opq.h>

namespace ork {
using namespace notcurses;
void pyinit_ncui(py::module& ncui_module) {
  auto type_codec = python::pb11_typecodec_t::instance();

  ncui_module.doc() = "NotCurses UI Framework";

  /////////////////////////////////////////////////////////////////////////////////
  // Context
  /////////////////////////////////////////////////////////////////////////////////
  auto context_type = py::class_<Context, context_ptr_t>(ncui_module, "Context")
    .def_static("instance", &context, py::return_value_policy::reference)
    .def("isReady", &Context::isReady)
    .def_property("content", 
      [](context_ptr_t ctx) -> widget_ptr_t {
        return ctx->_root_widget ? ctx->_root_widget->_content : nullptr;
      },
      [](context_ptr_t ctx, widget_ptr_t content) {
        if (ctx->_root_widget) {
          ctx->_root_widget->setContent(content, ctx->_root_widget);
        }
      })
    .def("swapContent", 
      [](context_ptr_t ctx, widget_ptr_t new_content) -> widget_ptr_t {
        if (ctx->_root_widget) {
          return ctx->_root_widget->swapContent(new_content, ctx->_root_widget);
        }
        return nullptr;
      },
      py::return_value_policy::reference)
    .def_property_readonly("width", 
      [](context_ptr_t ctx) -> int { return ctx->_numcols; })
    .def_property_readonly("height", 
      [](context_ptr_t ctx) -> int { return ctx->_numrows; })
    .def("waitForExit", [](context_ptr_t ctx){
      py::gil_scoped_release release; // Release GIL for blocking call
      ctx->waitForExit();
    })
    .def("__repr__", [](context_ptr_t ctx) -> std::string {
      return FormatString("Context(%dx%d, ready=%s)", 
        ctx->_numcols, ctx->_numrows, ctx->isReady() ? "true" : "false");
    });
  type_codec->registerStdCodec<context_ptr_t>(context_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Widget Base Class
  /////////////////////////////////////////////////////////////////////////////////
  auto widget_type = py::class_<Widget, widget_ptr_t>(ncui_module, "Widget")
    // Note: No py::init<>() since Widget is abstract with pure virtual methods
    .def("resize", &Widget::resize)
    .def("setPosition", &Widget::setPosition)
    .def("clearArea", &Widget::clearArea)
    .def("getParent", [](widget_ptr_t w) -> py::object {
      auto parent_weak = w->getParent();
      if (auto parent = parent_weak.lock()) {
        return py::cast(parent, py::return_value_policy::reference_internal);
      }
      return py::none();
    })
    .def("getRoot", [](widget_ptr_t w) -> py::object {
      auto root_weak = w->getRoot();
      if (auto root = root_weak.lock()) {
        return py::cast(root, py::return_value_policy::reference_internal);
      }
      return py::none();
    })
    .def("getAncestors", [](widget_ptr_t w) -> py::list {
      py::list result;
      auto ancestors = w->getAncestors();
      for (const auto& weak_ancestor : ancestors) {
        if (auto ancestor = weak_ancestor.lock()) {
          result.append(ancestor);
        }
      }
      return result;
    })
    .def("isChildOf", [](widget_ptr_t w, widget_ptr_t potential_parent) -> bool {
      if (potential_parent) {
        return w->isChildOf(potential_parent);
      }
      return false;
    })
    .def_property("name", 
      [](widget_ptr_t w) -> std::string { return w->_name; },
      [](widget_ptr_t w, const std::string& name) { w->_name = name; })
    .def_property("x", 
      [](widget_ptr_t w) -> int { return w->_x; },
      [](widget_ptr_t w, int x) { w->_x = x; })
    .def_property("y", 
      [](widget_ptr_t w) -> int { return w->_y; },
      [](widget_ptr_t w, int y) { w->_y = y; })
    .def_property("width", 
      [](widget_ptr_t w) -> int { return w->_width; },
      [](widget_ptr_t w, int width) { w->_width = width; })
    .def_property("height", 
      [](widget_ptr_t w) -> int { return w->_height; },
      [](widget_ptr_t w, int height) { w->_height = height; })
    .def_property_readonly("parent", 
      [](widget_ptr_t w) -> py::object {
        auto parent_weak = w->getParent();
        if (auto parent = parent_weak.lock()) {
          return py::cast(parent, py::return_value_policy::reference_internal);
        }
        return py::none();
      })
    .def("__repr__", [](widget_ptr_t w) -> std::string {
      return FormatString("Widget(%dx%d at %d,%d)", w->_width, w->_height, w->_x, w->_y);
    });
  type_codec->registerStdCodec<widget_ptr_t>(widget_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Group Class
  /////////////////////////////////////////////////////////////////////////////////
  auto group_type = py::class_<Group, Widget, group_ptr_t>(ncui_module, "Group")
    .def("__repr__", [](group_ptr_t g) -> std::string {
      return FormatString("Group()");
    });
  type_codec->registerStdCodec<group_ptr_t>(group_type);
  /////////////////////////////////////////////////////////////////////////////////
  // TextLines Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto textlines_type = py::class_<TextLines, Widget, textlines_ptr_t>(ncui_module, "TextLines")
    .def(py::init<>())
    .def("addLine", &TextLines::addLine)
    .def("setLines", &TextLines::setLines)
    .def("setColor", &TextLines::setColor)
    .def("setScrolling", &TextLines::setScrolling)
    .def("setAutoScroll", &TextLines::setAutoScroll)
    .def_property("color", 
      [](textlines_ptr_t tl) -> ork::fvec3 { return tl->_color; },
      [](textlines_ptr_t tl, const ork::fvec3& color) { tl->setColor(color); })
    .def_property("bg_color", 
      [](textlines_ptr_t tl) -> ork::fvec3 { return tl->_bg_color; },
      [](textlines_ptr_t tl, const ork::fvec3& color) { tl->_bg_color = color; })
    .def_property("max_lines", 
      [](textlines_ptr_t tl) -> size_t { return tl->_max_lines; },
      [](textlines_ptr_t tl, size_t max_lines) { tl->_max_lines = max_lines; })
    .def_property_readonly("lines", 
      [](textlines_ptr_t tl) -> const std::vector<std::string>& { return tl->_lines; })
    .def("__repr__", [](textlines_ptr_t tl) -> std::string {
      return FormatString("TextLines(%zu lines, %dx%d)", tl->_lines.size(), tl->_width, tl->_height);
    });
  type_codec->registerStdCodec<textlines_ptr_t>(textlines_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Box Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto box_type = py::class_<Box, Widget, box_ptr_t>(ncui_module, "Box")
    .def(py::init<>())
    .def_property("bgcolor", 
      [](box_ptr_t box) -> ork::fvec3 { return box->_bgcolor; },
      [](box_ptr_t box, const ork::fvec3& color) { box->_bgcolor = color; })
    .def_property("fgcolor", 
      [](box_ptr_t box) -> ork::fvec3 { return box->_fgcolor; },
      [](box_ptr_t box, const ork::fvec3& color) { box->_fgcolor = color; })
    .def_property("text", 
      [](box_ptr_t box) -> std::string { return box->_text; },
      [](box_ptr_t box, const std::string& text) { //
        box->_text = text; //
      })
    .def_property("halign", 
      [](box_ptr_t box) -> crcstring_ptr_t {
        return std::make_shared<CrcString>(static_cast<uint64_t>(box->_halign));
      },
      [](box_ptr_t box, crcstring_ptr_t align) {
        box->_halign = HorizontalAlign(align->hashed());
      })
    .def_property("valign",
      [](box_ptr_t box) -> crcstring_ptr_t {
        return std::make_shared<CrcString>(static_cast<uint64_t>(box->_valign));
      },
      [](box_ptr_t box, crcstring_ptr_t align) {
        box->_valign = VerticalAlign(align->hashed());
      })
    .def("__repr__", [](box_ptr_t box) -> std::string {
      return FormatString("Box('%s', %dx%d)", box->_text.c_str(), box->_width, box->_height);
    });
  type_codec->registerStdCodec<box_ptr_t>(box_type);

  /////////////////////////////////////////////////////////////////////////////////
  // VerticalPack Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto verticalpack_type = py::class_<VerticalPack, Group, verticalpack_ptr_t>(ncui_module, "VerticalPack")
    .def(py::init<>())
    .def("addChild", [](verticalpack_ptr_t vp, widget_ptr_t child) {
      vp->addChild(child, vp); // Pass the container itself as parent
    })
    .def("removeChild", &VerticalPack::removeChild)
    .def_property_readonly("children", 
      [](verticalpack_ptr_t vp) -> const std::vector<widget_ptr_t>& { 
        return vp->_children; 
      })
    .def_property("spacing", 
      [](verticalpack_ptr_t vp) -> bool { return vp->_spacing; },
      [](verticalpack_ptr_t vp, bool spacing) { 
        vp->_spacing = spacing; 
        vp->_markLayoutDirty(); // Mark layout dirty to reapply spacing
      })
    .def_property("width_mode", 
      [](verticalpack_ptr_t vp) -> crcstring_ptr_t { //
        auto crcstr = std::make_shared<CrcString>(vp->_width_mode); //
        return crcstr; //
      },
      [](verticalpack_ptr_t vp, crcstring_ptr_t hmode) { //
        vp->_width_mode = hmode->hashed(); //
        vp->_markLayoutDirty(); // Mark layout dirty to reapply spacing
      })
    .def("__repr__", [](verticalpack_ptr_t vp) -> std::string {
      return FormatString("VerticalPack(%zu children, spacing=%.1f)", vp->_children.size(), vp->_spacing);
    });
  type_codec->registerStdCodec<verticalpack_ptr_t>(verticalpack_type);

  /////////////////////////////////////////////////////////////////////////////////
  // HorizontalPack Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto horizontalpack_type = py::class_<HorizontalPack, Group, horizontalpack_ptr_t>(ncui_module, "HorizontalPack")
    .def(py::init<>())
    .def("addChild", [](horizontalpack_ptr_t hp, widget_ptr_t child) {
      hp->addChild(child, hp); // Pass the container itself as parent
    })
    .def("removeChild", &HorizontalPack::removeChild)
    .def_property_readonly("children", 
      [](horizontalpack_ptr_t hp) -> const std::vector<widget_ptr_t>& { 
        return hp->_children; 
      })
    .def_property("spacing", 
      [](horizontalpack_ptr_t hp) -> bool { return hp->_spacing; },
      [](horizontalpack_ptr_t hp, bool spacing) { 
        hp->_spacing = spacing; 
        hp->_markLayoutDirty(); // Mark layout dirty to reapply spacing
      })
    .def_property("height_mode", 
      [](horizontalpack_ptr_t hp) -> crcstring_ptr_t { //
        auto crcstr = std::make_shared<CrcString>(hp->_height_mode); //
        return crcstr; //
      },
      [](horizontalpack_ptr_t hp, crcstring_ptr_t hmode) { //
        hp->_height_mode = hmode->hashed(); //
        hp->_markLayoutDirty(); // Mark layout dirty to reapply spacing
      })
    .def("__repr__", [](horizontalpack_ptr_t hp) -> std::string {
      return FormatString("HorizontalPack(%zu children, spacing=%.1f)", hp->_children.size(), hp->_spacing);
    });
  type_codec->registerStdCodec<horizontalpack_ptr_t>(horizontalpack_type);

  /////////////////////////////////////////////////////////////////////////////////
  // HorizSplit Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto horizsplit_type = py::class_<HorizSplit, Group, horizontalsplit_ptr_t>(ncui_module, "HorizSplit")
    .def(py::init<>())
    .def_property("left", 
      [](horizontalsplit_ptr_t hs) -> widget_ptr_t { return hs->_left; },
      [](horizontalsplit_ptr_t hs, widget_ptr_t widget) { hs->setLeft(widget, hs); })
    .def_property("right", 
      [](horizontalsplit_ptr_t hs) -> widget_ptr_t { return hs->_right; },
      [](horizontalsplit_ptr_t hs, widget_ptr_t widget) { hs->setRight(widget, hs); })
    .def_property("split_position", 
      [](horizontalsplit_ptr_t hs) -> float { return hs->_split_position; },
      [](horizontalsplit_ptr_t hs, float pos) { 
        hs->_split_position = std::clamp(pos, 0.0f, 1.0f); 
        hs->_onLayoutChanged();  // Immediately update layout
      })
    .def_property("split_color", 
      [](horizontalsplit_ptr_t hs) -> ork::fvec3 { return hs->_split_color; },
      [](horizontalsplit_ptr_t hs, const ork::fvec3& color) { hs->_split_color = color; })
    .def("setLeft", [](horizontalsplit_ptr_t hs, widget_ptr_t widget) {
      hs->setLeft(widget, hs); // Pass the container itself as parent
    })
    .def("setRight", [](horizontalsplit_ptr_t hs, widget_ptr_t widget) {
      hs->setRight(widget, hs); // Pass the container itself as parent
    })
    .def("__repr__", [](horizontalsplit_ptr_t hs) -> std::string {
      return FormatString("HorizSplit(split_pos=%.2f)", hs->_split_position);
    });
  type_codec->registerStdCodec<horizontalsplit_ptr_t>(horizsplit_type);

  /////////////////////////////////////////////////////////////////////////////////
  // VerticalSplit Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto verticalsplit_type = py::class_<VerticalSplit, Group, verticalsplit_ptr_t>(ncui_module, "VerticalSplit")
    .def(py::init<>())
    .def_property("top", 
      [](verticalsplit_ptr_t vs) -> widget_ptr_t { return vs->_top; },
      [](verticalsplit_ptr_t vs, widget_ptr_t widget) { vs->setTop(widget, vs); })
    .def_property("bottom", 
      [](verticalsplit_ptr_t vs) -> widget_ptr_t { return vs->_bottom; },
      [](verticalsplit_ptr_t vs, widget_ptr_t widget) { vs->setBottom(widget, vs); })
    .def_property("split_position", 
      [](verticalsplit_ptr_t vs) -> float { return vs->_split_position; },
      [](verticalsplit_ptr_t vs, float pos) { 
        vs->_split_position = std::clamp(pos, 0.0f, 1.0f); 
        vs->_onLayoutChanged();  // Immediately update layout
      })
    .def_property("split_color", 
      [](verticalsplit_ptr_t vs) -> ork::fvec3 { return vs->_split_color; },
      [](verticalsplit_ptr_t vs, const ork::fvec3& color) { vs->_split_color = color; })
    .def("setTop", [](verticalsplit_ptr_t vs, widget_ptr_t widget) {
      vs->setTop(widget, vs); // Pass the container itself as parent
    })
    .def("setBottom", [](verticalsplit_ptr_t vs, widget_ptr_t widget) {
      vs->setBottom(widget, vs); // Pass the container itself as parent
    })
    .def("__repr__", [](verticalsplit_ptr_t vs) -> std::string {
      return FormatString("VerticalSplit(split_pos=%.2f)", vs->_split_position);
    });
  type_codec->registerStdCodec<verticalsplit_ptr_t>(verticalsplit_type);

  /////////////////////////////////////////////////////////////////////////////////
  // TabGroup Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto tabgroup_type = py::class_<TabGroup, Group, tabgroup_ptr_t>(ncui_module, "TabGroup")
    .def(py::init<>())
    .def("addTab", 
      [](tabgroup_ptr_t tg, const std::string& name, widget_ptr_t content) {
        tg->addTab(name, content, tg);
      })
    .def("addTab", 
      [](tabgroup_ptr_t tg, const std::string& name, widget_ptr_t content, const ork::fvec3& color) {
        tg->addTab(name, content, tg, color);
      })
    .def("switchToTab", &TabGroup::switchToTab)
    .def("getActiveContent", &TabGroup::getActiveContent)
    .def("clearContentArea", &TabGroup::clearContentArea)
    .def_property("active_tab", 
      [](tabgroup_ptr_t tg) -> std::string { return tg->_active_tab; },
      [](tabgroup_ptr_t tg, const std::string& name) { tg->switchToTab(name); })
    .def_property_readonly("tabs", 
      [](tabgroup_ptr_t tg) -> py::list {
        py::list result;
        for (const auto& tab : tg->_tabs) {
          py::dict tab_info;
          tab_info["name"] = tab->_name;
          tab_info["color"] = tab->_color;
          result.append(tab_info);
        }
        return result;
      })
    .def("__repr__", [](tabgroup_ptr_t tg) -> std::string {
      return FormatString("TabGroup(%zu tabs, active='%s')", tg->_tabs.size(), tg->_active_tab.c_str());
    });
  type_codec->registerStdCodec<tabgroup_ptr_t>(tabgroup_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Button Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto button_type = py::class_<Button, Widget, button_ptr_t>(ncui_module, "Button")
    .def(py::init<>())
    .def_property("text", 
      [](button_ptr_t btn) -> std::string { return btn->_text; },
      [](button_ptr_t btn, const std::string& text) { btn->_text = text; })
    .def_property("enabled", 
      [](button_ptr_t btn) -> bool { return btn->_enabled; },
      [](button_ptr_t btn, bool enabled) { btn->_enabled = enabled; })
    .def_property("normal_bg_color", 
      [](button_ptr_t btn) -> ork::fvec3 { return btn->_normal_bg_color; },
      [](button_ptr_t btn, const ork::fvec3& color) { btn->_normal_bg_color = color; })
    .def_property("normal_fg_color", 
      [](button_ptr_t btn) -> ork::fvec3 { return btn->_normal_fg_color; },
      [](button_ptr_t btn, const ork::fvec3& color) { btn->_normal_fg_color = color; })
    .def_property("hover_bg_color", 
      [](button_ptr_t btn) -> ork::fvec3 { return btn->_hover_bg_color; },
      [](button_ptr_t btn, const ork::fvec3& color) { btn->_hover_bg_color = color; })
    .def_property("hover_fg_color", 
      [](button_ptr_t btn) -> ork::fvec3 { return btn->_hover_fg_color; },
      [](button_ptr_t btn, const ork::fvec3& color) { btn->_hover_fg_color = color; })
    .def_property("pressed_bg_color", 
      [](button_ptr_t btn) -> ork::fvec3 { return btn->_pressed_bg_color; },
      [](button_ptr_t btn, const ork::fvec3& color) { btn->_pressed_bg_color = color; })
    .def_property("pressed_fg_color", 
      [](button_ptr_t btn) -> ork::fvec3 { return btn->_pressed_fg_color; },
      [](button_ptr_t btn, const ork::fvec3& color) { btn->_pressed_fg_color = color; })
    .def_property("disabled_bg_color", 
      [](button_ptr_t btn) -> ork::fvec3 { return btn->_disabled_bg_color; },
      [](button_ptr_t btn, const ork::fvec3& color) { btn->_disabled_bg_color = color; })
    .def_property("disabled_fg_color", 
      [](button_ptr_t btn) -> ork::fvec3 { return btn->_disabled_fg_color; },
      [](button_ptr_t btn, const ork::fvec3& color) { btn->_disabled_fg_color = color; })
    .def_property("halign", 
      [](button_ptr_t btn) -> crcstring_ptr_t {
        return std::make_shared<CrcString>(static_cast<uint64_t>(btn->_halign));
      },
      [](button_ptr_t btn, crcstring_ptr_t align) {
        btn->_halign = HorizontalAlign(align->hashed());
      })
    .def_property("valign",
      [](button_ptr_t btn) -> crcstring_ptr_t {
        return std::make_shared<CrcString>(static_cast<uint64_t>(btn->_valign));
      },
      [](button_ptr_t btn, crcstring_ptr_t align) {
        btn->_valign = VerticalAlign(align->hashed());
      })
    .def_property("on_click", 
      [](button_ptr_t btn) -> py::object {
        return py::none();
      },
      [](button_ptr_t btn, py::object callback) {
        if (callback.is_none()) {
          btn->_python_callback = svar64_t();
        } else {
          btn->_python_callback.makeShared<py::function>(callback);
          btn->_on_click = [btn](){
            py::gil_scoped_acquire gil;
            auto cb = btn->_python_callback.getShared<py::function>();
            (*cb)();
          };
        }
      })
    .def("__repr__", [](button_ptr_t btn) -> std::string {
      return FormatString("Button('%s', enabled=%s)", btn->_text.c_str(), btn->_enabled ? "true" : "false");
    });
  type_codec->registerStdCodec<button_ptr_t>(button_type);

  /////////////////////////////////////////////////////////////////////////////////
  // ComboBox Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto combobox_type = py::class_<ComboBox, Widget, combobox_ptr_t>(ncui_module, "ComboBox")
    .def(py::init<>())
    .def_property("items", 
      [](combobox_ptr_t cb) -> std::vector<std::string> { return cb->_items; },
      [](combobox_ptr_t cb, const std::vector<std::string>& items) { cb->_items = items; })
    .def_property("selected_index", 
      [](combobox_ptr_t cb) -> int { return cb->_selected_index; },
      [](combobox_ptr_t cb, int index) { 
        if (index >= -1 && index < (int)cb->_items.size()) {
          cb->_selected_index = index;
        }
      })
    .def_property_readonly("selected_item", 
      [](combobox_ptr_t cb) -> py::object {
        if (cb->_selected_index >= 0 && cb->_selected_index < (int)cb->_items.size()) {
          return py::cast(cb->_items[cb->_selected_index]);
        }
        return py::none();
      })
    .def_property("bg_color", 
      [](combobox_ptr_t cb) -> ork::fvec3 { return cb->_bg_color; },
      [](combobox_ptr_t cb, const ork::fvec3& color) { cb->_bg_color = color; })
    .def_property("fg_color", 
      [](combobox_ptr_t cb) -> ork::fvec3 { return cb->_fg_color; },
      [](combobox_ptr_t cb, const ork::fvec3& color) { cb->_fg_color = color; })
    .def_property("selected_bg_color", 
      [](combobox_ptr_t cb) -> ork::fvec3 { return cb->_selected_bg_color; },
      [](combobox_ptr_t cb, const ork::fvec3& color) { cb->_selected_bg_color = color; })
    .def_property("selected_fg_color", 
      [](combobox_ptr_t cb) -> ork::fvec3 { return cb->_selected_fg_color; },
      [](combobox_ptr_t cb, const ork::fvec3& color) { cb->_selected_fg_color = color; })
    .def_property("dropdown_bg_color", 
      [](combobox_ptr_t cb) -> ork::fvec3 { return cb->_dropdown_bg_color; },
      [](combobox_ptr_t cb, const ork::fvec3& color) { cb->_dropdown_bg_color = color; })
    .def_property("dropdown_fg_color", 
      [](combobox_ptr_t cb) -> ork::fvec3 { return cb->_dropdown_fg_color; },
      [](combobox_ptr_t cb, const ork::fvec3& color) { cb->_dropdown_fg_color = color; })
    .def_property("hover_bg_color", 
      [](combobox_ptr_t cb) -> ork::fvec3 { return cb->_hover_bg_color; },
      [](combobox_ptr_t cb, const ork::fvec3& color) { cb->_hover_bg_color = color; })
    .def_property("hover_fg_color", 
      [](combobox_ptr_t cb) -> ork::fvec3 { return cb->_hover_fg_color; },
      [](combobox_ptr_t cb, const ork::fvec3& color) { cb->_hover_fg_color = color; })
    .def_property("max_visible_items", 
      [](combobox_ptr_t cb) -> int { return cb->_max_visible_items; },
      [](combobox_ptr_t cb, int max_items) { 
        if (max_items > 0) {
          cb->_max_visible_items = max_items;
        }
      })
    .def_property_readonly("is_open", 
      [](combobox_ptr_t cb) -> bool { return cb->_is_open; })
    .def_property("on_selection_changed", 
      [](combobox_ptr_t cb) -> py::object {
        if (auto py_callback = cb->_python_callback.tryAs<py::function>()) {
          return py_callback.value();
        }
        return py::none();
      },
      [](combobox_ptr_t cb, py::object callback) {
        if (callback.is_none()) {
          cb->_python_callback = svar64_t();
        } else {
          cb->_python_callback = py::cast<py::function>(callback);
        }
      })
    .def("addItem", 
      [](combobox_ptr_t cb, const std::string& item) {
        cb->_items.push_back(item);
      })
    .def("removeItem", 
      [](combobox_ptr_t cb, int index) {
        if (index >= 0 && index < (int)cb->_items.size()) {
          cb->_items.erase(cb->_items.begin() + index);
          if (cb->_selected_index == index) {
            cb->_selected_index = -1;
          } else if (cb->_selected_index > index) {
            cb->_selected_index--;
          }
        }
      })
    .def("clearItems", 
      [](combobox_ptr_t cb) {
        cb->_items.clear();
        cb->_selected_index = -1;
      })
    .def("openDropdown", 
      [](combobox_ptr_t cb) {
        cb->_openDropdown();
      })
    .def("closeDropdown", 
      [](combobox_ptr_t cb) {
        cb->_closeDropdown();
      })
    .def("__repr__", [](combobox_ptr_t cb) -> std::string {
      return FormatString("ComboBox(%zu items, selected=%d)", cb->_items.size(), cb->_selected_index);
    });
  type_codec->registerStdCodec<combobox_ptr_t>(combobox_type);

  /////////////////////////////////////////////////////////////////////////////////
  // OPQVisualizer Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto opqvisualizer_type = py::class_<OPQVisualizerWidget, Widget, opqviz_ptr_t>(ncui_module, "OPQVisualizer")
    .def(py::init<>())
    .def("setTargetOPQ", [](opqviz_ptr_t viz, opq::opq_ptr_t opq) {
      viz->setTargetOPQ(opq);
    })
    .def("setUpdateInterval", [](opqviz_ptr_t viz, float interval) {
      viz->setUpdateInterval(interval);
    })
    .def_property("border_color",
      [](opqviz_ptr_t viz) -> ork::fvec3 { return viz->_border_color; },
      [](opqviz_ptr_t viz, const ork::fvec3& color) { viz->_border_color = color; })
    .def_property("value_color",
      [](opqviz_ptr_t viz) -> ork::fvec3 { return viz->_value_color; },
      [](opqviz_ptr_t viz, const ork::fvec3& color) { viz->_value_color = color; })
    .def_property("update_interval",
      [](opqviz_ptr_t viz) -> float { return viz->_perf_update_interval; },
      [](opqviz_ptr_t viz, float interval) { viz->setUpdateInterval(interval); });
  type_codec->registerStdCodec<opqviz_ptr_t>(opqvisualizer_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Performance Data Source
  /////////////////////////////////////////////////////////////////////////////////
  auto perfdatasource_type = py::class_<PerformanceDataSource, perfdatasource_ptr_t>(ncui_module, "PerformanceDataSource")
    .def(py::init<>())
    .def_property("name", 
      [](perfdatasource_ptr_t ds) -> std::string { return ds->name; },
      [](perfdatasource_ptr_t ds, const std::string& name) { ds->name = name; })
    .def("addDoubleItem", 
      [](perfdatasource_ptr_t ds, const std::string& key, const std::string& display_name, 
         const std::string& units, py::object provider, bool sparkline) {
        auto item = PerformanceDataSource::makeDoubleItem(display_name, units, 
          [provider]() -> double {
            py::gil_scoped_acquire gil;
            return py::cast<double>(provider());
          }, sparkline);
        ds->addItem(key, item);
      }, py::arg("key"), py::arg("display_name"), py::arg("units"), py::arg("provider"), py::arg("sparkline") = true)
    .def("addIntItem", 
      [](perfdatasource_ptr_t ds, const std::string& key, const std::string& display_name, 
         const std::string& units, py::object provider, bool sparkline) {
        auto item = PerformanceDataSource::makeIntItem(display_name, units, 
          [provider]() -> int {
            py::gil_scoped_acquire gil;
            return py::cast<int>(provider());
          }, sparkline);
        ds->addItem(key, item);
      }, py::arg("key"), py::arg("display_name"), py::arg("units"), py::arg("provider"), py::arg("sparkline") = true)
    .def("addStringItem", 
      [](perfdatasource_ptr_t ds, const std::string& key, const std::string& display_name, 
         py::object provider) {
        auto item = PerformanceDataSource::makeStringItem(display_name, 
          [provider]() -> std::string {
            py::gil_scoped_acquire gil;
            return py::cast<std::string>(provider());
          });
        ds->addItem(key, item);
      })
    .def("updateAll", &PerformanceDataSource::updateAll)
    .def_property_readonly("item_count", 
      [](perfdatasource_ptr_t ds) -> size_t { return ds->_items_by_order.size(); })
    .def("__repr__", [](perfdatasource_ptr_t ds) -> std::string {
      return FormatString("PerformanceDataSource('%s', %zu items)", ds->name.c_str(), ds->_items_by_order.size());
    });
  type_codec->registerStdCodec<perfdatasource_ptr_t>(perfdatasource_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Performance Visualizer Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto perfviz_type = py::class_<PerformanceVisualizerWidget, Widget, perfviz_ptr_t>(ncui_module, "PerformanceVisualizer")
    .def(py::init<>())
    .def_property("data_source", 
      [](perfviz_ptr_t viz) -> perfdatasource_ptr_t { return viz->data_source; },
      [](perfviz_ptr_t viz, perfdatasource_ptr_t ds) { viz->data_source = ds; })
    .def_property("title", 
      [](perfviz_ptr_t viz) -> std::string { return viz->title; },
      [](perfviz_ptr_t viz, const std::string& title) { viz->title = title; })
    .def_property("update_interval", 
      [](perfviz_ptr_t viz) -> float { return viz->update_interval; },
      [](perfviz_ptr_t viz, float interval) { viz->update_interval = interval; })
    .def_property("sparkline_width", 
      [](perfviz_ptr_t viz) -> int { return viz->sparkline_width; },
      [](perfviz_ptr_t viz, int width) { viz->sparkline_width = width; })
    .def_property("border_color", 
      [](perfviz_ptr_t viz) -> ork::fvec3 { return viz->border_color; },
      [](perfviz_ptr_t viz, const ork::fvec3& color) { viz->border_color = color; })
    .def_property("background_color", 
      [](perfviz_ptr_t viz) -> ork::fvec3 { return viz->background_color; },
      [](perfviz_ptr_t viz, const ork::fvec3& color) { viz->background_color = color; })
    .def_property("title_color", 
      [](perfviz_ptr_t viz) -> ork::fvec3 { return viz->title_color; },
      [](perfviz_ptr_t viz, const ork::fvec3& color) { viz->title_color = color; })
    .def_property("text_color", 
      [](perfviz_ptr_t viz) -> ork::fvec3 { return viz->text_color; },
      [](perfviz_ptr_t viz, const ork::fvec3& color) { viz->text_color = color; })
    .def_property("value_color", 
      [](perfviz_ptr_t viz) -> ork::fvec3 { return viz->value_color; },
      [](perfviz_ptr_t viz, const ork::fvec3& color) { viz->value_color = color; })
    .def("__repr__", [](perfviz_ptr_t viz) -> std::string {
      return FormatString("PerformanceVisualizer('%s', %dx%d)", 
        viz->title.c_str(), viz->_width, viz->_height);
    });
  type_codec->registerStdCodec<perfviz_ptr_t>(perfviz_type);

  /////////////////////////////////////////////////////////////////////////////////
  // TextEdit Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto textedit_type = py::class_<TextEdit, Widget, textedit_ptr_t>(ncui_module, "TextEdit")
    .def(py::init<>())
    .def_property("text", 
      [](textedit_ptr_t edit) -> std::string { return edit->_text; },
      [](textedit_ptr_t edit, const std::string& text) { //
        edit->_text = text; //
        edit->_cursor_pos = text.length(); //
     })
    .def_property("cursor_pos", 
      [](textedit_ptr_t edit) -> size_t { return edit->_cursor_pos; },
      [](textedit_ptr_t edit, size_t pos) { edit->_cursor_pos = std::min(pos, edit->_text.length()); })
    .def_property("has_focus", 
      [](textedit_ptr_t edit) -> bool { return edit->_has_focus; },
      [](textedit_ptr_t edit, bool focus) { edit->_has_focus = focus; })
    .def_property("is_valid", 
      [](textedit_ptr_t edit) -> bool { return edit->_is_valid; },
      [](textedit_ptr_t edit, bool valid) { edit->_is_valid = valid; })
    .def("__repr__", [](textedit_ptr_t edit) -> std::string {
      return FormatString("TextEdit('%s', cursor=%zu)", edit->_text.c_str(), edit->_cursor_pos);
    });
  type_codec->registerStdCodec<textedit_ptr_t>(textedit_type);

  /////////////////////////////////////////////////////////////////////////////////
  // IntEdit Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto intedit_type = py::class_<IntEdit, TextEdit, intedit_ptr_t>(ncui_module, "IntEdit")
    .def(py::init<>())
    .def("getValue", &IntEdit::getValue)
    .def("setValue", &IntEdit::setValue)
    .def_property("min_value", 
      [](intedit_ptr_t edit) -> int { return edit->_min_value; },
      [](intedit_ptr_t edit, int min_val) { edit->_min_value = min_val; })
    .def_property("max_value", 
      [](intedit_ptr_t edit) -> int { return edit->_max_value; },
      [](intedit_ptr_t edit, int max_val) { edit->_max_value = max_val; })
    .def("__repr__", [](intedit_ptr_t edit) -> std::string {
      return FormatString("IntEdit(value=%d, range=[%d,%d])", edit->getValue(), edit->_min_value, edit->_max_value);
    });
  type_codec->registerStdCodec<intedit_ptr_t>(intedit_type);

  /////////////////////////////////////////////////////////////////////////////////
  // FloatEdit Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto floatedit_type = py::class_<FloatEdit, TextEdit, floatedit_ptr_t>(ncui_module, "FloatEdit")
    .def(py::init<>())
    .def("getValue", &FloatEdit::getValue)
    .def("setValue", &FloatEdit::setValue)
    .def_property("min_value", 
      [](floatedit_ptr_t edit) -> float { return edit->_min_value; },
      [](floatedit_ptr_t edit, float min_val) { edit->_min_value = min_val; })
    .def_property("max_value", 
      [](floatedit_ptr_t edit) -> float { return edit->_max_value; },
      [](floatedit_ptr_t edit, float max_val) { edit->_max_value = max_val; })
    .def_property("decimal_places", 
      [](floatedit_ptr_t edit) -> int { return edit->_decimal_places; },
      [](floatedit_ptr_t edit, int places) { edit->_decimal_places = places; })
    .def("__repr__", [](floatedit_ptr_t edit) -> std::string {
      return FormatString("FloatEdit(value=%.2f, range=[%.2f,%.2f])", edit->getValue(), edit->_min_value, edit->_max_value);
    });
  type_codec->registerStdCodec<floatedit_ptr_t>(floatedit_type);

  /////////////////////////////////////////////////////////////////////////////////
  // BoolEdit Widget
  /////////////////////////////////////////////////////////////////////////////////
  auto booledit_type = py::class_<BoolEdit, Widget, booledit_ptr_t>(ncui_module, "BoolEdit")
    .def(py::init<>())
    .def_property("value", 
      [](booledit_ptr_t edit) -> bool { return edit->_value; },
      [](booledit_ptr_t edit, bool value) { edit->_value = value; })
    .def_property("label", 
      [](booledit_ptr_t edit) -> std::string { return edit->_label; },
      [](booledit_ptr_t edit, const std::string& label) { edit->_label = label; })
    .def_property("has_focus", 
      [](booledit_ptr_t edit) -> bool { return edit->_has_focus; },
      [](booledit_ptr_t edit, bool focus) { edit->_has_focus = focus; })
    .def("toggle", [](booledit_ptr_t edit) { edit->_toggle(); })
    .def("__repr__", [](booledit_ptr_t edit) -> std::string {
      return FormatString("BoolEdit('%s', value=%s)", edit->_label.c_str(), edit->_value ? "True" : "False");
    });
  type_codec->registerStdCodec<booledit_ptr_t>(booledit_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Module-level functions
  /////////////////////////////////////////////////////////////////////////////////
  ncui_module.def("context", &context, py::return_value_policy::reference);
  
  /////////////////////////////////////////////////////////////////////////////////
  // Cleanup utilities
  /////////////////////////////////////////////////////////////////////////////////
  ncui_module.def("setupCleanup", []() {
    // Python-callable function to ensure proper NotCurses cleanup
    static bool setup_done = false;
    if (!setup_done) {
      setup_done = true;
      // This could set up atexit handlers or other cleanup mechanisms
      // specific to Python usage
    }
  });
}

} // namespace ork 