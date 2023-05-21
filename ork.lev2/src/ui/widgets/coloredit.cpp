#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/ui/coloredit.h>
#include <ork/lev2/gfx/material_freestyle.h>

namespace ork::ui {
///////////////////////////////////////////////////////////////////////////////
ColorEdit::ColorEdit(
    const std::string& name, //
    fvec4 color,
    int x,
    int y,
    int w,
    int h)
    : Widget(name, x, y, w, h)
    , _originalColor(color)
    , _currentColor(color) {
  auto hsv = _currentColor.xyz().convertRgbToHsv();
  _currentColorFullBright.setHSV(hsv.x, hsv.y, 1.0);
  _hue = hsv.x;
  _saturation = hsv.y;
  _intensity = hsv.z;
}
///////////////////////////////////////////////////////////////////////////////
HandlerResult ColorEdit::DoOnUiEvent(event_constptr_t cev) {
  HandlerResult rval;
  switch (cev->_eventcode) {
    case EventCode::KEY_DOWN: {
      int key = cev->miKeyCode;
      printf("key<%d>\n", key);
      switch (key) {
        case 256: // esc
          _currentColor         = _originalColor;
          rval._widget_finished = true;
          break;
        case 257: // enter
          rval._widget_finished = true;
          break;
        default:
          break;
      }
      rval.setHandled(this);
      break;
    }
    case EventCode::DOUBLECLICK: {
      float fx     = float(cev->miX) - float(_geometry._w >> 1);
      float fy     = float(cev->miY) - float(_geometry._h >> 1);
      auto pos    = fvec2(fx, fy);
      float radius = pos.length();
      if( radius < _radiusWheelInner ) {
        rval._widget_finished = true;
        rval.setHandled(this);
      }
      else if( radius > _radiusWheelOuter ) {
        _currentColor         = _originalColor;
        rval._widget_finished = true;
        rval.setHandled(this);
      }
      break;
    }
    case EventCode::PUSH: {
      float fx     = float(cev->miX) - float(_geometry._w >> 1);
      float fy     = float(cev->miY) - float(_geometry._h >> 1);
      _push_pos    = fvec2(fx, fy);
      _push_angle  = atan2f(_push_pos.y, _push_pos.x);
      _push_radius = _push_pos.length();
      rval.setHandled(this);
      break;
    }
    case EventCode::RELEASE: {
      rval.setHandled(this);
      break;
    }
    case EventCode::DRAG: {
      float fx        = float(cev->miX) - float(_geometry._w >> 1);
      float fy        = float(cev->miY) - float(_geometry._h >> 1);
      auto cur_pos    = fvec2(fx, fy);
      float cur_angle = atan2f(cur_pos.y, cur_pos.x);
      float radius    = cur_pos.length();
      
      if (_push_radius > _radiusIntensRingI) {
        _intensity = fmod(0.0 + (PI + cur_angle) / PI2, 1.0);
        _currentColor.setHSV(_hue, _saturation, _intensity);
        _currentColorFullBright.setHSV(_hue, _saturation, 1.0);
      } else {
        float range = _radiusWheelOuter - _radiusWheelInner;
        float bias = _radiusWheelInner;
        float SAT = (radius - bias) / range;
        SAT         = std::clamp(SAT, 0.0f, 1.0f);
        _saturation = SAT;
        _hue        = fmod(0.5 + (PI + cur_angle) / PI2, 1.0);
        _currentColor.setHSV(_hue, _saturation, _intensity);
        _currentColorFullBright.setHSV(_hue, _saturation, 1.0);
      }
      rval.setHandled(this);
      break;
    }
    default:
      break;
  }

  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void ColorEdit::DoDraw(drawevent_constptr_t drwev) {

  auto context = drwev->GetTarget();
  auto fbi     = context->FBI();
  auto gbi     = context->GBI();
  auto mtxi    = context->MTXI();
  auto& primi  = lev2::GfxPrimitives::GetRef();
  auto defmtl  = lev2::defaultUIMaterial();
  using vtx_t  = lev2::SVtxV16T16C16;
  auto& VB     = lev2::GfxEnv::GetSharedDynamicV16T16C16();
  lev2::RenderContextFrameData RCFD(context);
  auto uiMatrix = mtxi->uiMatrix(_geometry._w, _geometry._h);

  if (nullptr == _material) {
    _material = std::make_shared<lev2::FreestyleMaterial>();
    _material->gpuInit(context, "orkshader://ui2");
    _tekvtxcolor   = _material->technique("ui_vtxcolor");
    _tekmodcolor   = _material->technique("ui_modcolor");
    _tekcolorwheel = _material->technique("ui_colorwheel");
    _parmvp        = _material->param("mvp");
    _parmodcolor   = _material->param("modcolor");
    _material->dump();
  }

  int ix1, iy1, ix2, iy2, ixc, iyc;
  LocalToRoot(0, 0, ix1, iy1);
  ix2 = ix1 + _geometry._w;
  iy2 = iy1 + _geometry._h;
  ixc = ix1 + (_geometry._w >> 1);
  iyc = iy1 + (_geometry._h >> 1);

  _radiusIntensRingO = (0.5f * float(_geometry._w));
  _radiusIntensRingI = _radiusIntensRingO * 0.8;

  _radiusWheelOuter = _radiusIntensRingI*0.95;
  _radiusWheelInner = _radiusWheelOuter * 0.5;

  _radiusCurrentRingO = _radiusWheelInner * 0.9;
  _radiusCurrentRingI = 0.0;

  /////////////////////////////////////////////////////////////////

  int numquads = 180;
  fvec3 CTR(float(ixc), float(iyc), 0.0f);
  float transparency = 0.0f;

  /////////////////////////////////////////////////////////////////
  // draw intensity ring
  /////////////////////////////////////////////////////////////////

  lev2::VtxWriter<vtx_t> vw0;
  vw0.Lock(context, &VB, 6 * numquads);

  for (int i = 0; i < numquads; i++) {
    float A = DTOR * 360.0 * float(i) / float(numquads);
    float B = DTOR * 360.0 * float(i + 1) / float(numquads);

    fvec3 XYAI(_radiusIntensRingI * cosf(A), _radiusIntensRingI * sinf(A), 0.0f);
    fvec3 XYBI(_radiusIntensRingI * cosf(B), _radiusIntensRingI * sinf(B), 0.0f);
    fvec3 XYAO(_radiusIntensRingO * cosf(A), _radiusIntensRingO * sinf(A), 0.0f);
    fvec3 XYBO(_radiusIntensRingO * cosf(B), _radiusIntensRingO * sinf(B), 0.0f);

    float intensA = fmod(0.5f + float(i) / float(numquads), 1.0f);
    float intensB = fmod(0.5f + float(i + 1) / float(numquads), 1.0f);

    fvec3 ICOLORA = _currentColorFullBright.xyz() * intensA;
    fvec3 ICOLORB = _currentColorFullBright.xyz() * intensB;

    vtx_t v0(CTR + XYAI, fvec4(), ICOLORA);
    vtx_t v1(CTR + XYBI, fvec4(), ICOLORB);
    vtx_t v2(CTR + XYAO, fvec4(), ICOLORA);
    vtx_t v3(CTR + XYBO, fvec4(), ICOLORB);

    vw0.AddVertex(v0);
    vw0.AddVertex(v1);
    vw0.AddVertex(v2);

    vw0.AddVertex(v1);
    vw0.AddVertex(v3);
    vw0.AddVertex(v2);
  }

  vw0.UnLock(context);

  ///////////////////////////////

  _material->_rasterstate.SetRGBAWriteMask(true, true);
  _material->begin(_tekvtxcolor, RCFD);
  _material->bindParamMatrix(_parmvp, uiMatrix);
  gbi->DrawPrimitiveEML(vw0, lev2::PrimitiveType::TRIANGLES);
  _material->end(RCFD);

  /////////////////////////////////////////////////////////////////
  // draw current ring
  /////////////////////////////////////////////////////////////////

  lev2::VtxWriter<vtx_t> vw1;
  vw1.Lock(context, &VB, 6 * numquads);

  for (int i = 0; i < numquads; i++) {
    float A = DTOR * 360.0 * float(i) / float(numquads);
    float B = DTOR * 360.0 * float(i + 1) / float(numquads);

    fvec3 XYAI(_radiusCurrentRingI * cosf(A), _radiusCurrentRingI * sinf(A), 0.0f);
    fvec3 XYBI(_radiusCurrentRingI * cosf(B), _radiusCurrentRingI * sinf(B), 0.0f);
    fvec3 XYAO(_radiusCurrentRingO * cosf(A), _radiusCurrentRingO * sinf(A), 0.0f);
    fvec3 XYBO(_radiusCurrentRingO * cosf(B), _radiusCurrentRingO * sinf(B), 0.0f);

    vtx_t v0(CTR + XYAI, fvec4(), _currentColor);
    vtx_t v1(CTR + XYBI, fvec4(), _currentColor);
    vtx_t v2(CTR + XYAO, fvec4(), _currentColor);
    vtx_t v3(CTR + XYBO, fvec4(), _currentColor);

    vw1.AddVertex(v0);
    vw1.AddVertex(v1);
    vw1.AddVertex(v2);

    vw1.AddVertex(v1);
    vw1.AddVertex(v3);
    vw1.AddVertex(v2);
  }

  vw1.UnLock(context);

  ///////////////////////////////

  _material->_rasterstate.SetRGBAWriteMask(true, true);
  _material->begin(_tekvtxcolor, RCFD);
  _material->bindParamMatrix(_parmvp, uiMatrix);
  gbi->DrawPrimitiveEML(vw1, lev2::PrimitiveType::TRIANGLES);
  _material->end(RCFD);


  /////////////////////////////////////////////////////////////////
  // draw radial color picker
  /////////////////////////////////////////////////////////////////

  lev2::VtxWriter<vtx_t> vw2;
  vw2.Lock(context, &VB, 6 * numquads);

  for (int i = 0; i < numquads; i++) {
    float A = DTOR * 360.0 * float(i) / float(numquads);
    float B = DTOR * 360.0 * float(i + 1) / float(numquads);

    fvec3 XYAI(_radiusWheelInner * cosf(A), _radiusWheelInner * sinf(A), 0.0f);
    fvec3 XYBI(_radiusWheelInner * cosf(B), _radiusWheelInner * sinf(B), 0.0f);
    fvec3 XYAO(_radiusWheelOuter * cosf(A), _radiusWheelOuter * sinf(A), 0.0f);
    fvec3 XYBO(_radiusWheelOuter * cosf(B), _radiusWheelOuter * sinf(B), 0.0f);

    fvec4 RGBAI, RGBAO;

    RGBAI.setHSV(float(i) / float(numquads), 0.0f, _intensity);
    RGBAO.setHSV(float(i + 1) / float(numquads), 1.0f, _intensity);

    vtx_t v0(CTR + XYAI, fvec4(), RGBAI);
    vtx_t v1(CTR + XYBI, fvec4(), RGBAI);
    vtx_t v2(CTR + XYAO, fvec4(), RGBAO);
    vtx_t v3(CTR + XYBO, fvec4(), RGBAO);

    vw2.AddVertex(v0);
    vw2.AddVertex(v1);
    vw2.AddVertex(v2);

    vw2.AddVertex(v1);
    vw2.AddVertex(v3);
    vw2.AddVertex(v2);
  }

  vw2.UnLock(context);

  ///////////////////////////////
  _material->_rasterstate.SetRGBAWriteMask(true, true);
  _material->begin(_tekvtxcolor, RCFD);
  _material->bindParamMatrix(_parmvp, uiMatrix);
  gbi->DrawPrimitiveEML(vw2, lev2::PrimitiveType::TRIANGLES);
  _material->end(RCFD);
  ///////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
