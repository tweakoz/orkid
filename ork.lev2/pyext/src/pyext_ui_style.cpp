////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/style.h>
#include <ork/lev2/ui/context.h>
#include <ork/util/crc.h>

namespace ork::lev2 {

void pyinit_ui_style(py::module& module_ui) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // Style class
  /////////////////////////////////////////////////////////////////////////////////
  auto style_type = //
      py::class_<ui::Style, ui::style_ptr_t>(module_ui, "Style")
          .def(py::init<>())
          .def("clone", &ui::Style::clone)
          // Colors
          .def_property(
              "bg_color",
              [](ui::style_ptr_t style) -> fvec4 { return style->_bg_color; },
              [](ui::style_ptr_t style, fvec4 c) { style->_bg_color = c; })
          .def_property(
              "fg_color",
              [](ui::style_ptr_t style) -> fvec4 { return style->_fg_color; },
              [](ui::style_ptr_t style, fvec4 c) { style->_fg_color = c; })
          .def_property(
              "aux_color1",
              [](ui::style_ptr_t style) -> fvec4 { return style->_aux_color1; },
              [](ui::style_ptr_t style, fvec4 c) { style->_aux_color1 = c; })
          .def_property(
              "aux_color2",
              [](ui::style_ptr_t style) -> fvec4 { return style->_aux_color2; },
              [](ui::style_ptr_t style, fvec4 c) { style->_aux_color2 = c; })
          .def_property(
              "border_color",
              [](ui::style_ptr_t style) -> fvec4 { return style->_border_color; },
              [](ui::style_ptr_t style, fvec4 c) { style->_border_color = c; })
          .def_property(
              "text_color",
              [](ui::style_ptr_t style) -> fvec4 { return style->_text_color; },
              [](ui::style_ptr_t style, fvec4 c) { style->_text_color = c; })
          // Geometry
          .def_property(
              "corner_radius",
              [](ui::style_ptr_t style) -> int { return style->_corner_radius; },
              [](ui::style_ptr_t style, int r) { style->_corner_radius = r; })
          .def_property(
              "border_width",
              [](ui::style_ptr_t style) -> int { return style->_border_width; },
              [](ui::style_ptr_t style, int w) { style->_border_width = w; })
          .def_property(
              "padding",
              [](ui::style_ptr_t style) -> int { return style->_padding; },
              [](ui::style_ptr_t style, int p) { style->_padding = p; })
          // Rendering
          .def_property(
              "blend_mode",
              [](ui::style_ptr_t style) -> crcstring_ptr_t {
                return std::make_shared<CrcString>(uint64_t(style->_blend_mode));
              },
              [](ui::style_ptr_t style, crcstring_ptr_t bm) {
                style->_blend_mode = lev2::BlendingMacro(bm->hashed());
              })
          // Typography
          .def_property(
              "font",
              [](ui::style_ptr_t style) -> font_ptr_t { return style->_font; },
              [](ui::style_ptr_t style, font_ptr_t f) { style->_font = f; });
  type_codec->registerStdCodec<ui::style_ptr_t>(style_type);

  /////////////////////////////////////////////////////////////////////////////////
  // StyleDatabase class
  /////////////////////////////////////////////////////////////////////////////////
  auto styledb_type = //
      py::class_<ui::StyleDatabase, ui::styledatabase_ptr_t>(module_ui, "StyleDatabase")
          .def(py::init<>())
          .def_static(
              "createChild",
              [](ui::styledatabase_ptr_t parent) -> ui::styledatabase_ptr_t { //
                return ui::StyleDatabase::createChild(parent);
              })
          .def(
              "registerStyle",
              [](ui::styledatabase_ptr_t db, crcstring_ptr_t tag, ui::style_ptr_t style) { //
                db->registerStyle(tag->hashed(), style);
              })
          .def(
              "getStyle",
              [](ui::styledatabase_ptr_t db, crcstring_ptr_t tag) -> ui::style_ptr_t { //
                return db->getStyle(tag->hashed());
              });
  type_codec->registerStdCodec<ui::styledatabase_ptr_t>(styledb_type);

  /////////////////////////////////////////////////////////////////////////////////
  // ThemeEngine class
  /////////////////////////////////////////////////////////////////////////////////
  auto themeengine_type = //
      py::class_<ui::ThemeEngine, ui::themeengine_ptr_t>(module_ui, "ThemeEngine")
          .def(py::init<ui::styledatabase_ptr_t>())
          .def(
              "gpuInit",
              [](ui::themeengine_ptr_t engine, ctx_t ctx) { //
                engine->gpuInit(ctx.get());
              })
          .def_property_readonly(
              "styledb",
              [](ui::themeengine_ptr_t engine) -> ui::styledatabase_ptr_t { //
                return engine->_styledb;
              });
  type_codec->registerStdCodec<ui::themeengine_ptr_t>(themeengine_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Helper functions
  /////////////////////////////////////////////////////////////////////////////////
  module_ui.def("createDefaultStyleDatabase", &ui::createDefaultStyleDatabase);
  module_ui.def("createDarkStyleDatabase", &ui::createDarkStyleDatabase);
  module_ui.def("createLightStyleDatabase", &ui::createLightStyleDatabase);
}

} // namespace ork::lev2
