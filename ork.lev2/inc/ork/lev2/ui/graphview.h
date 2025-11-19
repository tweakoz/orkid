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

  // Auto-range tracking
  float _min_value = 0.0f;
  float _max_value = 1.0f;
  bool _auto_range = true;

  // Moving window display (0 = show all samples, >0 = show only most recent N)
  size_t _window_size = 0;

  // Range inertia/momentum (prevents jittery auto-scaling)
  float _range_decay_rate = 0.9998f;  // Decay rate: 0.9998^60 ≈ 0.99 (1% change per second at 60fps)
  float _historical_min = 0.0f;       // Historical min that decays toward current min
  float _historical_max = 1.0f;       // Historical max that decays toward current max
  size_t _range_update_counter = 0;   // Track updates for initialization

  // Display normalization (visual only, doesn't affect stored data)
  bool _normalize_for_display = false;  // Normalize to [0,1] range for display

  // Per-series vertical scale and offset (visual only)
  float _vertical_scale = 1.0f;   // Multiplier for Y values (zoom)
  float _vertical_offset = 0.0f;  // Offset added to Y values (pan)
  bool _freeze_auto_range = false;  // True to freeze this series' range (disable auto-range)
  float _frozen_min = 0.0f;  // Frozen min value when auto-range is disabled
  float _frozen_max = 1.0f;  // Frozen max value when auto-range is disabled

private:
  void _updateRange();
};
using graphseries_ptr_t = std::shared_ptr<GraphSeries>;
///////////////////////////////////////////////////////////////////////////////
enum class VerticalScaleMode : uint64_t {
  CrcEnum(AUTO),    // Auto-range (dynamically fits visible data)
  CrcEnum(MANUAL)   // Manual zoom (user-controlled via mouse wheel)
};
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

  std::string _name;
  fvec3 _color;
  bool _visible = true;
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
  int _downPixelY;  // Starting Y pixel position for drag
  float _downSeriesOffset;  // Starting offset of selected series when drag began
  bool _lockX;
  bool _lockY;
  bool _lockYZOOM;
  bool _dragging;
  bool _show_stats = true;

  int _label_spacing = 2;  // Margin between series label boxes
  VerticalScaleMode _vscale_mode = VerticalScaleMode::AUTO;  // Default to auto-range

  // Selected series for per-series scale/offset control
  graphseries_ptr_t _selected_series = nullptr;
  graphseries_ptr_t _hovered_series = nullptr;  // Series under mouse cursor

  // Key state tracking
  bool _v_key_down = false;  // True when 'v' key is held down

private:
  // Helper functions for event handling
  graphseries_ptr_t _findSeriesAtPoint(int x, int y);
  void _freezeSeriesAutoRange(graphseries_ptr_t series);
  float _getSeriesVerticalRange(graphseries_ptr_t series);
  void _adjustSeriesOffset(int pixel_delta_y);
  void _adjustSeriesScale(int wheel_delta);
  void _adjustGlobalZoom(int wheel_delta);
};
using graphview_ptr_t = std::shared_ptr<GraphView>;

} // namespace ork::ui
