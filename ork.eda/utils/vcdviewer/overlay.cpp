////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "vcdviewer.h"

///////////////////////////////////////////////////////////////////////////////
overlay_ptr_t Overlay::instance() {
  static overlay_ptr_t __overlay = std::make_shared<Overlay>("yo", fvec4(0));
  return __overlay;
}
///////////////////////////////////////////////////////////////////////////////
Overlay::Overlay(
    const std::string& name, //
    fvec4 color)
    : Widget(name)
    , _color(color)
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
    scale_matrix.setScale(matrix_xscale, matrix_yscale, 1.0f);
    trans_matrix.translate(matrix_xoffset, matrix_yoffset, 0.0f);
    return fmtx4::multiply_ltor(scale_matrix,trans_matrix);
  };
  ////////////////////////////////
  if (_vbdirty) {
    _vbdirty = false;
    VtxWriter<vtx_t> vw;
    vw.Lock(
        tgt, //
        &_vtxbuf,
        4 + viewparams->_sigtracks.size() * 2);
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
    for (auto track : viewparams->_sigtracks) {
      int y1 = track->_geometry._y;
      int y2 = track->_geometry.y2();
      add_vtx(track->_nearest_timestep, y1, 0xff404040);
      add_vtx(track->_nearest_timestep, y2, 0xff404040);
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

    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
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

    auto temp_rstate = std::make_shared<lev2::SRasterState>();
    temp_rstate->setBlendingMacro(lev2::BlendingMacro::ADDITIVE);
    temp_rstate->setDepthTest(lev2::EDepthTest::OFF);
    auto save_rstate = defmtl->swapRasterState(temp_rstate);

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

    ork::lev2::FontMan::PushFont(_font);
    lev2::FontMan::beginTextBlock(tgt, 1024);
    tgt->PushModColor(_textcolor);
    for (auto track : viewparams->_sigtracks) {
      if (track->_stringwidth) {
        auto& label = track->_label;
        lev2::FontMan::DrawText(
            tgt, //
            ixc - (track->_stringwidth >> 1),
            track->_labelY,
            label.c_str());
        //
      }
    }
    tgt->PopModColor();
    lev2::FontMan::endTextBlock(tgt);
    ork::lev2::FontMan::PopFont();
  }
  mtxi->PopUIMatrix();
}
