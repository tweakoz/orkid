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
// Channel metadata
///////////////////////////////////////////////////////////////////////////////

struct ChannelInfo {
  const char* _label;
  fvec3 _color;
};

static const ChannelInfo kChannelInfo[] = {
    {"Pos X", fvec3(0.9f, 0.2f, 0.2f)},
    {"Pos Y", fvec3(0.2f, 0.9f, 0.2f)},
    {"Pos Z", fvec3(0.3f, 0.5f, 0.9f)},
    {"Rot X", fvec3(0.9f, 0.4f, 0.5f)},
    {"Rot Y", fvec3(0.5f, 0.9f, 0.3f)},
    {"Rot Z", fvec3(0.3f, 0.8f, 0.9f)},
    {"Scale", fvec3(0.8f, 0.8f, 0.8f)},
    {"Scl X", fvec3(0.9f, 0.5f, 0.5f)},
    {"Scl Y", fvec3(0.5f, 0.9f, 0.5f)},
    {"Scl Z", fvec3(0.5f, 0.5f, 0.9f)},
};

///////////////////////////////////////////////////////////////////////////////
// Constructor
///////////////////////////////////////////////////////////////////////////////

TransformCurveEditor::TransformCurveEditor(
    const std::string& name,
    math::transformcurve_ptr_t curve)
    : Widget(name, 0, 0, 0, 0)
    , _curve(curve) {

  // Default visibility: position + uniform scale on
  for (int i = 0; i < CH_COUNT; i++)
    _channelVisible[i] = false;
  _channelVisible[CH_POS_X] = true;
  _channelVisible[CH_POS_Y] = true;
  _channelVisible[CH_POS_Z] = true;
  _channelVisible[CH_SCALE_UNI] = true;

  // Seed empty curves with two default points
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
// Channel data helpers
///////////////////////////////////////////////////////////////////////////////

bool TransformCurveEditor::_isChannelAvailable(int channel) const {
  if (!_curve) return false;
  switch (channel) {
    case CH_SCALE_UNI: return !_curve->_useNonUniformScale;
    case CH_SCALE_X:
    case CH_SCALE_Y:
    case CH_SCALE_Z: return _curve->_useNonUniformScale;
    default: return true;
  }
}

// Rotation channels are displayed scaled: 90 degrees = 1.0 on chart
static constexpr float ROT_CHART_SCALE = 1.0f / 90.0f;

float TransformCurveEditor::_getChannelValue(int channel, int pointIndex) const {
  auto pt = _curve->getPoint(pointIndex);
  switch (channel) {
    case CH_POS_X: return pt->_position.x;
    case CH_POS_Y: return pt->_position.y;
    case CH_POS_Z: return pt->_position.z;
    case CH_ROT_X: return pt->_eulerRotation.x * ROT_CHART_SCALE;
    case CH_ROT_Y: return pt->_eulerRotation.y * ROT_CHART_SCALE;
    case CH_ROT_Z: return pt->_eulerRotation.z * ROT_CHART_SCALE;
    case CH_SCALE_UNI: return pt->_scale.x;
    case CH_SCALE_X: return pt->_scale.x;
    case CH_SCALE_Y: return pt->_scale.y;
    case CH_SCALE_Z: return pt->_scale.z;
    default: return 0.0f;
  }
}

float TransformCurveEditor::_sampleChannelValue(int channel, float t) const {
  auto s = _curve->sample(t);
  switch (channel) {
    case CH_POS_X: return s._position.x;
    case CH_POS_Y: return s._position.y;
    case CH_POS_Z: return s._position.z;
    case CH_ROT_X: return _curve->sampleEuler(t).x * ROT_CHART_SCALE;
    case CH_ROT_Y: return _curve->sampleEuler(t).y * ROT_CHART_SCALE;
    case CH_ROT_Z: return _curve->sampleEuler(t).z * ROT_CHART_SCALE;
    case CH_SCALE_UNI: return s._scale.x;
    case CH_SCALE_X: return s._scale.x;
    case CH_SCALE_Y: return s._scale.y;
    case CH_SCALE_Z: return s._scale.z;
    default: return 0.0f;
  }
}

void TransformCurveEditor::_setChannelValue(int channel, int pointIndex, float value) {
  auto pt = _curve->getPoint(pointIndex);
  switch (channel) {
    case CH_POS_X: pt->_position.x = value; break;
    case CH_POS_Y: pt->_position.y = value; break;
    case CH_POS_Z: pt->_position.z = value; break;
    case CH_ROT_X: pt->_eulerRotation.x = value / ROT_CHART_SCALE; break;
    case CH_ROT_Y: pt->_eulerRotation.y = value / ROT_CHART_SCALE; break;
    case CH_ROT_Z: pt->_eulerRotation.z = value / ROT_CHART_SCALE; break;
    case CH_SCALE_UNI:
      pt->_scale = fvec3(value, value, value);
      break;
    case CH_SCALE_X: pt->_scale.x = value; break;
    case CH_SCALE_Y: pt->_scale.y = value; break;
    case CH_SCALE_Z: pt->_scale.z = value; break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// autoFitRanges
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

    for (int ch = 0; ch < CH_COUNT; ch++) {
      if (_channelVisible[ch] && _isChannelAvailable(ch)) {
        float v = _getChannelValue(ch, i);
        vMin = std::min(vMin, v);
        vMax = std::max(vMax, v);
      }
    }
  }

  if (vMin > vMax) { vMin = -1.0f; vMax = 1.0f; }

  float tMargin = std::max((tMax - tMin) * 0.1f, 0.5f);
  _timeMin = tMin - tMargin;
  _timeMax = tMax + tMargin;

  float vMargin = std::max((vMax - vMin) * 0.2f, 1.0f);
  _valueMin = vMin - vMargin;
  _valueMax = vMax + vMargin;
}

///////////////////////////////////////////////////////////////////////////////
// Layout helpers
///////////////////////////////////////////////////////////////////////////////

TransformCurveEditor::PlotRect TransformCurveEditor::_plotRect() const {
  int rx, ry;
  const_cast<TransformCurveEditor*>(this)->LocalToRoot(0, 0, rx, ry);
  PlotRect r;
  r.x = rx;
  r.y = ry + TOOLBAR_H;
  r.w = _geometry._w - SIDEBAR_W;
  r.h = _geometry._h - TOOLBAR_H;
  return r;
}

float TransformCurveEditor::_timeToScreenX(float t) const {
  auto r = _cachedPlotRect;
  return float(r.x) + (t - _timeMin) / (_timeMax - _timeMin) * float(r.w);
}

float TransformCurveEditor::_valueToScreenY(float v) const {
  auto r = _cachedPlotRect;
  return float(r.y + r.h) - (v - _valueMin) / (_valueMax - _valueMin) * float(r.h);
}

float TransformCurveEditor::_screenXToTime(float sx) const {
  auto r = _cachedPlotRect;
  return _timeMin + (sx - float(r.x)) / float(r.w) * (_timeMax - _timeMin);
}

float TransformCurveEditor::_screenYToValue(float sy) const {
  auto r = _cachedPlotRect;
  return _valueMin + (float(r.y + r.h) - sy) / float(r.h) * (_valueMax - _valueMin);
}

///////////////////////////////////////////////////////////////////////////////
// Hit testing
///////////////////////////////////////////////////////////////////////////////

int TransformCurveEditor::_hitTestCloseButton(int localX, int localY) const {
  int bx = _geometry._w - TOOLBAR_H;
  if (localX >= bx && localX < _geometry._w && localY >= 0 && localY < TOOLBAR_H)
    return 1;
  return 0;
}

int TransformCurveEditor::_hitTestPoint(int localX, int localY) const {
  if (!_curve) return -1;

  int rx, ry;
  const_cast<TransformCurveEditor*>(this)->LocalToRoot(0, 0, rx, ry);

  int ch = _activeEditChannel;
  if (!_isChannelAvailable(ch) || !_channelVisible[ch])
    return -1;

  for (int i = 0; i < _curve->numPoints(); i++) {
    float px = _timeToScreenX(_curve->getPoint(i)->_time);
    float py = _valueToScreenY(_getChannelValue(ch, i));

    float dx = float(localX + rx) - px;
    float dy = float(localY + ry) - py;
    if (dx * dx + dy * dy < float(HIT_RADIUS * HIT_RADIUS)) {
      return i;
    }
  }
  return -1;
}

int TransformCurveEditor::_hitTestSidebarCheckbox(int localX, int localY) const {
  int sidebarX = _geometry._w - SIDEBAR_W;
  if (localX < sidebarX || localX >= sidebarX + 16)
    return -1;
  int adjustedY = localY - TOOLBAR_H;
  if (adjustedY < 0) return -1;
  int row = adjustedY / ROW_H;
  if (row >= CH_COUNT) return -1;
  return row;
}

int TransformCurveEditor::_hitTestSidebarLabel(int localX, int localY) const {
  int sidebarX = _geometry._w - SIDEBAR_W;
  if (localX < sidebarX + 16 || localX >= _geometry._w)
    return -1;
  int adjustedY = localY - TOOLBAR_H;
  if (adjustedY < 0) return -1;
  int row = adjustedY / ROW_H;
  if (row >= CH_COUNT) return -1;
  return row;
}

int TransformCurveEditor::_hitTestNonUniformToggle(int localX, int localY) const {
  int sidebarX = _geometry._w - SIDEBAR_W;
  if (localX < sidebarX || localX >= _geometry._w)
    return 0;
  int toggleY = TOOLBAR_H + CH_COUNT * ROW_H + 4;
  if (localY >= toggleY && localY < toggleY + ROW_H)
    return 1;
  return 0;
}

int TransformCurveEditor::_hitTestResetButton(int localX, int localY) const {
  // Reset button is left of close button: [R][X]
  int bx = _geometry._w - TOOLBAR_H * 2;
  if (localX >= bx && localX < bx + TOOLBAR_H && localY >= 0 && localY < TOOLBAR_H)
    return 1;
  return 0;
}

bool TransformCurveEditor::_isInPlotArea(int localX, int localY) const {
  int sidebarX = _geometry._w - SIDEBAR_W;
  return localX >= 0 && localX < sidebarX && localY >= TOOLBAR_H && localY < _geometry._h;
}

///////////////////////////////////////////////////////////////////////////////
// Event handling
///////////////////////////////////////////////////////////////////////////////

HandlerResult TransformCurveEditor::DoOnUiEvent(event_constptr_t ev) {
  // Ensure cached plot rect is current for coordinate mapping
  _cachedPlotRect = _plotRect();

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
      } else if (key == 70) { // 'F' — fit/reset view
        autoFitRanges();
        rval.setHandled(this);
      } else if (key == 88) { // 'X' — pan mode
        _xKeyDown = true;
        _navPrevRootX = ev->miX;
        _navPrevRootY = ev->miY;
        rval.setHandled(this);
      } else if (key == 67) { // 'C' — zoom mode
        _cKeyDown = true;
        _navPrevRootX = ev->miX;
        _navPrevRootY = ev->miY;
        rval.setHandled(this);
      }
      break;
    }
    case EventCode::KEY_UP: {
      int key = ev->miKeyCode;
      if (key == 88) { // 'X'
        _xKeyDown = false;
        rval.setHandled(this);
      } else if (key == 67) { // 'C'
        _cKeyDown = false;
        rval.setHandled(this);
      }
      break;
    }
    case EventCode::PUSH: {
      // Close button
      if (_hitTestCloseButton(localX, localY)) {
        if (_onClose) _onClose();
        rval.setHandled(this);
        break;
      }
      // Reset view button
      if (_hitTestResetButton(localX, localY)) {
        autoFitRanges();
        rval.setHandled(this);
        break;
      }
      // Non-uniform scale toggle
      if (_hitTestNonUniformToggle(localX, localY)) {
        _curve->_useNonUniformScale = !_curve->_useNonUniformScale;
        // Adjust active channel if needed
        if (_curve->_useNonUniformScale && _activeEditChannel == CH_SCALE_UNI) {
          _activeEditChannel = CH_SCALE_X;
        } else if (!_curve->_useNonUniformScale &&
                   (_activeEditChannel == CH_SCALE_X ||
                    _activeEditChannel == CH_SCALE_Y ||
                    _activeEditChannel == CH_SCALE_Z)) {
          _activeEditChannel = CH_SCALE_UNI;
        }
        // Adjust visibility
        if (_curve->_useNonUniformScale) {
          bool wasVis = _channelVisible[CH_SCALE_UNI];
          _channelVisible[CH_SCALE_X] = wasVis;
          _channelVisible[CH_SCALE_Y] = wasVis;
          _channelVisible[CH_SCALE_Z] = wasVis;
        } else {
          bool anyVis = _channelVisible[CH_SCALE_X] ||
                        _channelVisible[CH_SCALE_Y] ||
                        _channelVisible[CH_SCALE_Z];
          _channelVisible[CH_SCALE_UNI] = anyVis;
        }
        if (_onCurveChanged) _onCurveChanged();
        rval.setHandled(this);
        break;
      }
      // Sidebar checkbox
      int cbCh = _hitTestSidebarCheckbox(localX, localY);
      if (cbCh >= 0 && _isChannelAvailable(cbCh)) {
        _channelVisible[cbCh] = !_channelVisible[cbCh];
        rval.setHandled(this);
        break;
      }
      // Sidebar label (active edit channel)
      int lblCh = _hitTestSidebarLabel(localX, localY);
      if (lblCh >= 0 && _isChannelAvailable(lblCh)) {
        _activeEditChannel = lblCh;
        if (!_channelVisible[lblCh])
          _channelVisible[lblCh] = true;
        rval.setHandled(this);
        break;
      }
      // Plot area: hit-test points of active edit channel
      int idx = _hitTestPoint(localX, localY);
      if (idx >= 0) {
        _selectedPointIndex = idx;
        _dragging = true;
        rval.setHandled(this);
      } else {
        _selectedPointIndex = -1;
        rval.setHandled(this);
      }
      break;
    }
    case EventCode::DRAG: {
      // Point drag
      if (_dragging && _selectedPointIndex >= 0 && _curve) {
        float newTime = _screenXToTime(float(ev->miX));
        float newValue = _screenYToValue(float(ev->miY));
        newTime = std::clamp(newTime, _timeMin, _timeMax);

        auto pt = _curve->getPoint(_selectedPointIndex);
        pt->_time = newTime;
        _setChannelValue(_activeEditChannel, _selectedPointIndex, newValue);
        _curve->setPoint(_selectedPointIndex, pt);

        // Re-locate after sort
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
    case EventCode::MOVE: {
      // X held → pan
      if (_xKeyDown) {
        auto pr = _cachedPlotRect;
        float dx = float(ev->miX - _navPrevRootX);
        float dy = float(ev->miY - _navPrevRootY);
        float dt = -dx / float(pr.w) * (_timeMax - _timeMin);
        float dv = dy / float(pr.h) * (_valueMax - _valueMin);
        _timeMin += dt;
        _timeMax += dt;
        _valueMin += dv;
        _valueMax += dv;
        _navPrevRootX = ev->miX;
        _navPrevRootY = ev->miY;
        rval.setHandled(this);
        break;
      }
      // C held → zoom (drag right/up = zoom in, left/down = zoom out)
      if (_cKeyDown) {
        auto pr = _cachedPlotRect;
        float dx = float(ev->miX - _navPrevRootX);
        float dy = float(ev->miY - _navPrevRootY);
        // Sensitivity: 200 pixels of drag = 2x zoom
        float zoomX = 1.0f - dx * 0.005f;
        float zoomY = 1.0f + dy * 0.005f;
        zoomX = std::clamp(zoomX, 0.5f, 2.0f);
        zoomY = std::clamp(zoomY, 0.5f, 2.0f);
        // Zoom around center of view
        float tCenter = (_timeMin + _timeMax) * 0.5f;
        float vCenter = (_valueMin + _valueMax) * 0.5f;
        float tHalf = (_timeMax - _timeMin) * 0.5f * zoomX;
        float vHalf = (_valueMax - _valueMin) * 0.5f * zoomY;
        _timeMin = tCenter - tHalf;
        _timeMax = tCenter + tHalf;
        _valueMin = vCenter - vHalf;
        _valueMax = vCenter + vHalf;
        _navPrevRootX = ev->miX;
        _navPrevRootY = ev->miY;
        rval.setHandled(this);
        break;
      }
      break;
    }
    case EventCode::DOUBLECLICK: {
      // Only add points in plot area
      if (_isInPlotArea(localX, localY) && _curve) {
        float t = _screenXToTime(float(ev->miX));
        float v = _screenYToValue(float(ev->miY));

        // Sample curve at t for all channels, override active channel
        auto sample = _curve->sample(t);
        auto newPt = std::make_shared<math::TransformCurvePoint>();
        newPt->_time = t;
        newPt->_position = sample._position;
        newPt->_eulerRotation = _curve->sampleEuler(t);
        newPt->_scale = sample._scale;

        // Override the active channel value
        // (need to add the point first, then set, because _setChannelValue accesses by index)
        int idx = _curve->addPoint(newPt);
        _setChannelValue(_activeEditChannel, idx, v);
        _selectedPointIndex = idx;

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
// Drawing helpers
///////////////////////////////////////////////////////////////////////////////

static void _drawQuad(
    lev2::Context* ctx, lev2::rcfd_ptr_t RCFD,
    lev2::freestyle_mtl_ptr_t mtl,
    const lev2::FxShaderTechnique* tek,
    const lev2::FxShaderParam* parmvp,
    const fmtx4& uiMtx,
    float x1, float y1, float x2, float y2,
    const fvec3& color) {
  auto gbi = ctx->GBI();
  using vtx_t = lev2::SVtxV16T16C16;
  auto vb = lev2::GfxEnv::GetSharedDynamicV16T16C16();
  lev2::VtxWriter<vtx_t> vw;
  vw.Lock(ctx, vb.get(), 6);
  vtx_t v0(fvec3(x1, y1, 0), fvec4(), color);
  vtx_t v1(fvec3(x2, y1, 0), fvec4(), color);
  vtx_t v2(fvec3(x1, y2, 0), fvec4(), color);
  vtx_t v3(fvec3(x2, y2, 0), fvec4(), color);
  vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
  vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
  vw.UnLock(ctx);
  mtl->begin(tek, RCFD);
  mtl->bindParamMatrix(parmvp, uiMtx);
  gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
  mtl->end(RCFD);
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveEditor::_drawToolbar(
    lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, const fmtx4& uiMtx) {
  int rx, ry;
  LocalToRoot(0, 0, rx, ry);

  // Toolbar background
  _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
            float(rx), float(ry), float(rx + _geometry._w), float(ry + TOOLBAR_H),
            fvec3(0.2f, 0.2f, 0.25f));

  // Reset view button [R] — left of close button
  _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
            float(rx + _geometry._w - TOOLBAR_H * 2), float(ry),
            float(rx + _geometry._w - TOOLBAR_H), float(ry + TOOLBAR_H),
            fvec3(0.25f, 0.35f, 0.5f));

  // Close button [X]
  _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
            float(rx + _geometry._w - TOOLBAR_H), float(ry),
            float(rx + _geometry._w), float(ry + TOOLBAR_H),
            fvec3(0.7f, 0.2f, 0.2f));

  // Title + button text
  lev2::FontMan::PushFont("i14");
  lev2::FontMan::beginTextBlock(ctx, 256);
  lev2::FontMan::DrawText(ctx, rx + 8, ry + 4, "TransformCurve Editor");
  lev2::FontMan::DrawText(ctx, rx + _geometry._w - TOOLBAR_H * 2 + 6, ry + 4, "R");
  lev2::FontMan::DrawText(ctx, rx + _geometry._w - TOOLBAR_H + 6, ry + 4, "X");
  lev2::FontMan::endTextBlock(ctx);
  lev2::FontMan::PopFont();
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveEditor::_drawPlotBackground(
    lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, const fmtx4& uiMtx) {
  auto r = _cachedPlotRect;
  _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
            float(r.x), float(r.y), float(r.x + r.w), float(r.y + r.h),
            fvec3(0.12f, 0.12f, 0.14f));
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveEditor::_drawGrid(
    lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, const fmtx4& uiMtx) {
  auto gbi = ctx->GBI();
  using vtx_t = lev2::SVtxV16T16C16;
  auto vb = lev2::GfxEnv::GetSharedDynamicV16T16C16();
  auto r = _cachedPlotRect;

  // Time grid: major = 1s, minor = 0.25s
  constexpr float MAJOR_T = 1.0f;
  constexpr float MINOR_T = 0.25f;
  fvec3 majorColor(0.28f, 0.28f, 0.32f);
  fvec3 minorColor(0.19f, 0.19f, 0.21f);

  // Value grid: adaptive "nice number" step sizes
  // Find a nice major step so we get ~5-8 major divisions
  auto niceStep = [](float range) -> float {
    float rough = range / 6.0f;
    float mag = powf(10.0f, floorf(log10f(rough)));
    float norm = rough / mag;
    float nice;
    if (norm < 1.5f) nice = 1.0f;
    else if (norm < 3.5f) nice = 2.0f;
    else if (norm < 7.5f) nice = 5.0f;
    else nice = 10.0f;
    return nice * mag;
  };

  float valueRange = _valueMax - _valueMin;
  float valueMajor = niceStep(valueRange);
  float valueMinor = valueMajor * 0.25f;

  // Count lines for buffer allocation
  int numMinorT = 0, numMajorT = 0;
  for (float t = ceilf(_timeMin / MINOR_T) * MINOR_T; t <= _timeMax; t += MINOR_T) {
    if (std::abs(t) < 1e-6f) continue;
    bool isMajor = (std::abs(fmodf(t, MAJOR_T)) < 1e-4f) || (std::abs(fmodf(t, MAJOR_T) - MAJOR_T) < 1e-4f);
    if (isMajor) numMajorT++; else numMinorT++;
  }
  int numValueMinor = 0, numValueMajor = 0;
  for (float v = ceilf(_valueMin / valueMinor) * valueMinor; v <= _valueMax; v += valueMinor) {
    if (std::abs(v) < 1e-6f) continue;
    bool isMajor = (std::abs(fmodf(v, valueMajor)) < valueMajor * 0.01f) ||
                   (std::abs(fmodf(v, valueMajor) - valueMajor) < valueMajor * 0.01f);
    if (isMajor) numValueMajor++; else numValueMinor++;
  }

  int totalLines = numMinorT + numMajorT + numValueMinor + numValueMajor;
  if (totalLines > 0) {
    lev2::VtxWriter<vtx_t> vw;
    vw.Lock(ctx, vb.get(), 6 * totalLines);

    // Minor time lines (0.25s)
    for (float t = ceilf(_timeMin / MINOR_T) * MINOR_T; t <= _timeMax; t += MINOR_T) {
      if (std::abs(t) < 1e-6f) continue;
      bool isMajor = (std::abs(fmodf(t, MAJOR_T)) < 1e-4f) || (std::abs(fmodf(t, MAJOR_T) - MAJOR_T) < 1e-4f);
      if (isMajor) continue;
      float sx = _timeToScreenX(t);
      vtx_t v0(fvec3(sx, float(r.y), 0), fvec4(), minorColor);
      vtx_t v1(fvec3(sx + 1.0f, float(r.y), 0), fvec4(), minorColor);
      vtx_t v2(fvec3(sx, float(r.y + r.h), 0), fvec4(), minorColor);
      vtx_t v3(fvec3(sx + 1.0f, float(r.y + r.h), 0), fvec4(), minorColor);
      vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
      vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
    }

    // Major time lines (1s)
    for (float t = ceilf(_timeMin / MAJOR_T) * MAJOR_T; t <= _timeMax; t += MAJOR_T) {
      if (std::abs(t) < 1e-6f) continue;
      float sx = _timeToScreenX(t);
      vtx_t v0(fvec3(sx, float(r.y), 0), fvec4(), majorColor);
      vtx_t v1(fvec3(sx + 1.0f, float(r.y), 0), fvec4(), majorColor);
      vtx_t v2(fvec3(sx, float(r.y + r.h), 0), fvec4(), majorColor);
      vtx_t v3(fvec3(sx + 1.0f, float(r.y + r.h), 0), fvec4(), majorColor);
      vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
      vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
    }

    // Minor value lines
    for (float v = ceilf(_valueMin / valueMinor) * valueMinor; v <= _valueMax; v += valueMinor) {
      if (std::abs(v) < 1e-6f) continue;
      bool isMajor = (std::abs(fmodf(v, valueMajor)) < valueMajor * 0.01f) ||
                     (std::abs(fmodf(v, valueMajor) - valueMajor) < valueMajor * 0.01f);
      if (isMajor) continue;
      float sy = _valueToScreenY(v);
      vtx_t v0(fvec3(float(r.x), sy, 0), fvec4(), minorColor);
      vtx_t v1(fvec3(float(r.x + r.w), sy, 0), fvec4(), minorColor);
      vtx_t v2(fvec3(float(r.x), sy + 1.0f, 0), fvec4(), minorColor);
      vtx_t v3(fvec3(float(r.x + r.w), sy + 1.0f, 0), fvec4(), minorColor);
      vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
      vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
    }

    // Major value lines
    for (float v = ceilf(_valueMin / valueMajor) * valueMajor; v <= _valueMax; v += valueMajor) {
      if (std::abs(v) < 1e-6f) continue;
      float sy = _valueToScreenY(v);
      vtx_t v0(fvec3(float(r.x), sy, 0), fvec4(), majorColor);
      vtx_t v1(fvec3(float(r.x + r.w), sy, 0), fvec4(), majorColor);
      vtx_t v2(fvec3(float(r.x), sy + 1.0f, 0), fvec4(), majorColor);
      vtx_t v3(fvec3(float(r.x + r.w), sy + 1.0f, 0), fvec4(), majorColor);
      vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
      vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
    }

    vw.UnLock(ctx);
    _material->begin(_tekvtxcolor, RCFD);
    _material->bindParamMatrix(_parmvp, uiMtx);
    gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
    _material->end(RCFD);
  }
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveEditor::_drawOriginLines(
    lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, const fmtx4& uiMtx) {
  auto r = _cachedPlotRect;
  fvec3 originColor(0.55f, 0.55f, 0.6f);
  float thickness = 2.0f;

  // Horizontal origin line (value = 0)
  if (_valueMin <= 0.0f && _valueMax >= 0.0f) {
    float sy = _valueToScreenY(0.0f);
    _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
              float(r.x), sy - thickness * 0.5f,
              float(r.x + r.w), sy + thickness * 0.5f,
              originColor);
  }

  // Vertical origin line (time = 0)
  if (_timeMin <= 0.0f && _timeMax >= 0.0f) {
    float sx = _timeToScreenX(0.0f);
    _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
              sx - thickness * 0.5f, float(r.y),
              sx + thickness * 0.5f, float(r.y + r.h),
              originColor);
  }
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveEditor::_drawChannel(
    lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, int channel, const fmtx4& uiMtx) {
  if (!_curve || _curve->numPoints() < 2) return;

  auto gbi = ctx->GBI();
  using vtx_t = lev2::SVtxV16T16C16;
  auto vb = lev2::GfxEnv::GetSharedDynamicV16T16C16();

  fvec3 color = kChannelInfo[channel]._color;
  // Dim non-active channels slightly
  if (channel != _activeEditChannel) {
    color = color * 0.6f;
  }

  lev2::VtxWriter<vtx_t> vw;
  vw.Lock(ctx, vb.get(), 6 * CURVE_SEGMENTS);

  for (int seg = 0; seg < CURVE_SEGMENTS; seg++) {
    float t0 = _timeMin + float(seg) / float(CURVE_SEGMENTS) * (_timeMax - _timeMin);
    float t1 = _timeMin + float(seg + 1) / float(CURVE_SEGMENTS) * (_timeMax - _timeMin);

    float v0 = _sampleChannelValue(channel, t0);
    float v1 = _sampleChannelValue(channel, t1);

    float sx0 = _timeToScreenX(t0);
    float sx1 = _timeToScreenX(t1);
    float sy0 = _valueToScreenY(v0);
    float sy1 = _valueToScreenY(v1);

    vtx_t vt0(fvec3(sx0, sy0, 0), fvec4(), color);
    vtx_t vt1(fvec3(sx1, sy1, 0), fvec4(), color);
    vtx_t vt2(fvec3(sx0, sy0 + 1.0f, 0), fvec4(), color);
    vtx_t vt3(fvec3(sx1, sy1 + 1.0f, 0), fvec4(), color);
    vw.AddVertex(vt0); vw.AddVertex(vt1); vw.AddVertex(vt2);
    vw.AddVertex(vt1); vw.AddVertex(vt3); vw.AddVertex(vt2);
  }

  vw.UnLock(ctx);
  _material->begin(_tekvtxcolor, RCFD);
  _material->bindParamMatrix(_parmvp, uiMtx);
  gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
  _material->end(RCFD);
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveEditor::_drawControlPoints(
    lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, int channel, const fmtx4& uiMtx) {
  if (!_curve) return;
  int numPts = _curve->numPoints();
  if (numPts == 0) return;

  auto gbi = ctx->GBI();
  using vtx_t = lev2::SVtxV16T16C16;
  auto vb = lev2::GfxEnv::GetSharedDynamicV16T16C16();

  fvec3 chColor = kChannelInfo[channel]._color;
  bool isActive = (channel == _activeEditChannel);

  lev2::VtxWriter<vtx_t> vw;
  vw.Lock(ctx, vb.get(), 6 * numPts);

  for (int i = 0; i < numPts; i++) {
    float sx = _timeToScreenX(_curve->getPoint(i)->_time);
    float sy = _valueToScreenY(_getChannelValue(channel, i));

    fvec3 ptColor;
    if (isActive && i == _selectedPointIndex) {
      ptColor = fvec3(1.0f, 1.0f, 0.3f);
    } else if (isActive) {
      ptColor = chColor;
    } else {
      ptColor = chColor * 0.5f;
    }

    float half = isActive ? float(HIT_RADIUS) : float(HIT_RADIUS - 2);
    vtx_t v0(fvec3(sx - half, sy - half, 0), fvec4(), ptColor);
    vtx_t v1(fvec3(sx + half, sy - half, 0), fvec4(), ptColor);
    vtx_t v2(fvec3(sx - half, sy + half, 0), fvec4(), ptColor);
    vtx_t v3(fvec3(sx + half, sy + half, 0), fvec4(), ptColor);
    vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
    vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
  }

  vw.UnLock(ctx);
  _material->begin(_tekvtxcolor, RCFD);
  _material->bindParamMatrix(_parmvp, uiMtx);
  gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
  _material->end(RCFD);

  // Draw time/value label for selected point on active channel
  if (isActive && _selectedPointIndex >= 0 && _selectedPointIndex < numPts) {
    auto pt = _curve->getPoint(_selectedPointIndex);
    float sx = _timeToScreenX(pt->_time);
    float sy = _valueToScreenY(_getChannelValue(channel, _selectedPointIndex));
    float chartVal = _getChannelValue(channel, _selectedPointIndex);
    // Show actual value in label (degrees for rotation, not chart-scaled)
    bool isRot = (channel >= CH_ROT_X && channel <= CH_ROT_Z);
    float labelVal = isRot ? (chartVal / ROT_CHART_SCALE) : chartVal;
    const char* suffix = isRot ? "\xc2\xb0" : "";  // degree sign for rotation

    char buf[64];
    snprintf(buf, sizeof(buf), "t:%.2f v:%.1f%s", pt->_time, labelVal, suffix);

    // Background pill behind text
    int textX = int(sx) + HIT_RADIUS + 4;
    int textY = int(sy) - 16;
    int textW = 110;
    int textH = 16;
    _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
              float(textX - 2), float(textY - 1),
              float(textX + textW), float(textY + textH),
              fvec3(0.08f, 0.08f, 0.1f));

    lev2::FontMan::PushFont("i14");
    lev2::FontMan::beginTextBlock(ctx, 64);
    lev2::FontMan::DrawText(ctx, textX, textY, buf);
    lev2::FontMan::endTextBlock(ctx);
    lev2::FontMan::PopFont();
  }
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveEditor::_drawSidebar(
    lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, const fmtx4& uiMtx) {
  int rx, ry;
  LocalToRoot(0, 0, rx, ry);

  int sidebarX = rx + _geometry._w - SIDEBAR_W;
  int sidebarY = ry + TOOLBAR_H;
  int sidebarH = _geometry._h - TOOLBAR_H;

  // Sidebar background
  _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
            float(sidebarX), float(sidebarY),
            float(sidebarX + SIDEBAR_W), float(sidebarY + sidebarH),
            fvec3(0.16f, 0.16f, 0.18f));

  // Separator line between plot and sidebar
  _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
            float(sidebarX), float(sidebarY),
            float(sidebarX + 1), float(sidebarY + sidebarH),
            fvec3(0.35f, 0.35f, 0.4f));

  lev2::FontMan::PushFont("i14");

  for (int ch = 0; ch < CH_COUNT; ch++) {
    bool avail = _isChannelAvailable(ch);
    if (!avail) continue;

    int rowY = sidebarY + ch * ROW_H;

    // Active edit highlight (background)
    if (ch == _activeEditChannel) {
      _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
                float(sidebarX + 2), float(rowY + 1),
                float(sidebarX + SIDEBAR_W - 2), float(rowY + ROW_H - 1),
                fvec3(0.25f, 0.25f, 0.35f));
    }

    // Checkbox
    fvec3 cbColor = _channelVisible[ch] ? kChannelInfo[ch]._color : fvec3(0.3f, 0.3f, 0.3f);
    _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
              float(sidebarX + 4), float(rowY + 4),
              float(sidebarX + 16), float(rowY + 16),
              cbColor);

    // Label text
    lev2::FontMan::beginTextBlock(ctx, 32);
    lev2::FontMan::DrawText(ctx, sidebarX + 20, rowY + 2, kChannelInfo[ch]._label);
    lev2::FontMan::endTextBlock(ctx);
  }

  // Non-uniform scale toggle
  int toggleY = sidebarY + CH_COUNT * ROW_H + 4;
  bool nonUni = _curve ? _curve->_useNonUniformScale : false;
  fvec3 toggleColor = nonUni ? fvec3(0.5f, 0.7f, 0.5f) : fvec3(0.3f, 0.3f, 0.3f);
  _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
            float(sidebarX + 4), float(toggleY + 4),
            float(sidebarX + 16), float(toggleY + 16),
            toggleColor);

  lev2::FontMan::beginTextBlock(ctx, 32);
  lev2::FontMan::DrawText(ctx, sidebarX + 20, toggleY + 2, "Non-Uni");
  lev2::FontMan::endTextBlock(ctx);

  lev2::FontMan::PopFont();
}

