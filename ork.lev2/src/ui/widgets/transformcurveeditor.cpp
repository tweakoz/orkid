////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/ui/transformcurveeditor.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/context.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

static const fvec3 kChannelColors[3] = {
    fvec3(0.9f, 0.3f, 0.3f),  // X = red
    fvec3(0.3f, 0.9f, 0.3f),  // Y = green
    fvec3(0.3f, 0.5f, 0.9f),  // Z = blue
};

static const fvec4 kBgColors[3] = {
    fvec4(0.18f, 0.10f, 0.10f, 1.0f),
    fvec4(0.10f, 0.18f, 0.10f, 1.0f),
    fvec4(0.10f, 0.10f, 0.18f, 1.0f),
};

static const char* kChannelNames[3] = {"X", "Y", "Z"};

///////////////////////////////////////////////////////////////////////////////

TransformCurveEditor::TransformCurveEditor(
    const std::string& name,
    math::transformcurve_ptr_t curve)
    : Widget(name, 0, 0, 0, 0)
    , _curve(curve) {
  // Seed empty curves with two default points so there's something to see
  if (_curve && _curve->numPoints() == 0) {
    auto p0 = std::make_shared<math::TransformCurvePoint>();
    p0->_time = 0.0f;
    p0->_position = fvec3(0, 0, 0);
    _curve->addPoint(p0);
    auto p1 = std::make_shared<math::TransformCurvePoint>();
    p1->_time = 5.0f;
    p1->_position = fvec3(1, 0, 0);
    _curve->addPoint(p1);
  }
  autoFitRanges();
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveEditor::autoFitRanges() {
  if (!_curve || _curve->numPoints() == 0) {
    _timeMin = 0.0f;
    _timeMax = 10.0f;
    _valueMin = -5.0f;
    _valueMax = 5.0f;
    return;
  }

  float tMin = 1e9f, tMax = -1e9f;
  float vMin = 1e9f, vMax = -1e9f;

  for (int i = 0; i < _curve->numPoints(); i++) {
    auto pt = _curve->getPoint(i);
    tMin = std::min(tMin, pt->_time);
    tMax = std::max(tMax, pt->_time);
    vMin = std::min(vMin, std::min({pt->_position.x, pt->_position.y, pt->_position.z}));
    vMax = std::max(vMax, std::max({pt->_position.x, pt->_position.y, pt->_position.z}));
  }

  // Time: 10% margin
  float tMargin = std::max((tMax - tMin) * 0.1f, 0.5f);
  _timeMin = tMin - tMargin;
  _timeMax = tMax + tMargin;

  // Value: 20% margin
  float vMargin = std::max((vMax - vMin) * 0.2f, 1.0f);
  _valueMin = vMin - vMargin;
  _valueMax = vMax + vMargin;
}

///////////////////////////////////////////////////////////////////////////////

TransformCurveEditor::SubplotRect TransformCurveEditor::_subplotRect(int channel) const {
  int rx1, ry1;
  const_cast<TransformCurveEditor*>(this)->LocalToRoot(0, 0, rx1, ry1);

  int total_h = _geometry._h - TOOLBAR_H;
  int subplot_h = total_h / NUM_CHANNELS;
  SubplotRect r;
  r.x = rx1;
  r.y = ry1 + TOOLBAR_H + channel * subplot_h;
  r.w = _geometry._w;
  r.h = subplot_h;
  return r;
}

///////////////////////////////////////////////////////////////////////////////

float TransformCurveEditor::_timeToScreenX(float t, const SubplotRect& r) const {
  return float(r.x) + (t - _timeMin) / (_timeMax - _timeMin) * float(r.w);
}

float TransformCurveEditor::_valueToScreenY(float v, const SubplotRect& r) const {
  return float(r.y + r.h) - (v - _valueMin) / (_valueMax - _valueMin) * float(r.h);
}

float TransformCurveEditor::_screenXToTime(float sx, const SubplotRect& r) const {
  return _timeMin + (sx - float(r.x)) / float(r.w) * (_timeMax - _timeMin);
}

float TransformCurveEditor::_screenYToValue(float sy, const SubplotRect& r) const {
  return _valueMin + (float(r.y + r.h) - sy) / float(r.h) * (_valueMax - _valueMin);
}

///////////////////////////////////////////////////////////////////////////////

int TransformCurveEditor::_hitTestCloseButton(int localX, int localY) const {
  int bx = _geometry._w - TOOLBAR_H;
  if (localX >= bx && localX < _geometry._w && localY >= 0 && localY < TOOLBAR_H)
    return 1;
  return 0;
}

int TransformCurveEditor::_channelForLocalY(int localY) const {
  int adjusted = localY - TOOLBAR_H;
  if (adjusted < 0) return -1;
  int total_h = _geometry._h - TOOLBAR_H;
  int subplot_h = total_h / NUM_CHANNELS;
  int ch = adjusted / subplot_h;
  if (ch >= NUM_CHANNELS) ch = NUM_CHANNELS - 1;
  return ch;
}

int TransformCurveEditor::_hitTestPoint(int localX, int localY, int& outChannel) const {
  if (!_curve) return -1;

  int rx, ry;
  const_cast<TransformCurveEditor*>(this)->LocalToRoot(0, 0, rx, ry);

  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    auto sr = _subplotRect(ch);
    for (int i = 0; i < _curve->numPoints(); i++) {
      auto pt = _curve->getPoint(i);
      float px = _timeToScreenX(pt->_time, sr);
      float vals[3] = {pt->_position.x, pt->_position.y, pt->_position.z};
      float py = _valueToScreenY(vals[ch], sr);

      float dx = float(localX + rx) - px;
      float dy = float(localY + ry) - py;
      if (dx * dx + dy * dy < float(HIT_RADIUS * HIT_RADIUS)) {
        outChannel = ch;
        return i;
      }
    }
  }
  return -1;
}

