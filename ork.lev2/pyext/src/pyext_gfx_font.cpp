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
          .def_static("draw", [](ctx_t& ctx, int x, int y, std::string text) { FontMan::DrawText(ctx.get(), x, y, text.c_str()); });
  type_codec->registerStdCodec<fontman_ptr_t>(fontman_t);

  /////////////////////////////////////////////////////////////////////////////////
  auto font_t =
      py::class_<Font, font_ptr_t>(module_lev2, "Font")
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
          .def_property_readonly("charWidth", [](font_ptr_t font) -> int { return font->mFontDesc.miCharWidth; })
          .def_property_readonly("charHeight", [](font_ptr_t font) -> int { return font->mFontDesc.miCharHeight; })
          .def_property_readonly("cellWidth", [](font_ptr_t font) -> int { return font->mFontDesc.miCellWidth; })
          .def_property_readonly("cellHeight", [](font_ptr_t font) -> int { return font->mFontDesc.miCellHeight; })
          .def_property_readonly("advanceWidth", [](font_ptr_t font) -> int { return font->mFontDesc.miAdvanceWidth; })
          .def_property_readonly("advanceHeight", [](font_ptr_t font) -> int { return font->mFontDesc.miAdvanceHeight; });
  type_codec->registerStdCodec<font_ptr_t>(font_t);

} // void pyinit_gfx_font(py::module& module_lev2) {

/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
/////////////////////////////////////////////////////////////////////////////////
