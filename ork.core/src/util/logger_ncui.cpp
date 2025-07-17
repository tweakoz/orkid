////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/logger.h>
#include <ork/util/ncui.h>
#include <ork/util/ncui_perfviz.h>
#include <ork/util/ringbuffer.inl>
#include <thread>
#include <atomic>
#include <cstdio>
#include <errno.h>
#include <string>

////////////////////////////////////////////////////////////////
namespace ork {
////////////////////////////////////////////////////////////////

using namespace ork::notcurses;

static std::atomic<int> _instance_count{0};

constexpr size_t MAX_STATUS_LINES = 14; // Maximum number of status lines

////////////////////////////////////////////////////////////////
// Type-specific performance item classes
////////////////////////////////////////////////////////////////

struct NotCursesPerfItemInt {
  std::string name;
  ork::RingBuffer<int> history{40};
  std::atomic<int> current_value{0};
  mutable std::mutex _mutex;
  
  void pushValue(int value) {
    std::lock_guard<std::mutex> lock(_mutex);
    current_value = value;
    history.push_one(value);
  }
  
  int getCurrentValue() const {
    return current_value.load();
  }
};

struct NotCursesPerfItemDouble {
  std::string name;
  ork::RingBuffer<double> history{40};
  std::atomic<double> current_value{0.0};
  mutable std::mutex _mutex;
  
  void pushValue(double value) {
    std::lock_guard<std::mutex> lock(_mutex);
    current_value = value;
    history.push_one(static_cast<float>(value));
  }
  
  double getCurrentValue() const {
    return current_value.load();
  }
};

struct NotCursesPerfItemString {
  std::string name;
  std::string current_value;
  mutable std::mutex _mutex;
  
  void pushValue(const std::string& value) {
    std::lock_guard<std::mutex> lock(_mutex);
    current_value = value;
  }
  
  std::string getCurrentValue() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return current_value;
  }
};

using ncui_perfitem_int_ptr_t = std::shared_ptr<NotCursesPerfItemInt>;
using ncui_perfitem_double_ptr_t = std::shared_ptr<NotCursesPerfItemDouble>;
using ncui_perfitem_string_ptr_t = std::shared_ptr<NotCursesPerfItemString>;

////////////////////////////////////////////////////////////////
// Updated channel implementation
////////////////////////////////////////////////////////////////

struct NotCursesChannelImpl {
  std::map<std::string, std::string> _status_data;
  std::mutex _subchannel_mutex;
  
  // Type-specific performance item storage
  std::map<std::string, ncui_perfitem_int_ptr_t> _perf_items_int;
  std::map<std::string, ncui_perfitem_double_ptr_t> _perf_items_double;
  std::map<std::string, ncui_perfitem_string_ptr_t> _perf_items_string;
  mutable std::mutex _perfitem_mutex;
  
  bool hasPerformanceItems() const {
    std::lock_guard<std::mutex> lock(_perfitem_mutex);
    return !_perf_items_int.empty() || !_perf_items_double.empty() || !_perf_items_string.empty();
  }
};

using channel_impl_ptr_t = std::shared_ptr<NotCursesChannelImpl>;

////////////////////////////////////////////////////////////////
// Enhanced ChannelPanel with conditional performance widget
////////////////////////////////////////////////////////////////
struct ChannelPanel;
using channel_panel_ptr_t = std::shared_ptr<ChannelPanel>;

struct ChannelPanel : public ::ork::notcurses::Group {

  ////////////////////////////////////////////

  ChannelPanel(const LogChannel* channel) 
      : _channel(channel) {

    _height = 24;
    _status_widget = std::make_shared<TextLines>();
    _log_widget = std::make_shared<TextLines>();

    _status_widget->_bg_color = fvec3::Yellow()*0.3;
    _status_widget->setColor(fvec3::Yellow());
    _log_widget->_bg_color = _channel->_color*0.3;
    _log_widget->setColor(_channel->_color);
    _log_widget->setScrolling(true);
    _log_widget->setAutoScroll(true);
  }

  ////////////////////////////////////////////

  void addLogLine(const std::string& line) {
    _log_widget->addLine(line);
  }

  ////////////////////////////////////////////

  void updateStatusLines(const std::vector<std::string>& status_lines) {
    _status_widget->setLines(status_lines);
    _status_widget->_height = statusHeight();
    _onLayoutChanged();
  }

  ////////////////////////////////////////////

