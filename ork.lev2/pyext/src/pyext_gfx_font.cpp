////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/dbgfontman.h>

/////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
/////////////////////////////////////////////////////////////////////////////////
void pyinit_gfx_font(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  auto fontman_t =
      py::class_<FontMan, fontman_ptr_t>(module_lev2, "FontManager")
          .def_static("gpuInit", [](ctx_t& ctx) { FontMan::gpuInit(ctx.get()); })
          .def_static(
              "beginTextBlock",
              [](ctx_t& ctx, const std::string& fontid, fvec4 color, int uiw, int uih, int maxchars) {
                ctx->MTXI()->PushMMatrix(fmtx4());
                ctx->MTXI()->PushUIMatrix(uiw, uih);
                ctx->PushModColor(color);
                FontMan::PushFont(fontid);
                FontMan::beginTextBlock(ctx.get(), maxchars);
              })
          .def_static(
              "endTextBlock",
              [](ctx_t& ctx) {
                FontMan::endTextBlock(ctx.get());
                FontMan::PopFont();
                ctx->PopModColor();
                ctx->MTXI()->PopMMatrix();
                ctx->MTXI()->PopUIMatrix();
              })
          .def_static("draw", [](ctx_t& ctx, int x, int y, std::string text) { FontMan::DrawText(ctx.get(), x, y, text.c_str()); })
          .def_static("instance", []() -> fontman_ptr_t { return FontMan::instance(); })
          .def_static("fontForId", [](const std::string& name) -> font_ptr_t { return FontMan::instance()->fontForId(name); });
  type_codec->registerStdCodec<fontman_ptr_t>(fontman_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto desc_t = py::class_<FontDesc,fontdesc_ptr_t>(module_lev2, "FontDesc")
                    .def_property(
                        "fontname",
                        [](fontdesc_ptr_t fd) -> std::string { return fd->mFontName; },
                        [](fontdesc_ptr_t fd, const std::string& v) { fd->mFontName = v; })
                    .def_property(
                        "fontfile",
                        [](fontdesc_ptr_t fd) -> std::string { return fd->mFontFile; },
                        [](fontdesc_ptr_t fd, const std::string& v) { fd->mFontFile = v; })
                    .def_property("tex_width", [](fontdesc_ptr_t fd) -> int { return fd->miTexWidth; }, [](fontdesc_ptr_t fd, int v) { fd->miTexWidth = v; })
                    .def_property("tex_height", [](fontdesc_ptr_t fd) -> int { return fd->miTexHeight; }, [](fontdesc_ptr_t fd, int v) { fd->miTexHeight = v; })
                    .def_property("cell_width", [](fontdesc_ptr_t fd) -> int { return fd->miCellWidth; }, [](fontdesc_ptr_t fd, int v) { fd->miCellWidth = v; })
                    .def_property("cell_height", [](fontdesc_ptr_t fd) -> int { return fd->miCellHeight; }, [](fontdesc_ptr_t fd, int v) { fd->miCellHeight = v; })
                    .def_property("char_width", [](fontdesc_ptr_t fd) -> int { return fd->miCharWidth; }, [](fontdesc_ptr_t fd, int v) { fd->miCharWidth = v; })
                    .def_property("char_height", [](fontdesc_ptr_t fd) -> int { return fd->miCharHeight; }, [](fontdesc_ptr_t fd, int v) { fd->miCharHeight = v; })
                    .def_property("char_offset_x", [](fontdesc_ptr_t fd) -> int { return fd->miCharOffsetX; }, [](fontdesc_ptr_t fd, int v) { fd->miCharOffsetX = v; })
                    .def_property("char_offset_y", [](fontdesc_ptr_t fd) -> int { return fd->miCharOffsetY; }, [](fontdesc_ptr_t fd, int v) { fd->miCharOffsetY = v; })
                    .def_property("y_shift", [](fontdesc_ptr_t fd) -> int { return fd->miYShift; }, [](fontdesc_ptr_t fd, int v) { fd->miYShift = v; })
                    .def_property("advance_width", [](fontdesc_ptr_t fd) -> int { return fd->miAdvanceWidth; }, [](fontdesc_ptr_t fd, int v) { fd->miAdvanceWidth = v; })
                    .def_property("advance_height", [](fontdesc_ptr_t fd) -> int { return fd->miAdvanceHeight; }, [](fontdesc_ptr_t fd, int v) { fd->miAdvanceHeight = v; })
                    .def_property("stereo_char_width", [](fontdesc_ptr_t fd) -> int { return fd->_3d_char_width; }, [](fontdesc_ptr_t fd, int v) { fd->_3d_char_width = v; })
                    .def_property("stereo_char_height", [](fontdesc_ptr_t fd) -> int { return fd->_3d_char_height; }, [](fontdesc_ptr_t fd, int v) { fd->_3d_char_height = v; })
                    .def_property("stereo_char_u_offset", [](fontdesc_ptr_t fd) -> int { return fd->_3d_char_u_offset; }, [](fontdesc_ptr_t fd, int v) { fd->_3d_char_u_offset = v; })
                    .def_property("stereo_char_v_offset", [](fontdesc_ptr_t fd) -> int { return fd->_3d_char_v_offset; }, [](fontdesc_ptr_t fd, int v) { fd->_3d_char_v_offset = v; })
                    .def_property("stereo_char_u_width", [](fontdesc_ptr_t fd) -> int { return fd->_3d_char_u_width; }, [](fontdesc_ptr_t fd, int v) { fd->_3d_char_u_width = v; })
                    .def_property("stereo_char_v_height", [](fontdesc_ptr_t fd) -> int { return fd->_3d_char_v_height; }, [](fontdesc_ptr_t fd, int v) { fd->_3d_char_v_height = v; })
                    .def("stringWidth", [](fontdesc_ptr_t fd, int numchars) -> int { return fd->stringWidth(numchars); })
                    .def("stringHeight", [](fontdesc_ptr_t fd, int numlines) -> int { return fd->stringHeight(numlines); });
  type_codec->registerStdCodec<fontdesc_ptr_t>(desc_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto font_t = py::class_<Font, font_ptr_t>(module_lev2, "Font")
                    .def(
                        "__repr__",
                        [](font_ptr_t font) -> std::string {
                          fxstring<256> fxs;
                          fxs.format("Font(\"%s\")", font->msFontName.c_str());
                          return fxs.c_str();
                        })
                    .def("bind", [](font_ptr_t font) { return FontMan::GetRef()._bindFont(font); })
                    .def_property_readonly("filename", [](font_ptr_t font) -> std::string { return font->msFileName; })
                    .def_property_readonly("fontname", [](font_ptr_t font) -> std::string { return font->msFontName; })
                    .def_property("description", [](font_ptr_t font) -> fontdesc_ptr_t { return font->_fontdesc; },
                                  [](font_ptr_t font, fontdesc_ptr_t desc) { font->_fontdesc = desc; })
                    .def_static("createFromDescription", [](ctx_t& ctx, fontdesc_ptr_t desc) -> font_ptr_t { //
                      auto font = std::make_shared<Font>();
                      font->load(ctx.get(), desc);
                      return font;
                    });
  type_codec->registerStdCodec<font_ptr_t>(font_t);

} // void pyinit_gfx_font(py::module& module_lev2) {

/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
/////////////////////////////////////////////////////////////////////////////////
