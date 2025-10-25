////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/ncui.h>
#if defined(ENABLE_NOTCURSES_UI)
#include <ork/kernel/opq.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/string/deco.inl>
#include <thread>
#include <atomic>
#include <cstdio>
#include <errno.h>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <sstream>
#include <iomanip>

namespace ork::notcurses {

////////////////////////////////////////////////////////////////
// OPQVisualizerWidget implementation
////////////////////////////////////////////////////////////////

OPQVisualizerWidget::OPQVisualizerWidget() {
  _name = "OPQVisualizerWidget";
  _update_timer.Start();
  _width = 64;  // Increased width for sparklines
  _height = 9;
  _perf_update_interval = 0.25f;  // Default 0.5 seconds
  _last_perf_update = 0.0f;
}

////////////////////////////////////////////////////////////////

OPQVisualizerWidget::~OPQVisualizerWidget() {
  // Cleanup if needed
}

////////////////////////////////////////////////////////////////

void OPQVisualizerWidget::setTargetOPQ(opq::opq_ptr_t opq_ptr) {
  _target_opq = opq_ptr;
}

void OPQVisualizerWidget::setUpdateInterval(float interval) {
  _perf_update_interval = interval;
}

////////////////////////////////////////////////////////////////

std::string OPQVisualizerWidget::_generateSparkline(const RingBuffer<float>& history, float min_override, float max_override) {
  const char* sparkline_chars[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
  
  if (history.size() == 0) {
    return "▁▁▁▁▁▁▁▁▁▁"; // Default 10 chars when no data
  }
  
  // Extract values from ring buffer
  std::vector<float> values;
  RingBuffer<float> temp_buffer = history;
  float sample;
  while (temp_buffer.try_pop(sample)) {
    values.push_back(sample);
  }
  
  if (values.empty()) {
    return "▁▁▁▁▁▁▁▁▁▁";
  }
  
  // Find min/max for scaling
  float min_val = (min_override >= 0) ? min_override : *std::min_element(values.begin(), values.end());
  float max_val = (max_override >= 0) ? max_override : *std::max_element(values.begin(), values.end());
  
  // Handle edge case where all values are the same
  if (max_val <= min_val) {
    max_val = min_val + 1.0f;
  }
  
  std::string result;
  result.reserve(values.size());
  
  for (float val : values) {
    // Scale to 0-7 range
    float normalized = (val - min_val) / (max_val - min_val);
    int level = std::clamp(int(normalized * 7.0f), 0, 7);
    result += sparkline_chars[level];
  }
  
  return result;
}

std::string OPQVisualizerWidget::_generateSparklineInt(const RingBuffer<int>& history, int min_override, int max_override) {
  const char* sparkline_chars[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
  
  if (history.size() == 0) {
    return "▁▁▁▁▁▁▁▁▁▁"; // Default 10 chars when no data
  }
  
  // Extract values from ring buffer
  std::vector<int> values;
  RingBuffer<int> temp_buffer = history;
  int sample;
  while (temp_buffer.try_pop(sample)) {
    values.push_back(sample);
  }
  
  if (values.empty()) {
    return "▁▁▁▁▁▁▁▁▁▁";
  }
  
  // Find min/max for scaling
  int min_val = (min_override >= 0) ? min_override : *std::min_element(values.begin(), values.end());
  int max_val = (max_override >= 0) ? max_override : *std::max_element(values.begin(), values.end());
  
  // Handle edge case where all values are the same
  if (max_val <= min_val) {
    max_val = min_val + 1;
  }
  
  std::string result;
  result.reserve(values.size());
  
  for (int val : values) {
    // Scale to 0-7 range
    float normalized = float(val - min_val) / float(max_val - min_val);
    int level = std::clamp(int(normalized * 7.0f), 0, 7);
    result += sparkline_chars[level];
  }
  
  return result;
}

////////////////////////////////////////////////////////////////

void OPQVisualizerWidget::_doDraw() {
  auto ctx = context();

  
  // Fill entire widget area with background color using optimized method
  drawOutlineCharBox(_x, _y, _width, _height, fvec3::Blue()*0.5f, fvec3(), ' ');
  drawFilledBox(_x+1, _y+1, _width-2, _height-2, fvec3::Blue()*0.3f);
  
  // Set colors for text drawing

  // Draw performance metrics with cached data
  if (_target_opq) {
    // Update cached performance data at configured interval
    float current_time = _update_timer.SecsSinceStart();
    if (current_time - _last_perf_update >= _perf_update_interval) {
      _cached_perf_data = _target_opq->getPerformanceData();
      _last_perf_update = current_time;
      
      // Collect historical samples for sparklines
      if (_cached_perf_data) {
        _history_ops_per_sec.push_one(_cached_perf_data->aggregate_ops_per_sec);
        _history_avg_latency.push_one(_cached_perf_data->avg_latency_ms);
        _history_thread_count.push_one(_cached_perf_data->num_threads);
        _history_pending_ops.push_one(_target_opq->_numPendingOperations.load());
      }
    }
    
  _setColors(fvec3::White(), fvec3::Blue()*0.4f);
  std::string title = "OPQ Performance Monitor";
  int title_length = title.length();
  int x = _x + (_width - title_length) / 2; //
    //ncplane_putstr_yx(ctx->_stdplane, _y, x, title.c_str());
  _setColors(fvec3::White(), fvec3::Blue()*0.3f);

    if (_cached_perf_data) {
      // Generate sparklines for historical data
      auto threads_spark = _generateSparklineInt(_history_thread_count, 0, -1);
      auto pending_spark = _generateSparklineInt(_history_pending_ops, 0, -1);
      auto ops_spark = _generateSparkline(_history_ops_per_sec, 0.0f, -1.0f);
      auto latency_spark = _generateSparkline(_history_avg_latency, 0.0f, -1.0f);
      
      // Use cached data for display with sparklines
      auto name_str = FormatString("OPQ: %s", _cached_perf_data->queue_name.c_str());
      auto tc_str = FormatString("Threads:  %s %d", threads_spark.c_str(), _cached_perf_data->num_threads);
      auto po_str = FormatString("Pending:  %s %d", pending_spark.c_str(), _target_opq->_numPendingOperations.load());
      auto ops_str = FormatString("Ops/sec:  %s %.1f", ops_spark.c_str(), _cached_perf_data->aggregate_ops_per_sec);
      auto lat_str = FormatString("Latency:  %s %.2f ms", latency_spark.c_str(), _cached_perf_data->avg_latency_ms);
      auto max_str = FormatString("MaxAvgLatency: %.2f ms", _cached_perf_data->max_latency_ms);
      auto co_str = FormatString("Completed: %d", _target_opq->_numCompletedOperations.load());
      
      int y = _y + 1;
      //ncplane_putstr_yx(ctx->_stdplane, y++, _x + 1, name_str.c_str());
      //ncplane_putstr_yx(ctx->_stdplane, y++, _x + 1, tc_str.c_str());
      //ncplane_putstr_yx(ctx->_stdplane, y++, _x + 1, po_str.c_str());
      //ncplane_putstr_yx(ctx->_stdplane, y++, _x + 1, ops_str.c_str());
      //ncplane_putstr_yx(ctx->_stdplane, y++, _x + 1, lat_str.c_str());
      //ncplane_putstr_yx(ctx->_stdplane, y++, _x + 1, max_str.c_str());
      //ncplane_putstr_yx(ctx->_stdplane, y++, _x + 1, co_str.c_str());
    }
  }
}

////////////////////////////////////////////////////////////////

void OPQVisualizerWidget::_onLayoutChanged() {
  // Widget handles its own sizing
  //formatDisplayLines();
}

////////////////////////////////////////////////////////////////

void OPQVisualizerWidget::_onInput(uint32_t c, struct ncinput ni) {
}

} // namespace ork::notcurses 
#endif