  void _updatePerfWidget() {
    auto impl = _getImplForChannel();
    bool should_have_perf = impl->hasPerformanceItems();
    
    if (should_have_perf) {
      _ensurePerfWidget();
    } else {
      _removePerfWidget();
    }

    for( auto item : impl->_perf_items_int ) {
      auto perf_item = item.second;
      dataitem_ptr_t data_item = _perf_data_source->findItem(perf_item->name);
      if (nullptr == data_item) {
        data_item = PerformanceDataSource::makeIntItem(perf_item->name, "", 
          [perf_item]() { return perf_item->getCurrentValue(); }, true);
       _perf_data_source->addItem(item.first, data_item);
      }
    }
    for( auto item : impl->_perf_items_double ) {
      auto perf_item = item.second;
      dataitem_ptr_t data_item = _perf_data_source->findItem(perf_item->name);
      if (nullptr == data_item) {
        data_item = PerformanceDataSource::makeDoubleItem(perf_item->name, "", 
          [perf_item]() { return perf_item->getCurrentValue(); }, true);
        _perf_data_source->addItem(item.first, data_item);
      }
    }
    for( auto item : impl->_perf_items_string ) {
      auto perf_item = item.second;
      dataitem_ptr_t data_item = _perf_data_source->findItem(perf_item->name);
      if (nullptr == data_item) {
        data_item = PerformanceDataSource::makeStringItem(perf_item->name, 
          [perf_item]() { return perf_item->getCurrentValue(); });
        _perf_data_source->addItem(item.first, data_item);  
      }
    }
  }

  ////////////////////////////////////////////

  void _ensurePerfWidget() {
    auto impl = _getImplForChannel();
    if(_perf_widget){
      return;
    }
        
    // Create performance data source
    _perf_data_source = std::make_shared<PerformanceDataSource>();
    _perf_data_source->name = _channel->_name + " Performance";
        
    // Create performance widget
    _perf_widget = std::make_shared<PerformanceVisualizerWidget>();
    _perf_widget->data_source = _perf_data_source;
    _perf_widget->title = "Performance";
    _perf_widget->update_interval = 0.125f;
    _perf_widget->sparkline_width =40;
    _perf_widget->_height = 8;
    _status_widget->_height = 8;
    
    _recreateLayout();
  }

  ////////////////////////////////////////////

  void _removePerfWidget() {
    if (!_perf_widget) {
      return;
    }
    _perf_widget.reset();
    _perf_data_source.reset();
    _recreateLayout();
  }

  ////////////////////////////////////////////

  void _recreateLayout() {
    _markLayoutDirty();
    _onLayoutChanged();
  }

  ////////////////////////////////////////////

  channel_impl_ptr_t _getImplForChannel() {
    channel_impl_ptr_t rval;
    OrkAssert(_channel != nullptr);
    if (auto as_impl = _channel->_backend_impl.tryAs<channel_impl_ptr_t>()) {
      rval = as_impl.value();
    } else {
      rval = _channel->_backend_impl.makeShared<NotCursesChannelImpl>();
    }
    return rval;
  }

  ////////////////////////////////////////////

  void _doDraw() final {
    if (_perf_widget) {
      _perf_widget->draw();
    }
    if (_status_widget) {
      _status_widget->draw();
    }
    if (_log_widget) {
      _log_widget->draw();
    }
  }

  ////////////////////////////////////////////

  int _statusWidth() const {
    return _perf_widget ? static_cast<int>(_width * 0.65f) : _width;
  }

  ////////////////////////////////////////////

  size_t statusHeight() const {
    size_t H = _status_widget->_lines.size();
    if(H > MAX_STATUS_LINES){
        H = MAX_STATUS_LINES;
    }
    if(H < 1){
        H = 1;
    }
    return H;
   }

  ////////////////////////////////////////////

  int _statusX() const {
    if (_perf_widget) {
      return _x + static_cast<int>(_width * 0.4f);
    }
    return _x; 
  }

  ////////////////////////////////////////////

  void _onLayoutChanged() final {
    int status_height = statusHeight();
    int status_width = _statusWidth();
      // Performance widget takes 40% of width
    if(_perf_widget){
      int perf_width = _width - status_width;
      int PDH = _perf_data_source ? _perf_data_source->_items_by_order.size()+2 : 0;

      _perf_widget->setPosition(_x, _y);
      _status_widget->setPosition(_x+perf_width,_y);
      _log_widget->setPosition(_x+perf_width,_y + status_height);

      _perf_widget->resize(perf_width, PDH);
      _status_widget->resize(status_width, status_height);
      _log_widget->resize(status_width, std::max(1, _height - status_height));
    }
    else {
      int remaining_height = _height - status_height;
      _status_widget->setPosition(_x, _y);
      _log_widget->setPosition(_x,_y + status_height);
      _status_widget->resize(status_width, status_height);
      _log_widget->resize(_width,std::max(1, remaining_height));
    }
  }

