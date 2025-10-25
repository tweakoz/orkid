////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/ncui_perfviz.h>
#if defined(ENABLE_NOTCURSES_UI)
#include <ork/kernel/string/deco.inl>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace ork::notcurses {

////////////////////////////////////////////////////////////////
// DataItem factory methods
////////////////////////////////////////////////////////////////

dataitem_ptr_t PerformanceDataSource::makeDoubleItem(const std::string& display_name, 
                                                     const std::string& units,
                                                     std::function<double()> provider,
                                                     bool sparkline) {
  auto item = std::make_shared<DataItem>();
  item->type = DataItem::Double;
  item->display_name = display_name;
  item->units = units;
  item->show_sparkline = sparkline;
  item->double_provider = provider;
  return item;
}

dataitem_ptr_t PerformanceDataSource::makeIntItem(const std::string& display_name,
                                                  const std::string& units, 
                                                  std::function<int()> provider,
                                                  bool sparkline) {
  auto item = std::make_shared<DataItem>();
  item->type = DataItem::Int;
  item->display_name = display_name;
  item->units = units;
  item->show_sparkline = sparkline;
  item->int_provider = provider;
  return item;
}

dataitem_ptr_t PerformanceDataSource::makeStringItem(const std::string& display_name,
                                                     std::function<std::string()> provider) {
  auto item = std::make_shared<DataItem>();
  item->type = DataItem::String;
  item->display_name = display_name;
  item->units = "";
  item->show_sparkline = false;
  item->string_provider = provider;
  return item;
}

void PerformanceDataSource::addItem(const std::string& key, dataitem_ptr_t item) {
  _items_by_order.push_back({key, item});
  _items_by_name.insert({key, item});
}
  dataitem_ptr_t PerformanceDataSource::findItem(const std::string& key) {
  auto it = _items_by_name.find(key);
  if (it != _items_by_name.end()) {
    return it->second;  
  }
  return nullptr; // Item not found
  }

void PerformanceDataSource::updateAll() {
  for (auto& [key, item] : _items_by_order) {
    try {
      switch (item->type) {
        case DataItem::Double:
          if (item->double_provider) {
            item->double_value = item->double_provider();
            if (item->show_sparkline) {
              item->history.push_one(static_cast<float>(item->double_value));
            }
          }
          break;
        case DataItem::Int:
          if (item->int_provider) {
            item->int_value = item->int_provider();
            if (item->show_sparkline) {
              item->history.push_one(static_cast<float>(item->int_value));
            }
          }
          break;
        case DataItem::String:
          if (item->string_provider) {
            item->string_value = item->string_provider();
          }
          break;
      }
    } catch (...) {
      // Ignore lambda exceptions - keep widget stable
    }
  }
}

////////////////////////////////////////////////////////////////
// PerformanceVisualizerWidget implementation
////////////////////////////////////////////////////////////////

PerformanceVisualizerWidget::PerformanceVisualizerWidget() {
  _name = "PerformanceVisualizerWidget";
  _width = 64;
  _height = 8; // Will be adjusted based on data items
  update_timer.Start();
}

PerformanceVisualizerWidget::~PerformanceVisualizerWidget() {
  // Cleanup if needed
}

void PerformanceVisualizerWidget::_updateData() {
  if (!data_source) return;
  
  float current_time = update_timer.SecsSinceStart();
  if (current_time - last_update >= update_interval) {
    data_source->updateAll();
    last_update = current_time;
  }
}

int PerformanceVisualizerWidget::_calculateRequiredHeight() {
  if (!data_source) return 3; // Minimum height for empty widget
  
  // Title line + border + data items + bottom border
  return 3 + static_cast<int>(data_source->_items_by_order.size());
}

