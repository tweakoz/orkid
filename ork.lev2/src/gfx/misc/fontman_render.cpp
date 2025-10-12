////////////////////////////////////////////////////////////////
// Copyright 2007, Michael T. Mayers, all rights reserved.
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

void FontMan::beginTextBlock(Context* pTARG, int imaxcharcount) {
  instance()->_beginTextBlock(pTARG, imaxcharcount);
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::endTextBlock(Context* pTARG) {
  instance()->_endTextBlock(pTARG);
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::_beginTextBlockWithState(Context* pTARG, textblockstate_ptr_t tbstate) {
  _textBlockStateStack.push(_currentTextBlockState);
  _currentTextBlockState = tbstate;
  _beginTextBlock(pTARG, tbstate->_maxcharcount);
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::_endTextBlockWithState(Context* pTARG, textblockstate_ptr_t tbstate) {

  OrkAssert(_currentTextBlockState == tbstate);

  _endTextBlock(pTARG);

  auto top = _textBlockStateStack.top();
  _textBlockStateStack.pop();
  _currentTextBlockState = top;
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::_beginTextBlock(Context* pTARG, int imaxcharcount) {
  _gpuInit(pTARG);

  if (0 == imaxcharcount)
    imaxcharcount = 2048;
  int inumv = imaxcharcount * 8;

  VertexBufferBase& VB = pTARG->IMI()->RefTextVB();
  OrkAssert(false == VB.IsLocked());
  vtxwriter_t& vw = mTextWriter;
  new (&vw) vtxwriter_t;
  vw.miWriteCounter = 0;
  vw.miWriteBase    = 0;
  int inuminvb      = VB.GetNumVertices();
  int imaxinvb      = VB.GetMax();
  // printf( "inuminvb<%d> imaxinvb<%d> inumv<%d>\n", inuminvb, imaxinvb, inumv );
  if ((inuminvb + inumv) >= imaxinvb) {
    VB.Reset();
    VB.SetNumVertices(0);
  }
  vw.Lock(pTARG, &pTARG->IMI()->RefTextVB(), inumv);
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::_endTextBlock(Context* context) {
  OrkAssert(context->IMI()->RefTextVB().IsLocked());
  mTextWriter.UnLock(context);
  bool bdraw      = mTextWriter.miWriteCounter != 0;
  auto RCFD       = context->topRenderContextFrameData();
  const auto& CPD = RCFD->topCPD();
  auto stereocams = CPD._stereo_cam_matrices;
  auto GBI        = context->GBI();
  //auto RSI        = context->RSI();
  auto the_font   = currentFont();
  auto top_state = _currentTextBlockState;
  if (bdraw) {
    if (stereocams) {
      rcid_ptr_t RCID;
      if( top_state ){
        RCID = top_state->_overrideRCID;
      }
      if( nullptr == RCID ){
        RCID = std::make_shared<RenderContextInstData>(RCFD);
        RCID->_genMatrix = [this]() -> fmtx4 {
          fmtx4 text_world;
          text_world.setScale(1.1f);
          return text_world;
        };
      }
      auto font_material = the_font->_fs_material;
      auto RSTATE       = font_material->_rasterstate;
      the_font->_pipe_stereo->wrappedDrawCall(*RCID, [&]() { //
        RSTATE->setCullTest(ECullTest::OFF);
        RSTATE->setDepthTest(EDepthTest::OFF);
        RSTATE->setBlendingMacro(top_state->_blending);
        context->FXI()->applyRasterState(*RSTATE);
        GBI->DrawPrimitiveEML(mTextWriter, PrimitiveType::TRIANGLES);
      });
    } else {
      auto material = the_font->material();
      auto RSTATE   = material->_rasterstate;
      the_font->_materialDeferred->_variant = "font"_crcu;
      material->BeginBlock(context);
      RSTATE->setCullTest(ECullTest::OFF);
      RSTATE->setDepthTest(EDepthTest::OFF);
      RSTATE->setBlendingMacro(top_state->_blending);
      context->FXI()->applyRasterState(*RSTATE);
      GBI->DrawPrimitiveEML(mTextWriter, PrimitiveType::TRIANGLES);
      material->EndBlock(context);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::_enqueueText( float fx, float fy,        //
                            vtxwriter_t& vwriter,      //
                            const fixedstring_t& text, //
                            const fvec4& color) {      //
                              
  ///////////////////////////////////
  auto the_font         = currentFont();
  size_t iLen           = text.length();
  const FontDesc& fdesc = the_font->description();

  int iSX     = fdesc.miAdvanceWidth;
  int iSY     = fdesc.miAdvanceHeight;
  int ishifty = fdesc.miYShift;

  uint32_t ucolor = color.BGRAU32();

  ///////////////////////////////////
  int iRow = 0;
  int iCol = 0;

  for (size_t i = 0; i < iLen; i++) {
    char ch = text[i];

    switch (ch) {
      case 0x0a: // linefeed
      {
        iRow++;
        iCol = 0;
        break;
      }
      case 0x20: // space
      {
        iCol++;
        break;
      }
      default: {
        int iCharRow = -1;
        int iCharCol = -1;

        const CharDesc& desc = mCharDescriptions[int(ch)];

        if (desc.ch != 0) {
          iCharRow = desc.miRow;
          iCharCol = desc.miCol;
        }

        if ((iCharRow >= 0) && (iCharCol >= 0)) {
          float fx0 = fx + float(iCol * iSX);
          float fy0 = fy + float(iRow * iSY) + float(ishifty);
          the_font->enqueueCharacter(vwriter, fx0, fy0, iCharCol, iCharRow, ucolor);
          iCol++;
        }

        break;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::DrawTextItems(Context* context, const textitem_vect& items) {

  auto fontman = instance();

  fontman->_gpuInit(context);

  auto mtxi             = context->MTXI();
  auto gbi              = context->GBI();
  auto imi              = context->IMI();
  auto fxi              = context->FXI();
  auto font             = currentFont();
  auto material         = font->material();
  const FontDesc& fdesc = font->description();
  int iSX               = fdesc.miAdvanceWidth;
  int iSY               = fdesc.miAdvanceHeight;
  int ishifty           = fdesc.miYShift;

  size_t char_count = 0;
  for (const auto& item : items) {
    char_count += item._text.length();
  }

  /////////////////////////////////////
  // begin block
  /////////////////////////////////////

  VertexBufferBase& VB = imi->RefTextVB();
  OrkAssert(false == VB.IsLocked());
  VB.Reset();

  /////////////////////////////////////
  // ensure that we have enough vertex writers
  /////////////////////////////////////

  size_t num_items = items.size();

  while (fontman->_writers.size() < num_items) {
    vtxwriter_ptr_t vw = std::make_shared<vtxwriter_t>();
    fontman->_writers.push_back(vw);
  }

  /////////////////////////////////////
  fvec4 color = context->RefModColor();

  for (size_t i = 0; i < items.size(); i++) {
    vtxwriter_ptr_t vw = fontman->_writers[i];
    vw->miWriteCounter = 0;
    vw->miWriteBase    = 0;
    //
    const auto& item   = items[i];
    const auto& string = item._text;
    size_t slen        = string.length();
    int inumv          = slen * 8;
    //
    vw->Lock(context, &VB, inumv);

    float fx = (slen * iSX);
    float fy = (iSY + ishifty);

    fontman->_enqueueText(-(fx * 0.5), -(fy * 0.5), *vw, string, color);
    vw->UnLock(context);
  }

  /////////////////////////////////////
  // end block
  /////////////////////////////////////

  font->_materialDeferred->_variant = "font-instanced"_crcu;

  int inumpasses = material->BeginBlock(context);

  fmtx4 matscale;
  matscale.setScale(0.25f);

  for (size_t i = 0; i < items.size(); i++) {
    const auto& item    = items[i];
    auto vw             = fontman->_writers[i];
    const auto& wmatrix = item._wmatrix;
    mtxi->PushMMatrix(fmtx4::multiply_ltor(matscale, wmatrix));
    material->UpdateMMatrix(context);
    fxi->applyRasterState(*(material->_rasterstate));
    gbi->DrawPrimitiveEML(*vw, ork::lev2::PrimitiveType::TRIANGLES);
    mtxi->PopMMatrix();
  }

  material->EndBlock(context);
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::DrawText(Context* context, int iX, int iY, const char* pFmt, ...) {
  fixedstring_t fxs;
  va_list argp;
  va_start(argp, pFmt);
  vsnprintf(fxs.mutable_c_str(), KFIXEDSTRINGLEN, pFmt, argp);
  va_end(argp);
  fxs.recalclen();
  fvec4 color = context->RefModColor();

  instance()->_enqueueText(iX, iY, instance()->mTextWriter, fxs, color);
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::DrawCenteredText(Context* context, int iY, const char* pFmt, ...) {
  fixedstring_t fxs;
  va_list argp;
  va_start(argp, pFmt);
  vsnprintf(fxs.mutable_c_str(), KFIXEDSTRINGLEN, pFmt, argp);
  va_end(argp);
  fxs.recalclen();
  auto font        = currentFont();
  const auto& desc = *(font->_fontdesc);
  int string_width = desc.stringWidth(fxs.length());
  int TARGW        = context->mainSurfaceWidth();
  int center_x     = (TARGW >> 1) - (string_width >> 1);
  fvec4 color      = context->RefModColor();
  instance()->_enqueueText(center_x, iY, instance()->mTextWriter, fxs, color);
}

///////////////////////////////////////////////////////////////////////////////

void Font::enqueueCharacter( FontMan::vtxwriter_t& vw, //
                             float fx, float fy,       //
                             int cell_X, int cell_Y,   //
                             U32 ucolor) const {       //

  auto fontman   = FontMan::instance();
  auto topstate  = fontman->_topstate();
  bool is_stereo = topstate->_stereo_3d_text;

  ///////////////////////////////////////////////
  // calc vertex pos - ensure pixel alignment
  ///////////////////////////////////////////////

  // Round to nearest pixel for crisp rendering
  float fix1       = std::floor(fx + 0.5f);
  float fiy1       = std::floor(fy + 0.5f);
  const float fix2 = fix1 + (is_stereo ? _fontdesc->_3d_char_width : _fontdesc->miCharWidth);
  const float fiy2 = fiy1 + (is_stereo ? _fontdesc->_3d_char_height : _fontdesc->miCharHeight);

  ///////////////////////////////////////////////
  // calc UVs for 1:1 texel mapping
  ///////////////////////////////////////////////

  // Calculate exact texel coordinates
  float texel_u1 = float(cell_X * _fontdesc->miCellWidth) +
                   (is_stereo ? _fontdesc->_3d_char_u_offset : _fontdesc->miCharOffsetX);
  float texel_v1 = float(cell_Y * _fontdesc->miCellHeight) +
                   (is_stereo ? _fontdesc->_3d_char_v_offset : _fontdesc->miCharOffsetY);
  float texel_u2 = texel_u1 + (is_stereo ? _fontdesc->_3d_char_u_width : _fontdesc->miCharWidth);
  float texel_v2 = texel_v1 + (is_stereo ? _fontdesc->_3d_char_v_height : _fontdesc->miCharHeight);

  ///////////////////////////////////////////////
  // unitize UV's for texture sampling
  // Use exact texture size (not size-1) for proper 1:1 mapping
  ///////////////////////////////////////////////

  float tex_width = float(_fontdesc->miTexWidth);
  float tex_height = float(_fontdesc->miTexHeight);

  // Direct texel-to-UV mapping for 1:1 correspondence
  // No half-texel offset with point filtering for exact texel sampling
  float fu1 = texel_u1 / tex_width;
  float fv1 = texel_v1 / tex_height;
  float fu2 = texel_u2 / tex_width;
  float fv2 = texel_v2 / tex_height;

  ///////////////////////////////////////////////
  // in 3d, flip V
  ///////////////////////////////////////////////

  if (is_stereo)
    std::swap(fv1, fv2);

  ///////////////////////////////////////////////
  // write vertices
  ///////////////////////////////////////////////

  vw.AddVertex(TEXT_VTXFMT(fix1, fiy1, 0.0f, fu1, fv1, ucolor));
  vw.AddVertex(TEXT_VTXFMT(fix2, fiy1, 0.0f, fu2, fv1, ucolor));
  vw.AddVertex(TEXT_VTXFMT(fix2, fiy2, 0.0f, fu2, fv2, ucolor));
  vw.AddVertex(TEXT_VTXFMT(fix1, fiy1, 0.0f, fu1, fv1, ucolor));
  vw.AddVertex(TEXT_VTXFMT(fix2, fiy2, 0.0f, fu2, fv2, ucolor));
  vw.AddVertex(TEXT_VTXFMT(fix1, fiy2, 0.0f, fu1, fv2, ucolor));

  ///////////////////////////////////////////////
  if (0)
    printf(
        "queuechar font<%s> fx<%g> fy<%g> cell_X<%d> cell_Y<%d> fix1<%g> fiy1<%g> fix2<%g> fiy2<%g> fu1<%g> fu2<%g>\n",
        msFontName.c_str(),
        fx,
        fy,
        cell_X,
        cell_Y,
        fix1,
        fiy1,
        fix2,
        fiy2,
        fu1,
        fu2);
}

} // namespace ork { namespace lev2 {