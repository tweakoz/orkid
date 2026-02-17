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

bool TransformCurveEditor::_isPositionChannel(int ch) const {
  return ch == CH_POS_X || ch == CH_POS_Y || ch == CH_POS_Z;
}

// Rotation channels are displayed scaled: 90 degrees = 1.0 on chart
static constexpr float ROT_CHART_SCALE = 1.0f / 90.0f;

math::CurveChannel TransformCurveEditor::_editorChannelToCurveChannel(int editorCh) const {
  switch (editorCh) {
    case CH_POS_X: return math::CurveChannel::CC_POS_X;
    case CH_POS_Y: return math::CurveChannel::CC_POS_Y;
    case CH_POS_Z: return math::CurveChannel::CC_POS_Z;
    case CH_ROT_X: return math::CurveChannel::CC_ROT_X;
    case CH_ROT_Y: return math::CurveChannel::CC_ROT_Y;
    case CH_ROT_Z: return math::CurveChannel::CC_ROT_Z;
    case CH_SCALE_UNI: return math::CurveChannel::CC_SCALE_X;  // representative
    case CH_SCALE_X: return math::CurveChannel::CC_SCALE_X;
    case CH_SCALE_Y: return math::CurveChannel::CC_SCALE_Y;
    case CH_SCALE_Z: return math::CurveChannel::CC_SCALE_Z;
    default: return math::CurveChannel::CC_POS_X;
  }
}

void TransformCurveEditor::_cycleSegmentType() {
  if (_selectedPointIndex < 0 || !_curve) return;
  int segIdx = _selectedPointIndex;
  int numSegs = _curve->numPoints() - 1;
  if (segIdx >= numSegs) return;

  auto cc = _editorChannelToCurveChannel(_activeEditChannel);
  auto curType = _curve->getChannelSegmentType(segIdx, cc);
  math::CurveSegmentType nextType;
  switch (curType) {
    case math::CurveSegmentType::LINEAR:
      nextType = math::CurveSegmentType::BEZIER;
      break;
    case math::CurveSegmentType::BEZIER:
      nextType = math::CurveSegmentType::CATMULL_ROM;
      break;
    case math::CurveSegmentType::CATMULL_ROM:
      nextType = math::CurveSegmentType::STEP;
      break;
    case math::CurveSegmentType::STEP:
    default:
      nextType = math::CurveSegmentType::LINEAR;
      break;
  }

  // For CH_SCALE_UNI, set all 3 scale channels
  if (_activeEditChannel == CH_SCALE_UNI) {
    _curve->setChannelSegmentType(segIdx, math::CurveChannel::CC_SCALE_X, nextType);
    _curve->setChannelSegmentType(segIdx, math::CurveChannel::CC_SCALE_Y, nextType);
    _curve->setChannelSegmentType(segIdx, math::CurveChannel::CC_SCALE_Z, nextType);
  } else {
    _curve->setChannelSegmentType(segIdx, cc, nextType);
  }

  // When entering BEZIER, initialize tangent handles for the active channel group
  if (nextType == math::CurveSegmentType::BEZIER) {
    auto ptA = _curve->getPoint(segIdx);
    auto ptB = _curve->getPoint(segIdx + 1);
    float timeDelta = ptB->_time - ptA->_time;
    float bump = std::max(0.3f, timeDelta * 0.15f);

    if (_isPositionChannel(_activeEditChannel)) {
      fvec3 delta = ptB->_position - ptA->_position;
      ptA->_tangent_out = delta * (1.0f / 3.0f);
      ptB->_tangent_in = delta * (-1.0f / 3.0f);
      ptA->_tangent_out.y += bump;
      ptB->_tangent_in.y += bump;
    } else if (_activeEditChannel >= CH_ROT_X && _activeEditChannel <= CH_ROT_Z) {
      fvec3 delta = ptB->_eulerRotation - ptA->_eulerRotation;
      ptA->_rot_tangent_out = delta * (1.0f / 3.0f);
      ptB->_rot_tangent_in = delta * (-1.0f / 3.0f);
      // bump the active component
      int comp = _activeEditChannel - CH_ROT_X;
      (&ptA->_rot_tangent_out.x)[comp] += bump / ROT_CHART_SCALE;
      (&ptB->_rot_tangent_in.x)[comp] += bump / ROT_CHART_SCALE;
    } else {
      // scale channels
      fvec3 delta = ptB->_scale - ptA->_scale;
      ptA->_scale_tangent_out = delta * (1.0f / 3.0f);
      ptB->_scale_tangent_in = delta * (-1.0f / 3.0f);
      int comp = (_activeEditChannel == CH_SCALE_UNI) ? 0 : (_activeEditChannel - CH_SCALE_X);
      (&ptA->_scale_tangent_out.x)[comp] += bump;
      (&ptB->_scale_tangent_in.x)[comp] += bump;
    }
  }

  _curve->enforceLoopConstraints();
  if (_onCurveChanged) _onCurveChanged();
}

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
// Tangent helpers
///////////////////////////////////////////////////////////////////////////////

