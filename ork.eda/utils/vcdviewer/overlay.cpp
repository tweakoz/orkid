#include "vcdviewer.h"

///////////////////////////////////////////////////////////////////////////////
overlay_ptr_t Overlay::instance() {
  static overlay_ptr_t __overlay = std::make_shared<Overlay>("yo", fvec4(0), "yo");
  return __overlay;
}
///////////////////////////////////////////////////////////////////////////////
Overlay::Overlay(
    const std::string& name, //
    fvec4 color,
    std::string label)
    : Widget(name)
    , _color(color)
    , _label(label)
    , _vtxbuf(1 << 20, 0, PrimitiveType::NONE) {
  _textcolor = fvec4(1, 1, 1, 1);
  _vtxbuf.SetRingLock(false);
}
///////////////////////////////////////////////////////////////////////////////
void Overlay::DoDraw(drawevent_constptr_t drwev) {

  auto tgt        = drwev->GetTarget();
  auto fbi        = tgt->FBI();
  auto gbi        = tgt->GBI();
  auto mtxi       = tgt->MTXI();
  auto& primi     = lev2::GfxPrimitives::GetRef();
  auto defmtl     = lev2::defaultUIMaterial();
  auto viewparams = ViewParams::instance();
  ////////////////////////////////
  auto genmatrix = [&](float offx, float offy) -> fmtx4 {
    float ftimemin      = float(viewparams->_min_timestamp);
    float ftimemax      = float(viewparams->_max_timestamp);
    float ftimerange    = ftimemax - ftimemin;
    float matrix_xscale = (float(_geometry._w) - offx) / ftimerange;
    // printf("matrix_xscale<%g>\n", matrix_xscale);
    float matrix_xoffset = float(_geometry._x) + offx + 4;
    float matrix_yscale  = 1.0f; // float(_geometry._h);
    float matrix_yoffset = 4.0f; // float(_geometry._y) + offy + 4;
    fmtx4 scale_matrix, trans_matrix;
    scale_matrix.SetScale(matrix_xscale, matrix_yscale, 1.0f);
    trans_matrix.Translate(matrix_xoffset, matrix_yoffset, 0.0f);
    return scale_matrix * trans_matrix;
  };
  ////////////////////////////////
  if (_vbdirty) {
    _vbdirty = false;
    VtxWriter<vtx_t> vw;
    vw.Lock(
        tgt, //
        &_vtxbuf,
        4);
    //////////////////////////////////////////////
    _numvertices = 0;
    //////////////////////////////////////////////
    auto add_vtx = [&](uint64_t timestamp, float value, uint32_t color) {
      float fx = float(timestamp);
      vtx_t vtx;
      vtx._position = fvec3(fx, value, 0.0f);
      vtx._color    = color;
      vw.AddVertex(vtx);
      _numvertices++;
    };
    add_vtx(viewparams->_cursor_actual, 0, 0xff202020);
    add_vtx(viewparams->_cursor_actual, _geometry._h, 0xff202020);
    if (viewparams->_curtrack) {
      int y1 = viewparams->_curtrack->_geometry._y;
      int y2 = viewparams->_curtrack->_geometry.y2();
      add_vtx(viewparams->_cursor_nearest, y1, 0xff00ffff);
      add_vtx(viewparams->_cursor_nearest, y2, 0xff00ffff);
    }
    //////////////////////////////////////////////
    vw.UnLock(tgt);
    _vtxbase = vw.miWriteBase;
  }
  ////////////////////////////////

  mtxi->PushUIMatrix();
  {
    int ix1, iy1, ix2, iy2, ixc, iyc;
    LocalToRoot(0, 0, ix1, iy1);
    ix2 = ix1 + _geometry._w;
    iy2 = iy1 + _geometry._h;
    ixc = ix1 + (_geometry._w >> 1);
    iyc = iy1 + (_geometry._h >> 1);

    defmtl->_rasterstate.SetBlending(lev2::Blending::ALPHA);
    defmtl->_rasterstate.SetDepthTest(lev2::EDEPTHTEST_OFF);
    tgt->PushModColor(_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
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

    /////////////////////////////////
    // line prims
    /////////////////////////////////

    SRasterState rstate;
    rstate.SetBlending(lev2::Blending::ADDITIVE);
    rstate.SetDepthTest(lev2::EDEPTHTEST_OFF);
    auto save_rstate = defmtl->swapRasterState(rstate);

    mtxi->PushMMatrix(genmatrix(192, 0));
    defmtl->SetUIColorMode(UiColorMode::VTX);
    gbi->DrawPrimitive(
        defmtl.get(), //
        _vtxbuf,
        PrimitiveType::LINES,
        _vtxbase,
        _numvertices);
    mtxi->PopMMatrix();
    defmtl->swapRasterState(save_rstate);
    /////////////////////////////////
    // mouselabel
    /////////////////////////////////

    int lablen = _label.length();
    if (lablen) {
      tgt->PushModColor(_textcolor);
      ork::lev2::FontMan::PushFont(_font);
      lev2::FontMan::beginTextBlock(tgt, lablen);
      int sw = lev2::FontMan::stringWidth(lablen);
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
  }
  mtxi->PopUIMatrix();
}
