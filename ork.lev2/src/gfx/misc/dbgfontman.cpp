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

namespace ork { namespace lev2 {

const int Font::kMaxChars = 16384;

fontman_ptr_t FontMan::instance() {
  struct PublicFontMan : public FontMan {
    PublicFontMan() {
      //printf("PublicFontMan instantiated<%p>...\n", this);
    }
  };
  static std::shared_ptr<PublicFontMan> _instance = std::make_shared<PublicFontMan>();
  // printf("FontMan::instance<%p>\n", _instance.get());
  return _instance;
}
FontMan& FontMan::GetRef() {
  return *instance();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FontMan::FontMan() {

  for (int ich = 0; ich <= 255; ich++) {
    CharDesc& desc = mCharDescriptions[ich];
    desc.miRow     = ich >> 4;
    desc.miCol     = ich % 16;
    desc.ch        = (char)ich;
  }

  auto tbstate           = std::make_shared<TextBlockState>();
  _defaultTextBlockState = tbstate;
  _textBlockStateStack.push(tbstate);
  _currentTextBlockState = tbstate;


  FontDesc Inconsolata12;
  Inconsolata12.mFontName       = "i12";
  Inconsolata12.mFontFile       = "lev2://textures/Inconsolata12";
  Inconsolata12.miTexWidth      = 512;
  Inconsolata12.miTexHeight     = 512;
  Inconsolata12.miCellWidth     = (512 / 16);
  Inconsolata12.miCellHeight    = (512 / 16);
  Inconsolata12.miCharWidth     = 12;
  Inconsolata12.miCharHeight    = 12;
  Inconsolata12.miCharOffsetX   = 12;
  Inconsolata12.miCharOffsetY   = 12;
  Inconsolata12.miAdvanceWidth  = 6;
  Inconsolata12.miAdvanceHeight = 12;

  /////////////////////////////////////////////////////////////

  FontDesc Inconsolata13;
  Inconsolata13.mFontName       = "i13";
  Inconsolata13.mFontFile       = "lev2://textures/Inconsolata13";
  Inconsolata13.miTexWidth      = 512;
  Inconsolata13.miTexHeight     = 512;
  Inconsolata13.miCellWidth     = (512 / 16);
  Inconsolata13.miCellHeight    = (512 / 16);
  Inconsolata13.miCharWidth     = 13;
  Inconsolata13.miCharHeight    = 13;
  Inconsolata13.miCharOffsetX   = 11;
  Inconsolata13.miCharOffsetY   = 11;
  Inconsolata13.miAdvanceWidth  = 7;
  Inconsolata13.miAdvanceHeight = 13;
  Inconsolata13.miYShift        = -1;

  /////////////////////////////////////////////////////////////

  FontDesc Inconsolata14;
  Inconsolata14.mFontName       = "i14";
  Inconsolata14.mFontFile       = "lev2://textures/Inconsolata14";
  Inconsolata14.miTexWidth      = 512;
  Inconsolata14.miTexHeight     = 512;
  Inconsolata14.miCellWidth     = (512 / 16);
  Inconsolata14.miCellHeight    = (512 / 16);
  Inconsolata14.miCharWidth     = 14;
  Inconsolata14.miCharHeight    = 14;
  Inconsolata14.miCharOffsetX   = 12;
  Inconsolata14.miCharOffsetY   = 8;
  Inconsolata14.miYShift        = -1;
  Inconsolata14.miAdvanceWidth  = 7;
  Inconsolata14.miAdvanceHeight = 12;
  //
  Inconsolata14._3d_char_width    = 7;
  Inconsolata14._3d_char_height   = 8;
  Inconsolata14._3d_char_u_offset = 12;
  Inconsolata14._3d_char_v_offset = 9;
  Inconsolata14._3d_char_u_width  = 8;
  Inconsolata14._3d_char_v_height = 12;

  /////////////////////////////////////////////////////////////

  FontDesc Inconsolata16;
  Inconsolata16.mFontName       = "i16";
  Inconsolata16.mFontFile       = "lev2://textures/Inconsolata16";
  Inconsolata16.miTexWidth      = 512;
  Inconsolata16.miTexHeight     = 512;
  Inconsolata16.miCellWidth     = (512 / 16);
  Inconsolata16.miCellHeight    = (512 / 16);
  Inconsolata16.miCharWidth     = 16;
  Inconsolata16.miCharHeight    = 16;
  Inconsolata16.miCharOffsetX   = 11;
  Inconsolata16.miCharOffsetY   = 11;
  Inconsolata16.miYShift        = 1;
  Inconsolata16.miAdvanceWidth  = 8;
  Inconsolata16.miAdvanceHeight = 12;

  /////////////////////////////////////////////////////////////

  FontDesc Inconsolata17;
  Inconsolata17.mFontName       = "i17";
  Inconsolata17.mFontFile       = "lev2://textures/Inconsolata17";
  Inconsolata17.miTexWidth      = 608;
  Inconsolata17.miTexHeight     = 608;
  Inconsolata17.miCellWidth     = 38;
  Inconsolata17.miCellHeight    = 38;
  Inconsolata17.miCharWidth     = 17;
  Inconsolata17.miCharHeight    = 17;
  Inconsolata17.miCharOffsetX   = 15;
  Inconsolata17.miCharOffsetY   = 14;
  Inconsolata17.miYShift        = 0;
  Inconsolata17.miAdvanceWidth  = 8;
  Inconsolata17.miAdvanceHeight = 17;

  /////////////////////////////////////////////////////////////

  FontDesc Inconsolata24;
  Inconsolata24.mFontName       = "i24";
  Inconsolata24.mFontFile       = "lev2://textures/Inconsolata24";
  Inconsolata24.miTexWidth      = 512;
  Inconsolata24.miTexHeight     = 512;
  Inconsolata24.miCellWidth     = (512 / 16);
  Inconsolata24.miCellHeight    = (512 / 16);
  Inconsolata24.miCharWidth     = 24;
  Inconsolata24.miCharHeight    = 24;
  Inconsolata24.miCharOffsetX   = 9;
  Inconsolata24.miCharOffsetY   = 3;
  Inconsolata24.miYShift        = 1;
  Inconsolata24.miAdvanceWidth  = 11;
  Inconsolata24.miAdvanceHeight = 24;

  /////////////////////////////////////////////////////////////

  FontDesc Inconsolata32;
  Inconsolata32.mFontName       = "i32";
  Inconsolata32.mFontFile       = "lev2://textures/Inconsolata32";
  Inconsolata32.miTexWidth      = 800;
  Inconsolata32.miTexHeight     = 800;
  Inconsolata32.miCellWidth     = (800 / 16);
  Inconsolata32.miCellHeight    = (800 / 16);
  Inconsolata32.miCharWidth     = 32;
  Inconsolata32.miCharHeight    = 32;
  Inconsolata32.miCharOffsetX   = 0;
  Inconsolata32.miCharOffsetY   = 0;
  Inconsolata32.miYShift        = -4;
  Inconsolata32.miAdvanceWidth  = 16;
  Inconsolata32.miAdvanceHeight = 24;

  /////////////////////////////////////////////////////////////

  FontDesc Inconsolata48;
  Inconsolata48.mFontName       = "i48";
  Inconsolata48.mFontFile       = "lev2://textures/Inconsolata48";
  Inconsolata48.miTexWidth      = 800;
  Inconsolata48.miTexHeight     = 800;
  Inconsolata48.miCellWidth     = (800 / 16);
  Inconsolata48.miCellHeight    = (800 / 16);
  Inconsolata48.miCharWidth     = 48;
  Inconsolata48.miCharHeight    = 48;
  Inconsolata48.miCharOffsetX   = 0;
  Inconsolata48.miCharOffsetY   = 0;
  Inconsolata48.miYShift        = -7;
  Inconsolata48.miAdvanceWidth  = 24;
  Inconsolata48.miAdvanceHeight = 40;
  //
  Inconsolata48._3d_char_width    = 48;
  Inconsolata48._3d_char_height   = 48;
  Inconsolata48._3d_char_u_offset = 1;
  Inconsolata48._3d_char_v_offset = 6;
  Inconsolata48._3d_char_u_width  = 48;
  Inconsolata48._3d_char_v_height = 48;

  /////////////////////////////////////////////////////////////

  FontDesc Transponder24;
  Transponder24.mFontName       = "d24";
  Transponder24.mFontFile       = "lev2://textures/transponder24";
  Transponder24.miTexWidth      = 512;
  Transponder24.miTexHeight     = 512;
  Transponder24.miCellWidth     = (512 / 16);
  Transponder24.miCellHeight    = (512 / 16);
  Transponder24.miCharWidth     = 24;
  Transponder24.miCharHeight    = 24;
  Transponder24.miCharOffsetX   = 9;
  Transponder24.miCharOffsetY   = 7;
  Transponder24.miAdvanceWidth  = 16;
  Transponder24.miAdvanceHeight = 24;

  _addFont(Inconsolata12);
  _addFont(Inconsolata13);
  _addFont(Inconsolata14);
  _addFont(Inconsolata16);
  _addFont(Inconsolata17);
  _addFont(Inconsolata24);
  _addFont(Inconsolata32);
  _addFont(Inconsolata48);
  _addFont(Transponder24);

}

///////////////////////////////////////////////////////////////////////////////

FontMan::~FontMan() {
  _fontmap.clear();
}

///////////////////////////////////////////////////////////////////////////////

int FontMan::stringWidth(int numchars) {
  auto font = currentFont();
  return font->mFontDesc.stringWidth(numchars);
}

///////////////////////////////////////////////////////////////////////////////

int FontDesc::stringWidth(int numchars) const {
  return miAdvanceWidth * numchars;
}

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
  const FontDesc& fdesc = the_font->GetFontDesc();

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
  const FontDesc& fdesc = font->GetFontDesc();
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
  const auto& desc = font->mFontDesc;
  int string_width = desc.stringWidth(fxs.length());
  int TARGW        = context->mainSurfaceWidth();
  int center_x     = (TARGW >> 1) - (string_width >> 1);
  fvec4 color      = context->RefModColor();
  instance()->_enqueueText(center_x, iY, instance()->mTextWriter, fxs, color);
}

///////////////////////////////////////////////////////////////////////////////
// Font Management
///////////////////////////////////////////////////////////////////////////////

void FontMan::_addFont(const FontDesc& fdesc) {
  auto it = _fontmap.find(fdesc.mFontName);
  if (it == _fontmap.end()) {
    auto new_font = std::make_shared<Font>(fdesc.mFontName, fdesc.mFontFile);
    _fontvect.push_back(new_font);
    _fontmap[fdesc.mFontName] = new_font;
    new_font->mFontDesc = fdesc;
  }
}

///////////////////////////////////////////////////////////////////////////////

font_ptr_t FontMan::currentFont() {
  auto top_state = instance()->_currentTextBlockState;
  return top_state->_font;
}

///////////////////////////////////////////////////////////////////////////////

font_ptr_t FontMan::fontForId(const std::string& name) {
  auto fontman = instance();
  auto it = fontman->_fontmap.find(name);
  if( it == fontman->_fontmap.end() ){
    orkprintf( "FontMan::fontForId<%s> not found\n", name.c_str() );
    return nullptr;
  }
  return it->second;
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::setCurrentFont(const std::string& name) {
  auto fontman      = instance();
  font_ptr_t font = fontForId(name);
  auto top_state   = fontman->_currentTextBlockState;
  top_state->_font = font;
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::PushFont(font_ptr_t font) {
  OrkAssert(font);
  auto top_state = instance()->_currentTextBlockState;
  top_state->_fontstack.push(top_state->_font);
  top_state->_font = font;
}

///////////////////////////////////////////////////////////////////////////////

font_ptr_t FontMan::PushFont(const std::string& name) {
  return instance()->_pushFont(name);
}

///////////////////////////////////////////////////////////////////////////////

font_ptr_t FontMan::PopFont() {
  return instance()->_popFont();
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::_bindFont(font_ptr_t pFont) {
  OrkAssert(pFont);
  _currentTextBlockState->_font = pFont;
}

///////////////////////////////////////////////////////////////////////////////

font_ptr_t FontMan::_pushFont(const std::string& name) {
  auto the_font = fontForId(name);
  if (the_font == nullptr) {
    printf("FontMan::_pushFont<%s> not found\n", name.c_str());
  }
  OrkAssert(the_font);
  auto top_state    = _currentTextBlockState;
  auto current_font = top_state->_font;
  top_state->_fontstack.push(current_font);
  top_state->_font = the_font;
  return the_font;
}

///////////////////////////////////////////////////////////////////////////////

font_ptr_t FontMan::_popFont() {
  auto top_state = _currentTextBlockState;
  top_state->_fontstack.pop();
  auto next_font = top_state->_fontstack.top();
  OrkAssert(next_font);
  top_state->_font = next_font;
  return next_font;
}

///////////////////////////////////////////////////////////////////////////////

textblockstate_ptr_t FontMan::_topstate() {
  return _currentTextBlockState;
}

///////////////////////////////////////////////////////////////////////////////

Font::Font(const std::string& fontname, const std::string& filename)
    : msFileName(filename)
    , msFontName(fontname) {
}

int Font::stringWidth(int numchars) const {
  return mFontDesc.stringWidth(numchars);
}

int Font::stringHeight(int numlines) const {
  return numlines * mFontDesc.miAdvanceHeight;
}
int Font::charHeight() const {
  return mFontDesc.miCharHeight;
}
int Font::centerY(int c) const {
  return c - (mFontDesc.miCharHeight >> 1);
}

///////////////////////////////////////////////////////////////////////////////

void Font::enqueueCharacter( FontMan::vtxwriter_t& vw, //
                             float fx, float fy,       //
                             int cell_X, int cell_Y,   //
                             U32 ucolor) const {       //

  auto fontman   = FontMan::instance();
  auto topstate  = fontman->_topstate();
  bool is_stereo = topstate->_stereo_3d_text;
  const auto& fontdesc = mFontDesc;

  ///////////////////////////////////////////////
  // calc vertex pos - ensure pixel alignment
  ///////////////////////////////////////////////

  // Round to nearest pixel for crisp rendering
  float fix1       = std::floor(fx + 0.5f);
  float fiy1       = std::floor(fy + 0.5f);
  const float fix2 = fix1 + (is_stereo ? fontdesc._3d_char_width : fontdesc.miCharWidth);
  const float fiy2 = fiy1 + (is_stereo ? fontdesc._3d_char_height : fontdesc.miCharHeight);

  ///////////////////////////////////////////////
  // calc UVs for 1:1 texel mapping
  ///////////////////////////////////////////////

  // Calculate exact texel coordinates
  float texel_u1 = float(cell_X * fontdesc.miCellWidth) +
                   (is_stereo ? fontdesc._3d_char_u_offset : fontdesc.miCharOffsetX);
  float texel_v1 = float(cell_Y * fontdesc.miCellHeight) +
                   (is_stereo ? fontdesc._3d_char_v_offset : fontdesc.miCharOffsetY);
  float texel_u2 = texel_u1 + (is_stereo ? fontdesc._3d_char_u_width : fontdesc.miCharWidth);
  float texel_v2 = texel_v1 + (is_stereo ? fontdesc._3d_char_v_height : fontdesc.miCharHeight);

  ///////////////////////////////////////////////
  // unitize UV's for texture sampling
  // Use exact texture size (not size-1) for proper 1:1 mapping
  ///////////////////////////////////////////////

  float tex_width = float(fontdesc.miTexWidth);
  float tex_height = float(fontdesc.miTexHeight);

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

///////////////////////////////////////////////////////////////////////////////

#if defined(__APPLE__)
extern bool _macosUseHIDPI;
#endif

GfxMaterial* Font::material() const {
  return _use_deferred ? _materialDeferred.get() : mpMaterial;
}
const FontDesc& Font::description() const {
  return mFontDesc;
}
const FontDesc& Font::GetFontDesc() const {
  return description();
}

void Font::LoadFromDisk(Context* context, const FontDesc& fdesc) {

  printf("Font<%s> LoadFromDisk<%s>\n", msFontName.c_str(), msFileName.c_str());
  auto FXI = context->FXI();

  //////////////////////////////////////////////////////////////////////
  AssetPath apath(msFileName.c_str());
  auto txi                                                 = context->TXI();
  _texture                                                 = std::make_shared<Texture>();
  _texture->_vars->makeValueForKey<bool>("loadimmediate") = true;
  txi->LoadTexture(apath, _texture);
  //////////////////////////////////////////////////////////////////////

  mpMaterial = new GfxMaterialUIText;
  mpMaterial->gpuInit(context);

  _materialDeferred = std::make_shared<PBRMaterial>();

  //////////////////////////////////////////////////////////////////////
  // stereo mode
  //////////////////////////////////////////////////////////////////////

  _fs_material = std::make_shared<FreestyleMaterial>();
  _fs_material->gpuInit(context, "orkshader://ui");
  _tek_stereo_text = _fs_material->technique("uitext_stereo");
  FxPipelinePermutation permu;
  permu._forced_technique = _tek_stereo_text;
  auto fxcache            = _fs_material->pipelineCache();
  OrkAssert(fxcache);
  _pipe_stereo = fxcache->findPipeline(permu);
  OrkAssert(_pipe_stereo);
  auto paramMVPL     = _fs_material->param("mvp_l");
  auto paramMVPR     = _fs_material->param("mvp_r");
  auto paramColorMap = _fs_material->param("ColorMap");
  auto paramModColor = _fs_material->param("ModColor");
  _pipe_stereo->bindParam(paramMVPL, "RCFD_Camera_MVP_Left"_crcsh);
  _pipe_stereo->bindParam(paramMVPR, "RCFD_Camera_MVP_Right"_crcsh);
  _pipe_stereo->bindParam(paramColorMap, _texture);
  _pipe_stereo->bindParam(paramModColor, "RCFD_MODCOLOR"_crcsh);

  //////////////////////////////////////////////////////////////////////
  // mono mode
  //////////////////////////////////////////////////////////////////////

  mpMaterial->SetTexture(ETEXDEST_DIFFUSE, _texture.get());
  _texture->TexSamplingMode().presetPointAndClamp();
  //context->TXI()->ApplySamplingMode(_texture.get());

  //_materialDeferred->_asset_texcolor = asset::AssetManager<lev2::TextureAsset>::load(apath);
  _materialDeferred->_texColor = _texture; //_materialDeferred->_asset_texcolor.GetTexture();
  //_materialDeferred->gpuInit(context);

#if defined(__APPLE__)
  /*if (_macosUseHIDPI) {
    mFontDesc.miCharWidth *= 2;
    mFontDesc.miCharHeight *= 2;
    mFontDesc.miCharOffsetX *= 2;
    mFontDesc.miCharOffsetY *= 2;
    mFontDesc.miYShift *= 2;
    mFontDesc.miAdvanceWidth *= 2;
    mFontDesc.miAdvanceHeight *= 2;
  }*/
#endif
}

// Inconsolata font from http://www.levien.com/type/myfonts/inconsolata.html
// font textures built with F2IBuilder http://sourceforge.net/projects/f2ibuilder/
//  set texture size to 512
//  use metrics off
//  use a monospace font
//  show grid should be fine so long as the font stays away from the cell boundaries
//  a 'cell' is one of the 256(16x16) tiles in the texture,
//   if the texture size is 512x512, then a single cell will be 32x32

void FontMan::gpuInit(Context* pTARG) {
  instance()->_gpuInit(pTARG);
}

void FontMan::_gpuInit(Context* pTARG) {

  /////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////

  if (_doGpuInit) {
    pTARG->makeCurrentContext();
    //pTARG->debugPushGroup("FontMan::InitFonts");

    for( auto font : _fontvect ){
      font->LoadFromDisk(pTARG, font->mFontDesc);
      _bindFont(font);
    }

    _defaultTextBlockState->_font = _pushFont("i14");

    //pTARG->debugPopGroup();
    _doGpuInit = false;
  }
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