float TransformCurveEditor::_getTangentOutComponent(int ch, int ptIdx) const {
  auto pt = _curve->getPoint(ptIdx);
  switch (ch) {
    case CH_POS_X: return pt->_tangent_out.x;
    case CH_POS_Y: return pt->_tangent_out.y;
    case CH_POS_Z: return pt->_tangent_out.z;
    case CH_ROT_X: return pt->_rot_tangent_out.x * ROT_CHART_SCALE;
    case CH_ROT_Y: return pt->_rot_tangent_out.y * ROT_CHART_SCALE;
    case CH_ROT_Z: return pt->_rot_tangent_out.z * ROT_CHART_SCALE;
    case CH_SCALE_UNI: return pt->_scale_tangent_out.x;
    case CH_SCALE_X: return pt->_scale_tangent_out.x;
    case CH_SCALE_Y: return pt->_scale_tangent_out.y;
    case CH_SCALE_Z: return pt->_scale_tangent_out.z;
    default: return 0.0f;
  }
}

float TransformCurveEditor::_getTangentInComponent(int ch, int ptIdx) const {
  auto pt = _curve->getPoint(ptIdx);
  switch (ch) {
    case CH_POS_X: return pt->_tangent_in.x;
    case CH_POS_Y: return pt->_tangent_in.y;
    case CH_POS_Z: return pt->_tangent_in.z;
    case CH_ROT_X: return pt->_rot_tangent_in.x * ROT_CHART_SCALE;
    case CH_ROT_Y: return pt->_rot_tangent_in.y * ROT_CHART_SCALE;
    case CH_ROT_Z: return pt->_rot_tangent_in.z * ROT_CHART_SCALE;
    case CH_SCALE_UNI: return pt->_scale_tangent_in.x;
    case CH_SCALE_X: return pt->_scale_tangent_in.x;
    case CH_SCALE_Y: return pt->_scale_tangent_in.y;
    case CH_SCALE_Z: return pt->_scale_tangent_in.z;
    default: return 0.0f;
  }
}

void TransformCurveEditor::_setTangentOutComponent(int ch, int ptIdx, float val) {
  auto pt = _curve->getPoint(ptIdx);
  switch (ch) {
    case CH_POS_X: pt->_tangent_out.x = val; break;
    case CH_POS_Y: pt->_tangent_out.y = val; break;
    case CH_POS_Z: pt->_tangent_out.z = val; break;
    case CH_ROT_X: pt->_rot_tangent_out.x = val / ROT_CHART_SCALE; break;
    case CH_ROT_Y: pt->_rot_tangent_out.y = val / ROT_CHART_SCALE; break;
    case CH_ROT_Z: pt->_rot_tangent_out.z = val / ROT_CHART_SCALE; break;
    case CH_SCALE_UNI:
      pt->_scale_tangent_out = fvec3(val, val, val);
      break;
    case CH_SCALE_X: pt->_scale_tangent_out.x = val; break;
    case CH_SCALE_Y: pt->_scale_tangent_out.y = val; break;
    case CH_SCALE_Z: pt->_scale_tangent_out.z = val; break;
    default: break;
  }
}

