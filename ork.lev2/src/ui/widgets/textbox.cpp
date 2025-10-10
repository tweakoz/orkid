#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/ui/textbox.h>

namespace ork::ui {
///////////////////////////////////////////////////////////////////////////////
TextBox::TextBox(
    const std::string& name, //
    fvec4 color,
    std::string text)
    : Widget(name)
    , _color(color){
  setText(text);
  _textcolor = fvec4(1, 1, 1, 1);
  _font = lev2::FontMan::fontForId("i14");
}
///////////////////////////////////////////////////////////////////////////////
void TextBox::setText(std::string txt) {
  _lines = SplitString(txt, '\n');
  _numchars = 0;
  _maxlinelen = 0;
  for(auto line : _lines) {
    size_t linelen = line.length();
    if( linelen>_maxlinelen ) _maxlinelen = linelen;
    _numchars += linelen;

  }
}
///////////////////////////////////////////////////////////////////////////////
void TextBox::DoDraw(drawevent_constptr_t drwev) {

  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto mtxi   = tgt->MTXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  mtxi->PushUIMatrix();
  {
    int ix1, iy1, ix2, iy2, ixc, iyc;
    LocalToRoot(0, 0, ix1, iy1);
    ix2 = ix1 + _geometry._w;
    iy2 = iy1 + _geometry._h;
    ixc = ix1 + (_geometry._w >> 1);
    iyc = iy1 + (_geometry._h >> 1);

    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
    tgt->PushModColor(_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(
        defmtl.get(),
        ix1,  // x0
        ix2,  // x1
        iy1,  // y0
        iy2,  // y1
        0.0f, // z
        0.0f,
        1.0f, // u0, u1
        0.0f,
        1.0f // v0, v1
    );
    tgt->PopModColor();

    ETextAlignH HALIGN = _halign;
    ETextAlignV VALIGN = _valign;
    int LINE_SPACING = 12;

    int numlines = _lines.size();
    tgt->PushModColor(_textcolor);
    ork::lev2::FontMan::PushFont(_font);
    lev2::FontMan::beginTextBlock(tgt, _numchars);
    if (numlines) {
      for( int iline=0; iline<numlines; iline++ ){
        auto line = _lines[iline];
        int linelen = line.length();
        int sw = lev2::FontMan::stringWidth(linelen);
        int sx = ix1;
        int sy = iy1 + (iline * LINE_SPACING);
        switch( HALIGN ) {
          case ETextAlignH::LEFT:
            sx = ix1;
            break;
          case ETextAlignH::CENTER:
            sx = ixc - (sw>>1);
            break;
          case ETextAlignH::CENTER_ALL:{
            sw = lev2::FontMan::stringWidth(_maxlinelen);
            sx = ixc - (sw>>1);
            break;
          }
          case ETextAlignH::RIGHT:
            sx = ix2 - sw;
            break;
          default:
            sx = ix1;
            break;
        }
        if( VALIGN==ETextAlignV::CENTER ) {
          int totalh = numlines*LINE_SPACING;
          int ycenter = iyc - (totalh>>1);
          sy = ycenter + (iline*LINE_SPACING);
        } else if( VALIGN==ETextAlignV::BOTTOM ) {
          int totalh = numlines*LINE_SPACING;
          sy = iy2 - totalh + (iline*LINE_SPACING);
        }
        lev2::FontMan::DrawText(
            tgt, //
            sx,
            sy,
            line.c_str());
      }
    }
    lev2::FontMan::endTextBlock(tgt);
    ork::lev2::FontMan::PopFont();
    tgt->PopModColor();
  }
  mtxi->PopUIMatrix();
}
} // namespace ork::ui
