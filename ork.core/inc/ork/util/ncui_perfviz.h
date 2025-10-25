#pragma once

#include <ork/util/ncui.h>
#if defined(ENABLE_NOTCURSES_UI)
#include <ork/util/ringbuffer.inl>
#include <ork/kernel/timer.h>
#include <functional>
#include <vector>
#include <memory>
#include <string>

namespace ork::notcurses {

////////////////////////////////////////////////////////////////
// Data item with lambda providers
////////////////////////////////////////////////////////////////

struct DataItem {
  enum Type { Double, Int, String };
  
  Type type;
  std::string display_name;
  std::string units;
  bool show_sparkline = true;
  
  // Lambda providers - only one will be used based on type
  std::function<double()> double_provider;
  std::function<int()> int_provider; 
  std::function<std::string()> string_provider;
  
  // History for sparklines
  RingBuffer<float> history{40}; // 10 seconds @ 0.25s
  
  // Current cached values
  double double_value = 0.0;
  int int_value = 0;
  std::string string_value = "";
};

using dataitem_ptr_t = std::shared_ptr<DataItem>;

////////////////////////////////////////////////////////////////
// Performance data source with lambda providers
////////////////////////////////////////////////////////////////

struct PerformanceDataSource {
  std::string name;
  using item_pair_t = std::pair<std::string, dataitem_ptr_t>;
  std::vector<item_pair_t> _items_by_order;
  std::map<std::string, dataitem_ptr_t> _items_by_name;

  // Factory methods for creating data items
  static dataitem_ptr_t makeDoubleItem(const std::string& display_name, 
                                       const std::string& units,
                                       std::function<double()> provider,
                                       bool sparkline = true);
  
  static dataitem_ptr_t makeIntItem(const std::string& display_name,
                                    const std::string& units, 
                                    std::function<int()> provider,
                                    bool sparkline = true);
                                    
  static dataitem_ptr_t makeStringItem(const std::string& display_name,
                                       std::function<std::string()> provider);
  
  void addItem(const std::string& key, dataitem_ptr_t item);
  dataitem_ptr_t findItem(const std::string& key);

    void updateAll(); // Call all lambdas, update histories
};

using perfdatasource_ptr_t = std::shared_ptr<PerformanceDataSource>;

////////////////////////////////////////////////////////////////
// Generic performance visualizer widget
////////////////////////////////////////////////////////////////

struct PerformanceVisualizerWidget : public Widget {
  // Public members - no accessors needed
  perfdatasource_ptr_t data_source;
  std::string title;
  float update_interval = 0.25f;
  int sparkline_width = 20; // Configurable sparkline width (default 20 chars)
  
  // Visual styling
  ork::fvec3 border_color = ork::fvec3::Blue() * 0.5f;
  ork::fvec3 background_color = ork::fvec3::Blue() * 0.3f;
  ork::fvec3 title_color = ork::fvec3::White();
  ork::fvec3 text_color = ork::fvec3::White();
  ork::fvec3 value_color = ork::fvec3::Green() * 0.8f;
  
  // Internal state
  Timer update_timer;
  float last_update = 0.0f;
  
  PerformanceVisualizerWidget();
  ~PerformanceVisualizerWidget();
  
  void _doDraw() override;
  void _onLayoutChanged() override; 
  void _onInput(uint32_t c, struct ncinput ni) override;
  
  // Utility methods
  std::string _generateSparkline(const RingBuffer<float>& history, int width);
  void _updateData();
  int _calculateRequiredHeight();
};

using perfviz_ptr_t = std::shared_ptr<PerformanceVisualizerWidget>;

} // namespace ork::notcurses 
#endif