void TransformCurveEditor::_setTangentInComponent(int ch, int ptIdx, float val) {
  auto pt = _curve->getPoint(ptIdx);
  switch (ch) {
    case CH_POS_X: pt->_tangent_in.x = val; break;
    case CH_POS_Y: pt->_tangent_in.y = val; break;
    case CH_POS_Z: pt->_tangent_in.z = val; break;
    case CH_ROT_X: pt->_rot_tangent_in.x = val / ROT_CHART_SCALE; break;
    case CH_ROT_Y: pt->_rot_tangent_in.y = val / ROT_CHART_SCALE; break;
    case CH_ROT_Z: pt->_rot_tangent_in.z = val / ROT_CHART_SCALE; break;
    case CH_SCALE_UNI:
      pt->_scale_tangent_in = fvec3(val, val, val);
      break;
    case CH_SCALE_X: pt->_scale_tangent_in.x = val; break;
    case CH_SCALE_Y: pt->_scale_tangent_in.y = val; break;
    case CH_SCALE_Z: pt->_scale_tangent_in.z = val; break;
    default: break;
  }
}

float TransformCurveEditor::_tangentOutScreenX(int ptIdx) const {
  auto pt = _curve->getPoint(ptIdx);
  int numPts = _curve->numPoints();
  if (ptIdx + 1 >= numPts) return _timeToScreenX(pt->_time);
  float nextTime = _curve->getPoint(ptIdx + 1)->_time;
  float segDur = nextTime - pt->_time;
  return _timeToScreenX(pt->_time + segDur * TANGENT_TIME_FRACTION);
}

float TransformCurveEditor::_tangentOutScreenY(int ptIdx, int ch) const {
  float ptVal = _getChannelValue(ch, ptIdx);
  float tangentComp = _getTangentOutComponent(ch, ptIdx);
  return _valueToScreenY(ptVal + tangentComp);
}

float TransformCurveEditor::_tangentInScreenX(int ptIdx) const {
  auto pt = _curve->getPoint(ptIdx);
  if (ptIdx <= 0) return _timeToScreenX(pt->_time);
  float prevTime = _curve->getPoint(ptIdx - 1)->_time;
  float segDur = pt->_time - prevTime;
  return _timeToScreenX(pt->_time - segDur * TANGENT_TIME_FRACTION);
}

float TransformCurveEditor::_tangentInScreenY(int ptIdx, int ch) const {
  float ptVal = _getChannelValue(ch, ptIdx);
  float tangentComp = _getTangentInComponent(ch, ptIdx);
  return _valueToScreenY(ptVal + tangentComp);
}

