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
  size_t _max_samples = 100;
  fvec3 _color;
  bool _visible = true;

  // Auto-range tracking
  float _min_value = 0.0f;
  float _max_value = 1.0f;
  bool _auto_range = true;

  // Moving window display (0 = show all samples, >0 = show only most recent N)
  size_t _window_size = 0;

private:
  void _updateRange();
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
  bool _lockX;
  bool _lockY;
  bool _lockYZOOM;
  bool _dragging;
};
using graphview_ptr_t = std::shared_ptr<GraphView>;

} // namespace ork::ui
