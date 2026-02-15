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
// Shows X/Y/Z position channels as stacked subplots with
// draggable control points.
////////////////////////////////////////////////////////////////////

struct TransformCurveEditor final : public Widget {
public:
  TransformCurveEditor(const std::string& name, math::transformcurve_ptr_t curve);

  HandlerResult DoOnUiEvent(event_constptr_t ev) final;
  void DoDraw(ui::drawevent_constptr_t drwev) override;

  math::transformcurve_ptr_t _curve;

  // Selection/drag state
  int _selectedPointIndex = -1;
  int _dragChannel = -1;        // 0=X, 1=Y, 2=Z
  bool _dragging = false;

  // View ranges
  float _timeMin = 0.0f, _timeMax = 10.0f;
  float _valueMin = -5.0f, _valueMax = 5.0f;

  // Callbacks
  std::function<void()> _onClose;
  std::function<void()> _onCurveChanged;

  void autoFitRanges();

private:
  static constexpr int TOOLBAR_H = 24;
  static constexpr int HIT_RADIUS = 6;
  static constexpr int NUM_CHANNELS = 3;
  static constexpr int CURVE_SEGMENTS = 100;

  struct SubplotRect {
    int x, y, w, h;
  };

  SubplotRect _subplotRect(int channel) const;

  float _timeToScreenX(float t, const SubplotRect& r) const;
  float _valueToScreenY(float v, const SubplotRect& r) const;
  float _screenXToTime(float sx, const SubplotRect& r) const;
  float _screenYToValue(float sy, const SubplotRect& r) const;

  int _hitTestCloseButton(int localX, int localY) const;
  int _hitTestPoint(int localX, int localY, int& outChannel) const;
  int _channelForLocalY(int localY) const;

  void _drawSubplot(lev2::Context* ctx, lev2::rcfd_ptr_t RCFD, int channel,
                    int rx, int ry, const fmtx4& uiMatrix);
  void _drawToolbar(lev2::Context* ctx, lev2::rcfd_ptr_t RCFD,
                    int rx, int ry, const fmtx4& uiMatrix);

  // Lazy-init material
  ork::lev2::freestyle_mtl_ptr_t _material;
  const ork::lev2::FxShaderTechnique* _tekvtxcolor = nullptr;
  const ork::lev2::FxShaderParam* _parmvp = nullptr;
};

using transformcurveeditor_ptr_t = std::shared_ptr<TransformCurveEditor>;

} // namespace ork::ui