int TransformCurveEditor::_hitTestTangentHandle(int lx, int ly, DragMode& outMode) const {
  if (!_curve) return -1;
  int ch = _activeEditChannel;
  auto cc = _editorChannelToCurveChannel(ch);
  int numPts = _curve->numPoints();

  int rx, ry;
  const_cast<TransformCurveEditor*>(this)->LocalToRoot(0, 0, rx, ry);
  float mx = float(lx + rx);
  float my = float(ly + ry);

  for (int i = 0; i < numPts; i++) {
    // tangent_out: check if segment to right is BEZIER for this channel
    if (i + 1 < numPts) {
      auto segType = _curve->getChannelSegmentType(i, cc);
      if (segType == math::CurveSegmentType::BEZIER) {
        float hx = _tangentOutScreenX(i);
        float hy = _tangentOutScreenY(i, ch);
        float dx = mx - hx;
        float dy = my - hy;
        if (dx * dx + dy * dy < float(TANGENT_HIT_RADIUS * TANGENT_HIT_RADIUS)) {
          outMode = DRAG_TANGENT_OUT;
          return i;
        }
      }
    }
    // tangent_in: check if segment to left is BEZIER for this channel
    if (i > 0) {
      auto segType = _curve->getChannelSegmentType(i - 1, cc);
      if (segType == math::CurveSegmentType::BEZIER) {
        float hx = _tangentInScreenX(i);
        float hy = _tangentInScreenY(i, ch);
        float dx = mx - hx;
        float dy = my - hy;
        if (dx * dx + dy * dy < float(TANGENT_HIT_RADIUS * TANGENT_HIT_RADIUS)) {
          outMode = DRAG_TANGENT_IN;
          return i;
        }
      }
    }
  }
  return -1;
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
  int bx = _geometry._w - BUTTON_H;
  if (localX >= bx && localX < _geometry._w && localY >= 0 && localY < BUTTON_H)
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

int TransformCurveEditor::_hitTestLoopToggle(int localX, int localY) const {
  int sidebarX = _geometry._w - SIDEBAR_W;
  if (localX < sidebarX || localX >= _geometry._w)
    return 0;
  int toggleY = TOOLBAR_H + CH_COUNT * ROW_H + ROW_H + 8;
  if (localY >= toggleY && localY < toggleY + ROW_H)
    return 1;
  return 0;
}

int TransformCurveEditor::_hitTestResetButton(int localX, int localY) const {
  // Reset button is left of close button: [R][X]
  int bx = _geometry._w - BUTTON_H * 2;
  if (localX >= bx && localX < bx + BUTTON_H && localY >= 0 && localY < BUTTON_H)
    return 1;
  return 0;
}

int TransformCurveEditor::_hitTestCycleButton(int localX, int localY) const {
  // Cycle button [S] is left of reset button, only visible when point selected
  if (_selectedPointIndex < 0) return 0;
  int bx = _geometry._w - BUTTON_H * 3;
  if (localX >= bx && localX < bx + BUTTON_H && localY >= 0 && localY < BUTTON_H)
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
      // Only handle key commands when mouse is within widget bounds
      bool mouseInWidget = localX >= 0 && localX < _geometry._w
                        && localY >= 0 && localY < _geometry._h;
      if (!mouseInWidget) break;

      int key = ev->miKeyCode;
      if (key == 256) { // ESC
        if (_onClose) _onClose();
        rval.setHandled(this);
      } else if (key == 261 || key == 259) { // Delete or Backspace
        if (_selectedPointIndex >= 0 && _curve && _curve->numPoints() > 2) {
          _curve->removePoint(_selectedPointIndex);
          _selectedPointIndex = -1;
          _curve->enforceLoopConstraints();
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
      } else if (key == 83 && !_sKeyDown) { // 'S' — cycle segment type
        _sKeyDown = true;
        _cycleSegmentType();
        rval.setHandled(this);
      } else if (key == 32 && !_spaceKeyDown) { // Space — debug scrub
        _spaceKeyDown = true;
        if (_curve) {
          float t = _screenXToTime(float(ev->miX));
          _curve->_debugScrubTime = t;
          if (_onCurveChanged) _onCurveChanged();
        }
        _navPrevRootX = ev->miX;
        rval.setHandled(this);
      } else if (key == 65 && !_aKeyDown) { // 'A' — add point at mouse position
        _aKeyDown = true;
        if (_isInPlotArea(localX, localY) && _curve) {
          float t = _screenXToTime(float(ev->miX));
          float v = _screenYToValue(float(ev->miY));

          auto sample = _curve->sample(t);
          auto newPt = std::make_shared<math::TransformCurvePoint>();
          newPt->_time = t;
          newPt->_position = sample._position;
          newPt->_eulerRotation = _curve->sampleEuler(t);
          newPt->_scale = sample._scale;

          int idx = _curve->addPoint(newPt);
          _setChannelValue(_activeEditChannel, idx, v);
          _selectedPointIndex = idx;

          _curve->enforceLoopConstraints();
          if (_onCurveChanged) _onCurveChanged();
        }
        rval.setHandled(this);
      }
      break;
    }
    case EventCode::KEY_UP: {
      int key = ev->miKeyCode;
      // Always release state for keys we captured, to avoid stuck keys
      if (key == 88 && _xKeyDown) {
        _xKeyDown = false;
        rval.setHandled(this);
      } else if (key == 67 && _cKeyDown) {
        _cKeyDown = false;
        rval.setHandled(this);
      } else if (key == 65 && _aKeyDown) {
        _aKeyDown = false;
        rval.setHandled(this);
      } else if (key == 83 && _sKeyDown) {
        _sKeyDown = false;
        rval.setHandled(this);
      } else if (key == 32 && _spaceKeyDown) {
        _spaceKeyDown = false;
        if (_curve) {
          _curve->_debugScrubTime = -1.0f;
          if (_onCurveChanged) _onCurveChanged();
        }
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
      // Cycle segment type button
      if (_hitTestCycleButton(localX, localY)) {
        _cycleSegmentType();
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
      // Loop toggle
      if (_hitTestLoopToggle(localX, localY)) {
        _curve->_looping = !_curve->_looping;
        if (_curve->_looping) {
          _curve->enforceLoopConstraints();
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
      // Plot area: hit-test tangent handles first (they may overlap control points)
      {
        DragMode tangentMode = DRAG_NONE;
        int tangentIdx = _hitTestTangentHandle(localX, localY, tangentMode);
        if (tangentIdx >= 0) {
          _dragMode = tangentMode;
          _dragTangentPointIndex = tangentIdx;
          rval.setHandled(this);
          break;
        }
      }
      // Hit-test control points of active edit channel
      {
        int idx = _hitTestPoint(localX, localY);
        if (idx >= 0) {
          _selectedPointIndex = idx;
          // Shift+drag on last point → time stretch
          int lastIdx = _curve->numPoints() - 1;
          if (ev->mbSHIFT && idx == lastIdx && lastIdx > 0) {
            _dragMode = DRAG_TIME_STRETCH;
            _stretchOriginalTimes.resize(_curve->numPoints());
            for (int i = 0; i < _curve->numPoints(); i++)
              _stretchOriginalTimes[i] = _curve->getPoint(i)->_time;
          } else if (ev->mbALT) {
            // Alt+drag any point → shift all points vertically
            _dragMode = DRAG_VALUE_SHIFT;
            int n = _curve->numPoints();
            _shiftOriginalValues.resize(n);
            for (int i = 0; i < n; i++)
              _shiftOriginalValues[i] = _getChannelValue(_activeEditChannel, i);
            _shiftAnchorValue = _screenYToValue(float(ev->miY));
          } else {
            _dragMode = DRAG_POINT;
          }
          rval.setHandled(this);
        } else {
          _selectedPointIndex = -1;
          rval.setHandled(this);
        }
      }
      break;
    }
    case EventCode::DRAG: {
      if (_dragMode == DRAG_TIME_STRETCH && _selectedPointIndex >= 0 && _curve) {
        float newEndTime = _screenXToTime(float(ev->miX));
        newEndTime = std::max(newEndTime, _stretchOriginalTimes[0] + 0.01f);  // don't collapse past start
        float origStart = _stretchOriginalTimes[0];
        float origEnd = _stretchOriginalTimes.back();
        float origDuration = origEnd - origStart;
        if (origDuration > 1e-9f) {
          float newDuration = newEndTime - origStart;
          float scale = newDuration / origDuration;
          int n = _curve->numPoints();
          for (int i = 1; i < n; i++) {
            float t = origStart + (_stretchOriginalTimes[i] - origStart) * scale;
            _curve->getPoint(i)->_time = t;
          }
          _selectedPointIndex = n - 1;  // keep last point selected
          _curve->enforceLoopConstraints();
          if (_onCurveChanged) _onCurveChanged();
        }
        rval.setHandled(this);
      } else if (_dragMode == DRAG_VALUE_SHIFT && _curve) {
        float mouseValue = _screenYToValue(float(ev->miY));
        float delta = mouseValue - _shiftAnchorValue;
        int n = _curve->numPoints();
        for (int i = 0; i < n; i++)
          _setChannelValue(_activeEditChannel, i, _shiftOriginalValues[i] + delta);
        _curve->enforceLoopConstraints();
        if (_onCurveChanged) _onCurveChanged();
        rval.setHandled(this);
      } else if (_dragMode == DRAG_POINT && _selectedPointIndex >= 0 && _curve) {
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

        _curve->enforceLoopConstraints();
        if (_onCurveChanged) _onCurveChanged();
        rval.setHandled(this);
      } else if ((_dragMode == DRAG_TANGENT_OUT || _dragMode == DRAG_TANGENT_IN)
                 && _dragTangentPointIndex >= 0 && _curve) {
        int ch = _activeEditChannel;
        float mouseValue = _screenYToValue(float(ev->miY));
        float ptValue = _getChannelValue(ch, _dragTangentPointIndex);
        float offset = mouseValue - ptValue;
        if (_dragMode == DRAG_TANGENT_OUT) {
          _setTangentOutComponent(ch, _dragTangentPointIndex, offset);
        } else {
          _setTangentInComponent(ch, _dragTangentPointIndex, offset);
        }
        _curve->enforceLoopConstraints();
        if (_onCurveChanged) _onCurveChanged();
        rval.setHandled(this);
      }
      break;
    }
    case EventCode::RELEASE: {
      _dragMode = DRAG_NONE;
      _dragTangentPointIndex = -1;
      rval.setHandled(this);
      break;
    }
    case EventCode::MOVE: {
      // Space held → debug scrub
      if (_spaceKeyDown && _curve) {
        float t = _screenXToTime(float(ev->miX));
        _curve->_debugScrubTime = t;
        if (_onCurveChanged) _onCurveChanged();
        rval.setHandled(this);
        break;
      }
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

        _curve->enforceLoopConstraints();
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

  // Toolbar background (full TOOLBAR_H)
  _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
            float(rx), float(ry), float(rx + _geometry._w), float(ry + TOOLBAR_H),
            fvec3(0.2f, 0.2f, 0.25f));

  // Reset view button [R] — left of close button (BUTTON_H height)
  _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
            float(rx + _geometry._w - BUTTON_H * 2), float(ry),
            float(rx + _geometry._w - BUTTON_H), float(ry + BUTTON_H),
            fvec3(0.25f, 0.35f, 0.5f));

  // Close button [X] (BUTTON_H height)
  _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
            float(rx + _geometry._w - BUTTON_H), float(ry),
            float(rx + _geometry._w), float(ry + BUTTON_H),
            fvec3(0.7f, 0.2f, 0.2f));

  // Cycle button [S] — visible when point selected (left of reset)
  if (_selectedPointIndex >= 0) {
    _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
              float(rx + _geometry._w - BUTTON_H * 3), float(ry),
              float(rx + _geometry._w - BUTTON_H * 2), float(ry + BUTTON_H),
              fvec3(0.35f, 0.4f, 0.25f));
  }

  // Title + button text (first row)
  lev2::FontMan::PushFont("i14");
  lev2::FontMan::beginTextBlock(ctx, 256);
  lev2::FontMan::DrawText(ctx, rx + 8, ry + 4, "TransformCurve Editor");
  lev2::FontMan::DrawText(ctx, rx + _geometry._w - BUTTON_H * 2 + 6, ry + 4, "R");
  lev2::FontMan::DrawText(ctx, rx + _geometry._w - BUTTON_H + 6, ry + 4, "X");
  if (_selectedPointIndex >= 0) {
    lev2::FontMan::DrawText(ctx, rx + _geometry._w - BUTTON_H * 3 + 6, ry + 4, "S");
  }
  lev2::FontMan::endTextBlock(ctx);
  lev2::FontMan::PopFont();

  // Second row: point info when selected
  if (_selectedPointIndex >= 0 && _curve && _selectedPointIndex < _curve->numPoints()) {
    auto pt = _curve->getPoint(_selectedPointIndex);
    int ch = _activeEditChannel;
    float chartVal = _getChannelValue(ch, _selectedPointIndex);
    bool isRot = (ch >= CH_ROT_X && ch <= CH_ROT_Z);
    float displayVal = isRot ? (chartVal / ROT_CHART_SCALE) : chartVal;

    // Segment type label (per-channel)
    const char* segLabel = "---";
    int segIdx = _selectedPointIndex;
    int numSegs = _curve->numPoints() - 1;
    if (segIdx < numSegs) {
      auto cc = _editorChannelToCurveChannel(ch);
      auto segType = _curve->getChannelSegmentType(segIdx, cc);
      switch (segType) {
        case math::CurveSegmentType::LINEAR: segLabel = "LIN"; break;
        case math::CurveSegmentType::BEZIER: segLabel = "BEZ"; break;
        case math::CurveSegmentType::CATMULL_ROM: segLabel = "C-R"; break;
        case math::CurveSegmentType::STEP: segLabel = "STP"; break;
      }
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "Pt%d t:%.3f val:%.2f seg:%s",
             _selectedPointIndex, pt->_time, displayVal, segLabel);

    lev2::FontMan::PushFont("i12");
    lev2::FontMan::beginTextBlock(ctx, 128);
    lev2::FontMan::DrawText(ctx, rx + 8, ry + 18, buf);
    lev2::FontMan::endTextBlock(ctx);
    lev2::FontMan::PopFont();
  }
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

  // Vertical endpoint line (time of last point)
  if (_curve && _curve->numPoints() >= 2) {
    float tEnd = _curve->getPoint(_curve->numPoints() - 1)->_time;
    if (_timeMin <= tEnd && _timeMax >= tEnd) {
      float sx = _timeToScreenX(tEnd);
      _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
                sx - thickness * 0.5f, float(r.y),
                sx + thickness * 0.5f, float(r.y + r.h),
                originColor);
    }
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

static void _drawLine(
    lev2::Context* ctx, lev2::rcfd_ptr_t RCFD,
    lev2::freestyle_mtl_ptr_t mtl,
    const lev2::FxShaderTechnique* tek,
    const lev2::FxShaderParam* parmvp,
    const fmtx4& uiMtx,
    float x0, float y0, float x1, float y1,
    const fvec3& color) {
  float dx = x1 - x0;
  float dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.5f) return;
  float nx = -dy / len;
  float ny = dx / len;

  auto gbi = ctx->GBI();
  using vtx_t = lev2::SVtxV16T16C16;
  auto vb = lev2::GfxEnv::GetSharedDynamicV16T16C16();
  lev2::VtxWriter<vtx_t> vw;
  vw.Lock(ctx, vb.get(), 6);
  vtx_t v0(fvec3(x0 + nx, y0 + ny, 0), fvec4(), color);
  vtx_t v1(fvec3(x1 + nx, y1 + ny, 0), fvec4(), color);
  vtx_t v2(fvec3(x0 - nx, y0 - ny, 0), fvec4(), color);
  vtx_t v3(fvec3(x1 - nx, y1 - ny, 0), fvec4(), color);
  vw.AddVertex(v0); vw.AddVertex(v1); vw.AddVertex(v2);
  vw.AddVertex(v1); vw.AddVertex(v3); vw.AddVertex(v2);
  vw.UnLock(ctx);
  mtl->begin(tek, RCFD);
  mtl->bindParamMatrix(parmvp, uiMtx);
  gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
  mtl->end(RCFD);
}

void TransformCurveEditor::_drawTangentHandles(
    lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, int channel, const fmtx4& uiMtx) {
  if (!_curve) return;
  int numPts = _curve->numPoints();
  if (numPts < 2) return;

  auto cc = _editorChannelToCurveChannel(channel);

  fvec3 colorOut(0.3f, 0.9f, 0.9f);  // cyan for tangent_out
  fvec3 colorIn(0.9f, 0.3f, 0.9f);   // magenta for tangent_in
  float handleHalf = 8.0f;

  for (int i = 0; i < numPts; i++) {
    float ptSx = _timeToScreenX(_curve->getPoint(i)->_time);
    float ptSy = _valueToScreenY(_getChannelValue(channel, i));

    // tangent_out handle
    if (i + 1 < numPts) {
      auto segType = _curve->getChannelSegmentType(i, cc);
      if (segType == math::CurveSegmentType::BEZIER) {
        float hx = _tangentOutScreenX(i);
        float hy = _tangentOutScreenY(i, channel);

        _drawLine(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
                  ptSx, ptSy, hx, hy, colorOut);

        _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
                  hx - handleHalf, hy - handleHalf,
                  hx + handleHalf, hy + handleHalf,
                  colorOut);
      }
    }

    // tangent_in handle
    if (i > 0) {
      auto segType = _curve->getChannelSegmentType(i - 1, cc);
      if (segType == math::CurveSegmentType::BEZIER) {
        float hx = _tangentInScreenX(i);
        float hy = _tangentInScreenY(i, channel);

        _drawLine(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
                  ptSx, ptSy, hx, hy, colorIn);

        _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
                  hx - handleHalf, hy - handleHalf,
                  hx + handleHalf, hy + handleHalf,
                  colorIn);
      }
    }
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

  // Loop toggle
  int loopY = toggleY + ROW_H + 4;
  bool looping = _curve ? _curve->_looping : false;
  fvec3 loopColor = looping ? fvec3(0.5f, 0.5f, 0.8f) : fvec3(0.3f, 0.3f, 0.3f);
  _drawQuad(ctx, RCFD, _material, _tekvtxcolor, _parmvp, uiMtx,
            float(sidebarX + 4), float(loopY + 4),
            float(sidebarX + 16), float(loopY + 16),
            loopColor);

  lev2::FontMan::beginTextBlock(ctx, 32);
  lev2::FontMan::DrawText(ctx, sidebarX + 20, loopY + 2, "Loop");
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

  // Scissor plot area to prevent curves from drawing outside
  auto fbi = context->FBI();
  auto pr = _cachedPlotRect;
  fbi->pushScissor(pr.x, pr.y, pr.w, pr.h);

  _drawGrid(context, RCFD, uiMatrix);
  _drawOriginLines(context, RCFD, uiMatrix);

  // Draw debug scrub indicator
  if (_curve && _curve->_debugScrubTime >= 0.0f) {
    float sx = _timeToScreenX(_curve->_debugScrubTime);
    if (sx >= float(pr.x) && sx <= float(pr.x + pr.w)) {
      _drawQuad(context, RCFD, _material, _tekvtxcolor, _parmvp, uiMatrix,
                sx - 1.0f, float(pr.y), sx + 1.0f, float(pr.y + pr.h),
                fvec3(1.0f, 0.4f, 0.1f));
    }
  }

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
    _drawTangentHandles(context, RCFD, _activeEditChannel, uiMatrix);
  }

  fbi->popScissor();

  _drawSidebar(context, RCFD, uiMatrix);

  mtxi->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