  ////////////////////////////////////////////

  void _updateLogWidgetLayout() {
  }

  ////////////////////////////////////////////

  void _onInput(uint32_t c, struct ncinput ni) final {
    int sh = statusHeight();
    if (ni.y >= _y && ni.y < _y + sh) {
      if (_hpack) {
        _hpack->onInput(c, ni);
      } else {
        _status_widget->onInput(c, ni);
      }
      return;
    }
    if (ni.y >= (_y+sh) && ni.y < (_y + sh+_log_widget->_height)) {
      _log_widget->onInput(c, ni);
      return;
    }
  }
  
  ////////////////////////////////////////////

  widget_vect_t children() const final {
    widget_vect_t rval;
    if (_hpack) {
      rval.push_back(_hpack);
    } else {
      rval.push_back(_status_widget);
    }
    rval.push_back(_log_widget);
    return rval;
  }

  ////////////////////////////////////////////

  const LogChannel* _channel;
  textlines_ptr_t _status_widget;
  textlines_ptr_t _log_widget;
  
  // Performance visualization components
  perfviz_ptr_t _perf_widget;
  horizontalpack_ptr_t _hpack;
  perfdatasource_ptr_t _perf_data_source;
};

channel_panel_ptr_t createChannelPanel(const LogChannel* channel) {
  channel_panel_ptr_t chp = std::make_shared<ChannelPanel>(channel);
  chp->_status_widget->_setParent(chp);
  chp->_log_widget->_setParent(chp);
  return chp;
}

////////////////////////////////////////////////////////////////
// NotCursesLoggerUI: Updated to handle performance items
////////////////////////////////////////////////////////////////
struct NotCursesLoggerUI {

  ////////////////////////////////////////////

  NotCursesLoggerUI() {
    int num_instances = _instance_count.fetch_add(1) + 1;
    OrkAssertI(num_instances == 1, "Only one NotCursesLoggerUI instance allowed at a time");

    _context = context();
    _setupLoggerWidgets();
  }

  ////////////////////////////////////////////

  ~NotCursesLoggerUI() {
    int num_instances = _instance_count.fetch_sub(1) - 1;
    OrkAssertI(num_instances == 0, "NotCursesLoggerUI instance count mismatch");
  }

  ////////////////////////////////////////////

  void _setupLoggerWidgets() {
    _tab_group = std::make_shared<TabGroup>();
    _context->_root_widget->setContent(_tab_group, _context->_root_widget);
  }

  ////////////////////////////////////////////

  void _add_log_line(const LogChannel* channel, const std::string& line) {
    auto panel = _getOrCreateChannelPanel(channel);
    panel->addLogLine(line);
  }

  ////////////////////////////////////////////

  void _update_subchannel(const LogChannel* channel, const std::string& subchannel, const std::string& value) {
    auto impl = _implForChannel(channel);
    {
      std::lock_guard<std::mutex> lock(impl->_subchannel_mutex);
      impl->_status_data[subchannel] = value;
    }

    std::vector<std::string> status_lines;
    {
      std::lock_guard<std::mutex> lock(impl->_subchannel_mutex);
      for (const auto& [sub_name, sub_value] : impl->_status_data) {
        status_lines.push_back(sub_name + ": " + sub_value);
      }
    }

    auto panel = _getOrCreateChannelPanel(channel);
    panel->updateStatusLines(status_lines);
  }

  ////////////////////////////////////////////

  void _update_perfitem(const LogChannel* channel, const std::string& perfitem, svar64_t& dd) {
    auto impl = _implForChannel(channel);
    
    // Type-based storage
    {
      
      std::lock_guard<std::mutex> perf_lock(impl->_perfitem_mutex);
      
      if (auto as_int = dd.tryAs<int>()) {
        ncui_perfitem_int_ptr_t& item = impl->_perf_items_int[perfitem];
        if (!item) {
          item = std::make_shared<NotCursesPerfItemInt>();
          item->name = perfitem;
        }
        item->pushValue(as_int.value());
        
      } else if (auto as_double = dd.tryAs<double>()) {
        ncui_perfitem_double_ptr_t& item = impl->_perf_items_double[perfitem];
        if (!item) {
          item = std::make_shared<NotCursesPerfItemDouble>();
          item->name = perfitem;
        }
        item->pushValue(as_double.value());
        
      } else if (auto as_float = dd.tryAs<float>()) {
        ncui_perfitem_double_ptr_t& item = impl->_perf_items_double[perfitem];
        if (!item) {
          item = std::make_shared<NotCursesPerfItemDouble>();
          item->name = perfitem;
        }
        item->pushValue(static_cast<double>(as_float.value()));
        
      } else if (auto as_string = dd.tryAs<std::string>()) {
        ncui_perfitem_string_ptr_t& item = impl->_perf_items_string[perfitem];
        if (!item) {
          item = std::make_shared<NotCursesPerfItemString>();
          item->name = perfitem;
        }
        item->pushValue(as_string.value());
      }
    }
    
    // Update UI if needed
    auto panel = _getOrCreateChannelPanel(channel);
    panel->_updatePerfWidget();
  }

