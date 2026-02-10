////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/lev2/gfx/util/grid.h>
#include <deque>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////
struct GraphPanel {
  void setRect(int iX, int iY, int iW, int iH, bool snap = false);
  ui::anchor::layout_ptr_t _panelLayout;
  ui::layoutgroup_ptr_t _layoutgroup;
  ui::panel_ptr_t _uipanel;
  ui::surface_ptr_t _uisurface;
};
///////////////////////////////////////////////////////////////////////////////
// GraphSeries: Stores time-series data in a ring buffer
// Automatically calculates range (min/max) and handles sample management
///////////////////////////////////////////////////////////////////////////////
struct GraphSeries {
  GraphSeries(const std::string& name, fvec3 color);

  void addSample(float value);
  void clearSamples();
  void setMaxSamples(size_t count);
  size_t sampleCount() const { return _samples.size(); }
  float getSample(size_t index) const;

  std::string _name;
  std::deque<float> _samples;
  size_t _max_samples;
  fvec3 _color;
  bool _visible = true;

  // Auto-range tracking (blended at 3% per frame)
  float _min_value = 0.0f;
  float _max_value = 1.0f;
  float _blend_rate = 0.5f;

  // Optional fixed range (overrides auto-range when enabled)
  bool _use_fixed_range = false;
  float _fixed_min = 0.0f;
  float _fixed_max = 1.0f;

  // Moving window display (0 = show all samples, >0 = show only most recent N)
  size_t _window_size = 0;

  // Display normalization (visual only, doesn't affect stored data)
  bool _normalize_for_display = false;  // Normalize to [0,1] range for display

  // Per-series vertical scale (visual only)
  float _vertical_scale = 1.0f;   // Multiplier for Y values (zoom)
};
using graphseries_ptr_t = std::shared_ptr<GraphSeries>;
///////////////////////////////////////////////////////////////////////////////
// GraphChannel: Can use either lambda-based or series-based data
// Lambda mode: backward compatible with existing code
// Series mode: new simplified API with internal data storage
///////////////////////////////////////////////////////////////////////////////
struct GraphChannel {
  // Lambda-based system (backward compat)
  using range_lambda_t               = std::function<fvec2()>;
  using count_lambda_t               = std::function<size_t()>;
  using indexed_point_lambda_t       = std::function<fvec2(size_t)>;
  range_lambda_t _getVerticalRange   = nullptr;
  range_lambda_t _getHorizontalRange = nullptr;
  count_lambda_t _getCount           = nullptr;
  indexed_point_lambda_t _getPoint   = nullptr;

  // Series-based system (new)
  std::vector<graphseries_ptr_t> _series;
  graphseries_ptr_t addSeries(const std::string& name, fvec3 color);
  void removeSeries(const std::string& name);
  graphseries_ptr_t getSeries(const std::string& name);
  void setSeriesOrder(const std::vector<std::string>& names);

  std::string _name;
  fvec3 _color;
  bool _visible = true;

  // Stacked bar mode: all series rendered as stacked bars in one lane
  bool _stacked = false;
  float _min_series_height = 2.0f;  // minimum pixel height per series band

  // Per-lane styling
  fvec4 _lane_bgcolor = fvec4(0, 0, 0, 0);  // transparent by default
  bool _lane_outline = false;
  fvec3 _lane_outline_color = fvec3(0.4f, 0.4f, 0.4f);
};
using graphchannel_ptr_t = std::shared_ptr<GraphChannel>;
///////////////////////////////////////////////////////////////////////////////
struct GraphView : public ui::Surface {
  GraphView();
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) override;
  void _doGpuInit(lev2::Context* pt) override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;

  graphchannel_ptr_t channel(std::string named);

  std::vector<graphchannel_ptr_t> _channelmap;
  lev2::Grid2d _grid;
  fvec2 _downPos;
  fvec2 _downCenter;
  bool _lockX;
  bool _lockY;
  bool _lockYZOOM;
  bool _dragging;
  bool _show_stats = true;

  int _label_spacing = 2;  // Margin between series label boxes

  // Selected series for per-series scale control
  graphseries_ptr_t _selected_series = nullptr;
  graphseries_ptr_t _hovered_series = nullptr;  // Series under mouse cursor

  // Key state tracking
  bool _v_key_down = false;  // True when 'v' key is held down

private:
  // Helper functions for event handling
  graphseries_ptr_t _findSeriesAtPoint(int x, int y);
  void _adjustSeriesScale(int wheel_delta);
  void _adjustGlobalZoom(int wheel_delta);
};
using graphview_ptr_t = std::shared_ptr<GraphView>;

} // namespace ork::ui