///////////////////////////////////////////////////////////////////////////////

HandlerResult TransformCurveEditor::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult rval;
  int localX = 0, localY = 0;
  RootToLocal(ev->miX, ev->miY, localX, localY);

  switch (ev->_eventcode) {
    case EventCode::KEY_DOWN: {
      int key = ev->miKeyCode;
      if (key == 256) { // ESC
        if (_onClose) _onClose();
        rval.setHandled(this);
      } else if (key == 261) { // Delete
        if (_selectedPointIndex >= 0 && _curve && _curve->numPoints() > 2) {
          _curve->removePoint(_selectedPointIndex);
          _selectedPointIndex = -1;
          if (_onCurveChanged) _onCurveChanged();
        }
        rval.setHandled(this);
      }
      break;
    }
    case EventCode::PUSH: {
      if (_hitTestCloseButton(localX, localY)) {
        if (_onClose) _onClose();
        rval.setHandled(this);
        break;
      }
      int ch = -1;
      int idx = _hitTestPoint(localX, localY, ch);
      if (idx >= 0) {
        _selectedPointIndex = idx;
        _dragChannel = ch;
        _dragging = true;
        rval.setHandled(this);
      } else {
        _selectedPointIndex = -1;
        rval.setHandled(this);
      }
      break;
    }
    case EventCode::DRAG: {
      if (_dragging && _selectedPointIndex >= 0 && _curve) {
        auto sr = _subplotRect(_dragChannel);
        float newTime = _screenXToTime(float(ev->miX), sr);
        float newValue = _screenYToValue(float(ev->miY), sr);
        newTime = std::clamp(newTime, _timeMin, _timeMax);

        auto pt = _curve->getPoint(_selectedPointIndex);
        pt->_time = newTime;
        switch (_dragChannel) {
          case 0: pt->_position.x = newValue; break;
          case 1: pt->_position.y = newValue; break;
          case 2: pt->_position.z = newValue; break;
        }
        _curve->setPoint(_selectedPointIndex, pt);

        // After setPoint (which re-sorts by time), re-locate point
        for (int i = 0; i < _curve->numPoints(); i++) {
          if (std::abs(_curve->getPoint(i)->_time - newTime) < 1e-6f) {
            _selectedPointIndex = i;
            break;
          }
        }

        if (_onCurveChanged) _onCurveChanged();
        rval.setHandled(this);
      }
      break;
    }
    case EventCode::RELEASE: {
      _dragging = false;
      rval.setHandled(this);
      break;
    }
    case EventCode::DOUBLECLICK: {
      int ch = _channelForLocalY(localY);
      if (ch >= 0 && _curve) {
        auto sr = _subplotRect(ch);
        float t = _screenXToTime(float(ev->miX), sr);
        float v = _screenYToValue(float(ev->miY), sr);

        auto sample = _curve->samplePosition(t);
        auto newPt = std::make_shared<math::TransformCurvePoint>();
        newPt->_time = t;
        newPt->_position = sample;
        switch (ch) {
          case 0: newPt->_position.x = v; break;
          case 1: newPt->_position.y = v; break;
          case 2: newPt->_position.z = v; break;
        }
        int idx = _curve->addPoint(newPt);
        _selectedPointIndex = idx;
        _dragChannel = ch;

        if (_onCurveChanged) _onCurveChanged();
        rval.setHandled(this);
      }
      break;
    }
    default:
      break;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveEditor::_drawToolbar(
    lev2::Context* context, lev2::rcfd_ptr_t RCFD,
    int rx, int ry, const fmtx4& uiMatrix) {
  auto gbi = context->GBI();
  using vtx_t = lev2::SVtxV16T16C16;
  auto vb = lev2::GfxEnv::GetSharedDynamicV16T16C16();

  // Toolbar background
  {
    lev2::VtxWriter<vtx_t> vw;
    vw.Lock(context, vb.get(), 6);
    float x1 = float(rx), x2 = float(rx + _geometry._w);
    float y1 = float(ry), y2 = float(ry + TOOLBAR_H);
    fvec3 c(0.2f, 0.2f, 0.25f);
    vtx_t v0(fvec3(x1, y1, 0), fvec4(), c);
    vtx_t v1(fvec3(x2, y1, 0), fvec4(), c);
    vtx_t v2(fvec3(x1, y2, 0), fvec4(), c);
    vtx_t v3(fvec3(x2, y2, 0), fvec4(), c);
    vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
    vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
    vw.UnLock(context);
    _material->begin(_tekvtxcolor, RCFD);
    _material->bindParamMatrix(_parmvp, uiMatrix);
    gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
    _material->end(RCFD);
  }

  // Close button [X] — red square in top-right
  {
    lev2::VtxWriter<vtx_t> vw;
    vw.Lock(context, vb.get(), 6);
    float bx1 = float(rx + _geometry._w - TOOLBAR_H);
    float bx2 = float(rx + _geometry._w);
    float by1 = float(ry);
    float by2 = float(ry + TOOLBAR_H);
    fvec3 c(0.7f, 0.2f, 0.2f);
    vtx_t v0(fvec3(bx1, by1, 0), fvec4(), c);
    vtx_t v1(fvec3(bx2, by1, 0), fvec4(), c);
    vtx_t v2(fvec3(bx1, by2, 0), fvec4(), c);
    vtx_t v3(fvec3(bx2, by2, 0), fvec4(), c);
    vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
    vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
    vw.UnLock(context);
    _material->begin(_tekvtxcolor, RCFD);
    _material->bindParamMatrix(_parmvp, uiMatrix);
    gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
    _material->end(RCFD);
  }

  // Title text
  lev2::FontMan::PushFont("i14");
  lev2::FontMan::beginTextBlock(context, 256);
  lev2::FontMan::DrawText(context, rx + 8, ry + 4, "TransformCurve Editor");
  lev2::FontMan::DrawText(context, rx + _geometry._w - TOOLBAR_H + 6, ry + 4, "X");
  lev2::FontMan::endTextBlock(context);
  lev2::FontMan::PopFont();
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveEditor::_drawSubplot(
    lev2::Context* context, lev2::rcfd_ptr_t RCFD, int channel,
    int rx, int ry, const fmtx4& uiMatrix) {
  auto gbi = context->GBI();
  using vtx_t = lev2::SVtxV16T16C16;
  auto vb = lev2::GfxEnv::GetSharedDynamicV16T16C16();

  auto sr = _subplotRect(channel);
  const auto& chColor = kChannelColors[channel];
  const auto& bgColor = kBgColors[channel];

  // Background fill
  {
    lev2::VtxWriter<vtx_t> vw;
    vw.Lock(context, vb.get(), 6);
    float x1 = float(sr.x), x2 = float(sr.x + sr.w);
    float y1 = float(sr.y), y2 = float(sr.y + sr.h);
    fvec3 c = bgColor.xyz();
    vtx_t v0(fvec3(x1, y1, 0), fvec4(), c);
    vtx_t v1(fvec3(x2, y1, 0), fvec4(), c);
    vtx_t v2(fvec3(x1, y2, 0), fvec4(), c);
    vtx_t v3(fvec3(x2, y2, 0), fvec4(), c);
    vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
    vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
    vw.UnLock(context);
    _material->begin(_tekvtxcolor, RCFD);
    _material->bindParamMatrix(_parmvp, uiMatrix);
    gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
    _material->end(RCFD);
  }

  // Grid lines
  {
    int numTimeLines = 0;
    float timeStep = (_timeMax - _timeMin) / 10.0f;
    for (float t = ceilf(_timeMin / timeStep) * timeStep; t <= _timeMax; t += timeStep)
      numTimeLines++;

    int numValueLines = 0;
    float valueStep = (_valueMax - _valueMin) / 5.0f;
    for (float v = ceilf(_valueMin / valueStep) * valueStep; v <= _valueMax; v += valueStep)
      numValueLines++;

    int totalLines = numTimeLines + numValueLines;
    if (totalLines > 0) {
      lev2::VtxWriter<vtx_t> vw;
      vw.Lock(context, vb.get(), 6 * totalLines);
      fvec3 gridColor(0.25f, 0.25f, 0.25f);

      for (float t = ceilf(_timeMin / timeStep) * timeStep; t <= _timeMax; t += timeStep) {
        float sx = _timeToScreenX(t, sr);
        vtx_t v0(fvec3(sx, float(sr.y), 0), fvec4(), gridColor);
        vtx_t v1(fvec3(sx + 1.0f, float(sr.y), 0), fvec4(), gridColor);
        vtx_t v2(fvec3(sx, float(sr.y + sr.h), 0), fvec4(), gridColor);
        vtx_t v3(fvec3(sx + 1.0f, float(sr.y + sr.h), 0), fvec4(), gridColor);
        vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
        vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
      }

      for (float v = ceilf(_valueMin / valueStep) * valueStep; v <= _valueMax; v += valueStep) {
        float sy = _valueToScreenY(v, sr);
        vtx_t v0(fvec3(float(sr.x), sy, 0), fvec4(), gridColor);
        vtx_t v1(fvec3(float(sr.x + sr.w), sy, 0), fvec4(), gridColor);
        vtx_t v2(fvec3(float(sr.x), sy + 1.0f, 0), fvec4(), gridColor);
        vtx_t v3(fvec3(float(sr.x + sr.w), sy + 1.0f, 0), fvec4(), gridColor);
        vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
        vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
      }

      vw.UnLock(context);
      _material->begin(_tekvtxcolor, RCFD);
      _material->bindParamMatrix(_parmvp, uiMatrix);
      gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
      _material->end(RCFD);
    }
  }

  // Zero line (if visible)
  if (_valueMin < 0.0f && _valueMax > 0.0f) {
    lev2::VtxWriter<vtx_t> vw;
    vw.Lock(context, vb.get(), 6);
    float sy = _valueToScreenY(0.0f, sr);
    fvec3 zeroColor(0.4f, 0.4f, 0.4f);
    vtx_t v0(fvec3(float(sr.x), sy, 0), fvec4(), zeroColor);
    vtx_t v1(fvec3(float(sr.x + sr.w), sy, 0), fvec4(), zeroColor);
    vtx_t v2(fvec3(float(sr.x), sy + 1.0f, 0), fvec4(), zeroColor);
    vtx_t v3(fvec3(float(sr.x + sr.w), sy + 1.0f, 0), fvec4(), zeroColor);
    vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
    vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
    vw.UnLock(context);
    _material->begin(_tekvtxcolor, RCFD);
    _material->bindParamMatrix(_parmvp, uiMatrix);
    gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
    _material->end(RCFD);
  }

  // Curve tessellation
  if (_curve && _curve->numPoints() >= 2) {
    lev2::VtxWriter<vtx_t> vw;
    vw.Lock(context, vb.get(), 6 * CURVE_SEGMENTS);

    for (int seg = 0; seg < CURVE_SEGMENTS; seg++) {
      float t0 = _timeMin + float(seg) / float(CURVE_SEGMENTS) * (_timeMax - _timeMin);
      float t1 = _timeMin + float(seg + 1) / float(CURVE_SEGMENTS) * (_timeMax - _timeMin);

      auto p0 = _curve->samplePosition(t0);
      auto p1 = _curve->samplePosition(t1);

      float vals0[3] = {p0.x, p0.y, p0.z};
      float vals1[3] = {p1.x, p1.y, p1.z};

      float sx0 = _timeToScreenX(t0, sr);
      float sx1 = _timeToScreenX(t1, sr);
      float sy0 = _valueToScreenY(vals0[channel], sr);
      float sy1 = _valueToScreenY(vals1[channel], sr);

      vtx_t v0(fvec3(sx0, sy0, 0), fvec4(), chColor);
      vtx_t v1(fvec3(sx1, sy1, 0), fvec4(), chColor);
      vtx_t v2(fvec3(sx0, sy0 + 1.0f, 0), fvec4(), chColor);
      vtx_t v3(fvec3(sx1, sy1 + 1.0f, 0), fvec4(), chColor);
      vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
      vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
    }

    vw.UnLock(context);
    _material->begin(_tekvtxcolor, RCFD);
    _material->bindParamMatrix(_parmvp, uiMatrix);
    gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
    _material->end(RCFD);
  }

  // Control points
  if (_curve) {
    int numPts = _curve->numPoints();
    if (numPts > 0) {
      lev2::VtxWriter<vtx_t> vw;
      vw.Lock(context, vb.get(), 6 * numPts);

      for (int i = 0; i < numPts; i++) {
        auto pt = _curve->getPoint(i);
        float vals[3] = {pt->_position.x, pt->_position.y, pt->_position.z};
        float sx = _timeToScreenX(pt->_time, sr);
        float sy = _valueToScreenY(vals[channel], sr);

        fvec3 ptColor = (i == _selectedPointIndex)
                            ? fvec3(1.0f, 1.0f, 0.3f)
                            : chColor;

        float half = float(HIT_RADIUS);
        vtx_t v0(fvec3(sx - half, sy - half, 0), fvec4(), ptColor);
        vtx_t v1(fvec3(sx + half, sy - half, 0), fvec4(), ptColor);
        vtx_t v2(fvec3(sx - half, sy + half, 0), fvec4(), ptColor);
        vtx_t v3(fvec3(sx + half, sy + half, 0), fvec4(), ptColor);
        vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
        vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
      }

      vw.UnLock(context);
      _material->begin(_tekvtxcolor, RCFD);
      _material->bindParamMatrix(_parmvp, uiMatrix);
      gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
      _material->end(RCFD);
    }
  }

  // Subplot border (top separator line)
  {
    lev2::VtxWriter<vtx_t> vw;
    vw.Lock(context, vb.get(), 6);
    fvec3 sepColor(0.35f, 0.35f, 0.4f);
    vtx_t v0(fvec3(float(sr.x), float(sr.y), 0), fvec4(), sepColor);
    vtx_t v1(fvec3(float(sr.x + sr.w), float(sr.y), 0), fvec4(), sepColor);
    vtx_t v2(fvec3(float(sr.x), float(sr.y) + 1.0f, 0), fvec4(), sepColor);
    vtx_t v3(fvec3(float(sr.x + sr.w), float(sr.y) + 1.0f, 0), fvec4(), sepColor);
    vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
    vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
    vw.UnLock(context);
    _material->begin(_tekvtxcolor, RCFD);
    _material->bindParamMatrix(_parmvp, uiMatrix);
    gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
    _material->end(RCFD);
  }

  // Channel label
  lev2::FontMan::PushFont("i14");
  lev2::FontMan::beginTextBlock(context, 16);
  lev2::FontMan::DrawText(context, sr.x + 4, sr.y + 2, kChannelNames[channel]);
  lev2::FontMan::endTextBlock(context);
  lev2::FontMan::PopFont();
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveEditor::DoDraw(drawevent_constptr_t drwev) {
  auto context = drwev->GetTarget();
  auto mtxi = context->MTXI();

  // Lazy-init material
  if (!_material) {
    _material = std::make_shared<lev2::FreestyleMaterial>();
    _material->gpuInit(context, "orkshader://ui2");
    _tekvtxcolor = _material->technique("ui_vtxcolor");
    _parmvp = _material->param("mvp");
  }

  auto RCFD = std::make_shared<lev2::RenderContextFrameData>(context);

  mtxi->PushUIMatrix();
  int vp_w = context->mainSurfaceWidth();
  int vp_h = context->mainSurfaceHeight();
  auto uiMatrix = mtxi->uiMatrix(vp_w, vp_h);

  int rx, ry;
  LocalToRoot(0, 0, rx, ry);

  // Draw outer border / background
  {
    auto gbi = context->GBI();
    using vtx_t = lev2::SVtxV16T16C16;
    auto vb = lev2::GfxEnv::GetSharedDynamicV16T16C16();
    lev2::VtxWriter<vtx_t> vw;
    vw.Lock(context, vb.get(), 6);
    fvec3 borderColor(0.15f, 0.15f, 0.18f);
    float x1 = float(rx), x2 = float(rx + _geometry._w);
    float y1 = float(ry), y2 = float(ry + _geometry._h);
    vtx_t v0(fvec3(x1, y1, 0), fvec4(), borderColor);
    vtx_t v1(fvec3(x2, y1, 0), fvec4(), borderColor);
    vtx_t v2(fvec3(x1, y2, 0), fvec4(), borderColor);
    vtx_t v3(fvec3(x2, y2, 0), fvec4(), borderColor);
    vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
    vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
    vw.UnLock(context);
    _material->begin(_tekvtxcolor, RCFD);
    _material->bindParamMatrix(_parmvp, uiMatrix);
    gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
    _material->end(RCFD);
  }

  _drawToolbar(context, RCFD, rx, ry, uiMatrix);

  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    _drawSubplot(context, RCFD, ch, rx, ry, uiMatrix);
  }

  mtxi->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
