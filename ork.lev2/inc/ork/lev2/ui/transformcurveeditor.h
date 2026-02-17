////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/math/transform_curve.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// TransformCurveEditor — interactive editor for TransformCurve
// Single unified track with all channels overlaid, sidebar with
// visibility toggles and active edit channel selection.
////////////////////////////////////////////////////////////////////

struct TransformCurveEditor final : public Widget {
public:
  TransformCurveEditor(const std::string& name, math::transformcurve_ptr_t curve);

  HandlerResult DoOnUiEvent(event_constptr_t ev) final;
  void DoDraw(ui::drawevent_constptr_t drwev) override;

  math::transformcurve_ptr_t _curve;

  // Channel enum
  enum Channel : int {
    CH_POS_X = 0, CH_POS_Y, CH_POS_Z,
    CH_ROT_X, CH_ROT_Y, CH_ROT_Z,
    CH_SCALE_UNI,
    CH_SCALE_X, CH_SCALE_Y, CH_SCALE_Z,
    CH_COUNT
  };

  // Channel state
  bool _channelVisible[CH_COUNT];
  int _activeEditChannel = CH_POS_X;

  // Selection/drag
  int _selectedPointIndex = -1;
  bool _dragging = false;

  // Navigation: hold X+move = pan, hold C+move = zoom
  bool _xKeyDown = false;
  bool _cKeyDown = false;
  int _navPrevRootX = 0, _navPrevRootY = 0;

  // View ranges
  float _timeMin = 0.0f, _timeMax = 10.0f;
  float _valueMin = -5.0f, _valueMax = 5.0f;

  // Callbacks
  std::function<void()> _onClose;
  std::function<void()> _onCurveChanged;

  void autoFitRanges();

private:
  static constexpr int TOOLBAR_H = 24;
  static constexpr int SIDEBAR_W = 100;
  static constexpr int ROW_H = 20;
  static constexpr int HIT_RADIUS = 6;
  static constexpr int CURVE_SEGMENTS = 100;

  struct PlotRect { int x, y, w, h; };
  PlotRect _plotRect() const;

  // Channel data helpers
  float _getChannelValue(int channel, int pointIndex) const;
  float _sampleChannelValue(int channel, float t) const;
  void _setChannelValue(int channel, int pointIndex, float value);
  bool _isChannelAvailable(int channel) const;

  // Coordinate mapping
  float _timeToScreenX(float t) const;
  float _valueToScreenY(float v) const;
  float _screenXToTime(float sx) const;
  float _screenYToValue(float sy) const;

  // Hit testing
  int _hitTestCloseButton(int lx, int ly) const;
  int _hitTestResetButton(int lx, int ly) const;
  int _hitTestPoint(int lx, int ly) const;
  int _hitTestSidebarCheckbox(int lx, int ly) const;
  int _hitTestSidebarLabel(int lx, int ly) const;
  int _hitTestNonUniformToggle(int lx, int ly) const;
  bool _isInPlotArea(int lx, int ly) const;

  // Drawing
  void _drawPlotBackground(lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, const fmtx4& uiMtx);
  void _drawGrid(lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, const fmtx4& uiMtx);
  void _drawOriginLines(lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, const fmtx4& uiMtx);
  void _drawChannel(lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, int channel, const fmtx4& uiMtx);
  void _drawControlPoints(lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, int channel, const fmtx4& uiMtx);
  void _drawToolbar(lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, const fmtx4& uiMtx);
  void _drawSidebar(lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, const fmtx4& uiMtx);

  // Material
  ork::lev2::freestyle_mtl_ptr_t _material;
  const ork::lev2::FxShaderTechnique* _tekvtxcolor = nullptr;
  const ork::lev2::FxShaderParam* _parmvp = nullptr;

  // Root-space plot rect (cached per frame)
  PlotRect _cachedPlotRect;
};

using transformcurveeditor_ptr_t = std::shared_ptr<TransformCurveEditor>;

} // namespace ork::ui
