#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/ui/label.h>

namespace ork::ui {
///////////////////////////////////////////////////////////////////////////////
Label::Label(
    const std::string& name, //
    fvec4 color,
    std::string label)
    : Widget(name)
    , _color(color)
    , _label(label) {
  _textcolor = fvec4(1, 1, 1, 1);
}
///////////////////////////////////////////////////////////////////////////////
void Label::DoDraw(drawevent_constptr_t drwev) {

  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto mtxi   = tgt->MTXI();
  auto& primi = lev2::GfxPrimitives::GetRef();
  auto defmtl = lev2::defaultUIMaterial();

  mtxi->PushUIMatrix();
  {
    int ix1, iy1, ix2, iy2, ixc, iyc;
    LocalToRoot(0, 0, ix1, iy1);
    ix2 = ix1 + _geometry._w;
    iy2 = iy1 + _geometry._h;
    ixc = ix1 + (_geometry._w >> 1);
    iyc = iy1 + (_geometry._h >> 1);

    defmtl->_rasterstate.SetBlending(lev2::EBLENDING_ALPHA);
    defmtl->_rasterstate.SetDepthTest(lev2::EDEPTHTEST_OFF);
    tgt->PushModColor(_color);
    primi.RenderQuadAtZ(
        defmtl.get(),
        tgt,
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

    tgt->PushModColor(_textcolor);
    ork::lev2::FontMan::PushFont("i14");
    lev2::FontMan::beginTextBlock(tgt, 256);
    int sw = lev2::FontMan::stringWidth(_label.length());
    lev2::FontMan::DrawText(
        tgt, //
        ixc - (sw >> 1),
        iyc - 6,
        _label.c_str());
    //
    lev2::FontMan::endTextBlock(tgt);
    ork::lev2::FontMan::PopFont();
    tgt->PopModColor();
  }
  mtxi->PopUIMatrix();
}

} // namespace ork::ui