///////////////////////////////////////////////////////////////////////////////
// Main draw
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

  // Cache the plot rect for this frame
  _cachedPlotRect = _plotRect();

  int rx, ry;
  LocalToRoot(0, 0, rx, ry);

  // Outer background
  _drawQuad(context, RCFD, _material, _tekvtxcolor, _parmvp, uiMatrix,
            float(rx), float(ry),
            float(rx + _geometry._w), float(ry + _geometry._h),
            fvec3(0.15f, 0.15f, 0.18f));

  _drawToolbar(context, RCFD, uiMatrix);
  _drawPlotBackground(context, RCFD, uiMatrix);
  _drawGrid(context, RCFD, uiMatrix);
  _drawOriginLines(context, RCFD, uiMatrix);

  // Draw channels: non-active first, then active on top
  for (int ch = 0; ch < CH_COUNT; ch++) {
    if (ch == _activeEditChannel) continue;
    if (_channelVisible[ch] && _isChannelAvailable(ch)) {
      _drawChannel(context, RCFD, ch, uiMatrix);
      _drawControlPoints(context, RCFD, ch, uiMatrix);
    }
  }
  // Draw active channel last (on top)
  if (_activeEditChannel >= 0 && _activeEditChannel < CH_COUNT &&
      _channelVisible[_activeEditChannel] && _isChannelAvailable(_activeEditChannel)) {
    _drawChannel(context, RCFD, _activeEditChannel, uiMatrix);
    _drawControlPoints(context, RCFD, _activeEditChannel, uiMatrix);
  }

  _drawSidebar(context, RCFD, uiMatrix);

  mtxi->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
