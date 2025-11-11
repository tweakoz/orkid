////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/logger_group.h>
#include <ork/lev2/ui/logger_ui_backend.h>
#include <ork/lev2/ui/textbox.h>
#include <ork/lev2/ui/tabs.h>
#include <ork/lev2/ui/graphview.h>
#include <ork/lev2/ui/dynagrid.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/kernel/string/deco.inl>
#include <sstream>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

LoggerGroup::LoggerGroup(const std::string& name)
  : Group(name, 0, 0, 0, 0) {
}

///////////////////////////////////////////////////////////////////////////////

LoggerGroup::~LoggerGroup() {
}

///////////////////////////////////////////////////////////////////////////////

loggergroup_ptr_t LoggerGroup::create(
  const std::string& name,
  const std::set<std::string>& allowed_channels
) {
  auto group = std::make_shared<LoggerGroup>(name);
  group->_allowed_channels = allowed_channels;

  // Create internal decoupled layout
  group->_internal_layout = std::make_shared<LayoutGroup>("logger_internal_layout");

  return group;
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::registerOnBackend(loggergroup_ptr_t group, logger_backend_ptr_t backend) {
  if (!group || !backend)
    return;

  auto impl_var = backend->_impl;
  if (impl_var.isA<loggeruibackend_ptr_t>()) {
    auto impl = impl_var.get<loggeruibackend_ptr_t>();
    impl->registerGroup(group);
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::unregisterFromBackend(loggergroup_ptr_t group, logger_backend_ptr_t backend) {
  if (!group || !backend)
    return;

  auto impl_var = backend->_impl;
  if (impl_var.isA<loggeruibackend_ptr_t>()) {
    auto impl = impl_var.get<loggeruibackend_ptr_t>();
    impl->unregisterGroup(group);
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::_doGpuInit(lev2::Context* pt) {
  // Initialize UI for each allowed channel
  for (const auto& channel_name : _allowed_channels) {
    addChannel(channel_name, pt);
  }

  // Create tab widget for channels if multiple channels
  if (_allowed_channels.size() > 1) {
    _tab_widget = std::make_shared<TabWidget>("logger_tabs", 0, 0, width(), height());
    addChild(_tab_widget);
    _tab_widget->gpuInit(pt);

    for (auto& [name, view] : _channel_views) {
      if (view._container) {
        _tab_widget->addChild(view._container);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::addChannel(const std::string& name, lev2::Context* pt) {
  if (_channel_views.find(name) != _channel_views.end()) {
    return; // Already exists
  }

  ChannelView view;

  // Create container group for this channel
  view._container = std::make_shared<Group>(name, 0, 0, width(), height());
  addChild(view._container);
  view._container->gpuInit(pt);

  // Status area at top (shows current status lines)
  view._status_area = std::make_shared<TextBox>(
    name + "_status",
    fvec4(0.15f, 0.15f, 0.2f, 1.0f),  // Dark background
    ""
  );
  view._status_area->_textcolor = fvec4(0.8f, 0.8f, 1.0f, 1.0f);
  view._status_area->_halign = ETextAlignH::LEFT;
  view._status_area->_valign = ETextAlignV::TOP;
  view._container->addChild(view._status_area);
  view._status_area->gpuInit(pt);

  // Performance graphs grid in middle
  view._perf_grid = std::make_shared<DynaGrid>(name + "_perf_grid", 0, 0, width(), 200);
  view._container->addChild(view._perf_grid);
  view._perf_grid->gpuInit(pt);

  // Log area at bottom (scrolling log text)
  view._log_area = std::make_shared<TextBox>(
    name + "_log",
    fvec4(0.1f, 0.1f, 0.15f, 1.0f),   // Darker background
    ""
  );
  view._log_area->_textcolor = fvec4(0.9f, 0.9f, 0.9f, 1.0f);
  view._log_area->_halign = ETextAlignH::LEFT;
  view._log_area->_valign = ETextAlignV::BOTTOM;
  view._container->addChild(view._log_area);
  view._log_area->gpuInit(pt);

  _channel_views[name] = view;
}

void LoggerGroup::DoLayout() {
  Group::DoLayout();

  // Layout each channel view
  for (auto& [name, view] : _channel_views) {
    if (!view._container)
      continue;

    int w = view._container->width();
    int h = view._container->height();

    // Status area: top 80px
    if (view._status_area) {
      view._status_area->SetRect(0, 0, w, 80);
    }

    // Perf grid: 200px below status
    if (view._perf_grid) {
      view._perf_grid->SetRect(0, 84, w, 200);
    }

    // Log area: fills remaining space
    if (view._log_area) {
      view._log_area->SetRect(0, 288, w, h - 288);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::removeChannel(const std::string& name) {
  _channel_views.erase(name);
}

///////////////////////////////////////////////////////////////////////////////

bool LoggerGroup::hasChannel(const std::string& name) const {
  return _allowed_channels.find(name) != _allowed_channels.end();
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::onLogMessage(const std::string& channel, const std::string& msg) {
  if (!hasChannel(channel))
    return;

  std::lock_guard<std::mutex> lock(_message_mutex);
  _pending_messages.push_back({
    PendingMessage::LOG,
    channel,
    msg,
    "",
    "",
    svar64_t()
  });
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::onStatus(const std::string& channel, const std::string& subchan, const std::string& msg) {
  if (!hasChannel(channel))
    return;

  std::lock_guard<std::mutex> lock(_message_mutex);
  _pending_messages.push_back({
    PendingMessage::STATUS,
    channel,
    msg,
    subchan,
    "",
    svar64_t()
  });
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::onPerfItem(const std::string& channel, const std::string& name, svar64_t value) {
  if (!hasChannel(channel))
    return;

  std::lock_guard<std::mutex> lock(_message_mutex);
  _pending_messages.push_back({
    PendingMessage::PERF,
    channel,
    "",
    "",
    name,
    value
  });
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::processQueuedMessages() {
  std::vector<PendingMessage> local_queue;
  {
    std::lock_guard<std::mutex> lock(_message_mutex);
    local_queue.swap(_pending_messages);
  }

  for (const auto& msg : local_queue) {
    switch (msg.type) {
      case PendingMessage::LOG:
        _appendLogToUI(msg.channel, msg.message);
        break;
      case PendingMessage::STATUS:
        _updateStatusUI(msg.channel, msg.subchan, msg.message);
        break;
      case PendingMessage::PERF:
        _updatePerfGraphUI(msg.channel, msg.perf_name, msg.perf_value);
        break;
    }
  }

  if (!local_queue.empty()) {
    SetDirty();
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::_appendLogToUI(const std::string& channel, const std::string& msg) {
  auto it = _channel_views.find(channel);
  if (it == _channel_views.end())
    return;

  auto& view = it->second;
  if (view._log_area) {
    // Append message to existing log lines
    std::string current_text;
    for (const auto& line : view._log_area->_lines) {
      current_text += line + "\n";
    }
    current_text += msg;

    // Keep only last 1000 lines to prevent memory bloat
    view._log_area->setText(current_text);
    if (view._log_area->_lines.size() > 1000) {
      std::string trimmed;
      size_t start_line = view._log_area->_lines.size() - 1000;
      for (size_t i = start_line; i < view._log_area->_lines.size(); ++i) {
        trimmed += view._log_area->_lines[i] + "\n";
      }
      view._log_area->setText(trimmed);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::_updateStatusUI(const std::string& channel, const std::string& subchan, const std::string& msg) {
  auto it = _channel_views.find(channel);
  if (it == _channel_views.end())
    return;

  auto& view = it->second;
  view._status_lines[subchan] = msg;

  if (view._status_area) {
    // Rebuild status text from all status lines
    std::ostringstream oss;
    for (const auto& [key, value] : view._status_lines) {
      oss << key << ": " << value << "\n";
    }
    view._status_area->setText(oss.str());
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::_updatePerfGraphUI(const std::string& channel, const std::string& name, svar64_t value) {
  auto it = _channel_views.find(channel);
  if (it == _channel_views.end())
    return;

  auto& view = it->second;
  if (!view._perf_grid)
    return;

  // Find or create graph for this perf item
  auto graph_it = view._perf_graphs.find(name);
  graphview_ptr_t graph;

  if (graph_it == view._perf_graphs.end()) {
    // Create new graph
    graph = std::make_shared<GraphView>();
    graph->_name = name;
    view._perf_grid->addChild(graph);
    view._perf_graphs[name] = graph;

    // Create channel and series for this perf metric
    auto channel_ptr = graph->channel(name);
    channel_ptr->addSeries(name, fvec3(0.3f, 0.8f, 1.0f)); // Cyan color
  } else {
    graph = graph_it->second;
  }

  if (!graph)
    return;

  // Convert value to float
  float float_value = 0.0f;
  if (value.isA<float>()) {
    float_value = value.get<float>();
  } else if (value.isA<double>()) {
    float_value = static_cast<float>(value.get<double>());
  } else if (value.isA<int>()) {
    float_value = static_cast<float>(value.get<int>());
  } else if (value.isA<int64_t>()) {
    float_value = static_cast<float>(value.get<int64_t>());
  } else if (value.isA<uint64_t>()) {
    float_value = static_cast<float>(value.get<uint64_t>());
  }

  // Add sample to graph
  auto channel_ptr = graph->channel(name);
  if (channel_ptr && !channel_ptr->_series.empty()) {
    auto series = channel_ptr->_series[0];
    series->addSample(float_value);
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