  ////////////////////////////////////////////

  channel_panel_ptr_t _getOrCreateChannelPanel(const LogChannel* channel) {
    auto it = _channel_panels.find(channel->_name);
    if (it == _channel_panels.end()) {
      auto panel                      = createChannelPanel(channel);
      _channel_panels[channel->_name] = panel;
      _tab_group->addTab(channel->_name, panel, _tab_group, channel->_color);
      return panel;
    }
    return it->second;
  }

  ////////////////////////////////////////////

  channel_impl_ptr_t _implForChannel(const LogChannel* channel) {
    channel_impl_ptr_t rval;
    OrkAssert(channel != nullptr);
    if (auto as_impl = channel->_backend_impl.tryAs<channel_impl_ptr_t>()) {
      rval = as_impl.value();
    } else {
      rval = channel->_backend_impl.makeShared<NotCursesChannelImpl>();
    }
    return rval;
  }

  ////////////////////////////////////////////

  bool isReady() const {
    return _context && _context->isReady();
  }

  context_ptr_t _context;
  std::shared_ptr<TabGroup> _tab_group;
  std::map<std::string, channel_panel_ptr_t> _channel_panels;
  LoggerBackend* _backend = nullptr;
};

////////////////////////////////////////////////////////////////
// Singleton and function pointers
////////////////////////////////////////////////////////////////

static std::shared_ptr<NotCursesLoggerUI> notcurses_ui() {
  static std::shared_ptr<NotCursesLoggerUI> instance = std::make_shared<NotCursesLoggerUI>();
  return instance->isReady() ? instance : nullptr;
}

////////////////////////////////////////////////////////////////

static void DebugIt(const LogChannel* channel, const std::string& str) {
  static FILE* debug_file = fopen("/tmp/ork_debug.log", "a");
  fprintf(debug_file, "%s[%s]\t%s\n", channel->_c1_prefix.c_str(), channel->_name.c_str(), str.c_str());
  fflush(debug_file);
}

////////////////////////////////////////////////////////////////

static void NotCursesAddLogFn(const LogChannel* channel, const std::string& str) {
  auto ui = notcurses_ui();
  auto& mutex = ui->_backend->_mutex;
  std::lock_guard<std::mutex> lock(mutex);
  if (ui) {
    ui->_add_log_line(channel, str);
  }
}

////////////////////////////////////////////////////////////////

static void NotCursesStatusLogFn(const LogChannel* channel, std::string subchannel, const std::string& str) {
  auto ui = notcurses_ui();
  auto& mutex = ui->_backend->_mutex;
  std::lock_guard<std::mutex> lock(mutex);
  if (ui) {
    ui->_update_subchannel(channel, subchannel, str);
  }
}

static void NotCursesPerfItem(const LogChannel* channel, std::string perfitem, svar64_t& dd){
  auto ui = notcurses_ui();
  auto& mutex = ui->_backend->_mutex;
  std::lock_guard<std::mutex> lock(mutex);
  if (ui) {
    ui->_update_perfitem(channel, perfitem, dd);
  }
}

////////////////////////////////////////////////////////////////

void installNotCursesToBackend(LoggerBackend* backend) {
  std::lock_guard<std::mutex> lock(backend->_mutex);
  auto ui = notcurses_ui();
  if (ui) {
    ui->_backend = backend;
    backend->_add_log_line      = NotCursesAddLogFn;
    backend->_begin_log_line    = NotCursesAddLogFn;
    backend->_continue_log_line = NotCursesAddLogFn;
    backend->_end_log_line      = NotCursesAddLogFn;
    backend->_status            = NotCursesStatusLogFn;
    backend->_on_perf_item      = NotCursesPerfItem;
  }
}

////////////////////////////////////////////////////////////////
} // namespace ork