std::string PerformanceVisualizerWidget::_generateSparkline(const RingBuffer<float>& history, int width) {
  const char* sparkline_chars[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
  
  if (history.size() == 0) {
    // Return default sparkline of requested width
    return std::string(width, sparkline_chars[0][0]);
  }
  
  // Extract values from ring buffer
  std::vector<float> values;
  RingBuffer<float> temp_buffer = history;
  float sample;
  while (temp_buffer.try_pop(sample)) {
    values.push_back(sample);
  }
  
  if (values.empty()) {
    return std::string(width, sparkline_chars[0][0]);
  }
  
  // Find min/max for scaling
  float min_val = *std::min_element(values.begin(), values.end());
  float max_val = *std::max_element(values.begin(), values.end());
  
  // Handle edge case where all values are the same
  if (max_val <= min_val) {
    max_val = min_val + 1.0f;
  }
  
  std::string result;
  
  // If we have more data than width, sample evenly
  if (static_cast<int>(values.size()) > width) {
    result.reserve(width);
    for (int i = 0; i < width; ++i) {
      int index = (i * values.size()) / width;
      float val = values[index];
      float normalized = (val - min_val) / (max_val - min_val);
      int level = std::clamp(int(normalized * 7.0f), 0, 7);
      result += sparkline_chars[level];
    }
  } else {
    // Use all values and pad if necessary
    result.reserve(width);
    for (float val : values) {
      float normalized = (val - min_val) / (max_val - min_val);
      int level = std::clamp(int(normalized * 7.0f), 0, 7);
      result += sparkline_chars[level];
    }
    // Pad with the minimum sparkline character if needed
    while (static_cast<int>(result.length()) < width) {
      result += sparkline_chars[0];
    }
  }
  
  return result;
}

void PerformanceVisualizerWidget::_doDraw() {
  auto ctx = context();
  if (!ctx || !ctx->_stdplane) return;
  
  // Update data first
  _updateData();
  
  // Adjust height based on data items
  //int required_height = _calculateRequiredHeight();
  //if (_height != required_height) {
    //_height = required_height;
  //}
  
  // Draw border and background
  drawOutlineCharBox(_x, _y, _width, _height, border_color, fvec3(), ' ');
  drawFilledBox(_x + 1, _y + 1, _width - 2, _height - 2, background_color);
  
  if (!data_source) {
    // Draw empty state
    _setColors(text_color, background_color);
    //ncplane_putstr_yx(ctx->_stdplane, _y + 1, _x + 2, "No data source");
    return;
  }
  
  // Draw centered title
  _setColors(title_color, border_color);
  int title_length = static_cast<int>(title.length());
  int title_x = _x + (_width - title_length) / 2;
  //ncplane_putstr_yx(ctx->_stdplane, _y, title_x, title.c_str());
  
  // Draw data items
  _setColors(text_color, background_color);
  int current_y = _y + 1;
  
  for (auto it : data_source->_items_by_name) {
    auto key = it.first;
    auto item = it.second;

    if (current_y >= _y + _height - 1) break; // Don't overflow widget
    
    constexpr int max_label_width = 20; // Fixed width for item labels
    
    // Truncate and right-justify display name to fixed width
    std::string label = item->display_name;
    if (static_cast<int>(label.length()) > max_label_width) {
      label = label.substr(0, max_label_width);
    } else {
      // Right-justify by padding with spaces on the left
      while (static_cast<int>(label.length()) < max_label_width) {
        label = " " + label;
      }
    }
    
    std::string line;
    
    // Generate sparkline if enabled
    if (item->show_sparkline) {
      std::string sparkline = _generateSparkline(item->history, sparkline_width);
      line = label + ": " + sparkline + " ";
    } else {
      line = label + ": ";
    }
    
    // Add value based on type
    switch (item->type) {
      case DataItem::Double:
        if (item->units.empty()) {
          line += FormatString("%.2f", item->double_value);
        } else {
          line += FormatString("%.2f %s", item->double_value, item->units.c_str());
        }
        break;
      case DataItem::Int:
        if (item->units.empty()) {
          line += FormatString("%d", item->int_value);
        } else {
          line += FormatString("%d %s", item->int_value, item->units.c_str());
        }
        break;
      case DataItem::String:
        line += item->string_value;
        break;
    }
    
    //ncplane_putstr_yx(ctx->_stdplane, current_y, _x + 1, line.c_str());
    current_y++;
  }
}

void PerformanceVisualizerWidget::_onLayoutChanged() {
  // Widget handles its own sizing based on data
}

void PerformanceVisualizerWidget::_onInput(uint32_t c, struct ncinput ni) {
  // No input handling for now - could add pause/resume, etc.
}

} // namespace ork::notcurses 
#endif