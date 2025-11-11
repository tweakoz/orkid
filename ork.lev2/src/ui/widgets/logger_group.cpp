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
#include <ork/lev2/ui/pack.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/kernel/string/deco.inl>
#include <sstream>
#include <regex>

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

  printf("LoggerGroup::create(%s) with %zu patterns:\n", name.c_str(), allowed_channels.size());

  // Compile regex patterns from channel names/patterns
  for (const auto& pattern : allowed_channels) {
    printf("  Compiling pattern: '%s'\n", pattern.c_str());
    try {
      group->_channel_patterns.push_back(std::regex(pattern));
      printf("    -> compiled successfully\n");
    } catch (const std::regex_error& e) {
      printf("    -> regex error: %s, escaping as literal\n", e.what());
      // If pattern is invalid, treat it as a literal string
      // Escape special regex characters
      std::string escaped;
      for (char c : pattern) {
        if (c == '.' || c == '*' || c == '+' || c == '?' || c == '|' ||
            c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
            c == '^' || c == '$' || c == '\\') {
          escaped += '\\';
        }
        escaped += c;
      }
      printf("    -> escaped to: '%s'\n", escaped.c_str());
      group->_channel_patterns.push_back(std::regex(escaped));
    }
  }

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
  //printf("LoggerGroup<%s>::_doGpuInit pt<%p>\n", _name.c_str(), (void*)pt);

  // Don't create channels for wildcard patterns here
  _tab_widget = std::make_shared<TabWidget>("logger_tabs", 0, 0, width(), height());
  addChild(_tab_widget);
  _tab_widget->gpuInit(pt);
  }

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::addChannel(const std::string& name, lev2::Context* pt) {
  if (_channel_views.find(name) != _channel_views.end()) {
    return; // Already exists
  }

  printf("LoggerGroup<%s>::addChannel(%s)\n", _name.c_str(), name.c_str());

  ChannelView view;

  // Create container group for this channel
  auto vpack = std::make_shared<VerticalPack>(name, 0, 0, width(), height());
  view._container = vpack;
  _tab_widget->addChild(view._container);
  view._container->gpuInit(pt);
  vpack->_draw_background = false;
  // Status area at top (shows current status lines)
  auto statusarea = std::make_shared<TextBox>(
    name + "_status",
    fvec4(0.15f, 0.15f, 0.2f, 1.0f),  // Dark background
    ""
  );
  statusarea->_fixed_height = 80;
  statusarea->_blending = lev2::BlendingMacro::ALPHA;
  view._status_area = statusarea;
  view._status_area->_color = fvec4(0.8f, 0.8f, 1.0f, 0.5f);
  view._status_area->_textcolor = fvec4(0.8f, 0.8f, 1.0f, 1.0f);
  view._status_area->_halign = ETextAlignH::LEFT;
  view._status_area->_valign = ETextAlignV::TOP;
  vpack->addChild(view._status_area);
  view._status_area->gpuInit(pt);

  // Performance graphs grid in middle
  auto dynagrid = std::make_shared<DynaGrid>(name + "_perf_grid", 0, 0, width(), 200);
  view._perf_grid = dynagrid;
  vpack->addChild(view._perf_grid);
  view._perf_grid->gpuInit(pt);
  dynagrid->_fixed_height = 200;

  // Log area at bottom (scrolling log text)
  auto log_area = std::make_shared<TextBox>(
    name + "_log",
    fvec4(0.1f, 0.1f, 0.15f, 0.5f),   // Darker background
    ""
  );
  log_area->_blending = lev2::BlendingMacro::ALPHA;
  view._log_area = log_area;
  view._log_area->_textcolor = fvec4(0.9f, 0.9f, 0.9f, 0.5f);
  view._log_area->_halign = ETextAlignH::LEFT;
  view._log_area->_valign = ETextAlignV::BOTTOM;
  vpack->addChild(view._log_area);
  view._log_area->gpuInit(pt);

  _channel_views[name] = view;
}

void LoggerGroup::DoDraw(drawevent_constptr_t drwev) {

  // Process queued messages on UI/render thread
  auto ctx = drwev->GetTarget();
  processQueuedMessages(ctx);

  // Draw semi-transparent dark background
  Widget::_drawColoredBox(drwev, _background_color, lev2::BlendingMacro::ALPHA);

  _tab_widget->_contentBackground = _background_color;

  // Draw children
  drawChildren(drwev);
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::DoLayout() {
  Group::DoLayout();

  // Layout each channel view
  /*
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
  }*/
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::removeChannel(const std::string& name) {
  _channel_views.erase(name);
}

///////////////////////////////////////////////////////////////////////////////

bool LoggerGroup::hasChannel(const std::string& name) const {
  // Check if any of the precompiled patterns match
  if(0)printf("LoggerGroup::hasChannel('%s') checking against %zu patterns\n", name.c_str(), _channel_patterns.size());
  for (size_t i = 0; i < _channel_patterns.size(); i++) {
    bool matches = std::regex_match(name, _channel_patterns[i]);
    if(0)printf("  pattern[%zu]: matches=%d\n", i, matches);
    if (matches) {
      return true;
    }
  }
  if(0)printf("  -> no match found\n");
  return false;
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::onLogMessage(const std::string& channel, const std::string& msg) {
  bool matches = hasChannel(channel);
  if(0)printf("LoggerGroup<%s>::onLogMessage channel<%s> matches<%d>\n",
         _name.c_str(), channel.c_str(), matches);

  if (!matches)
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
  if(0)printf("  -> queued message, total pending<%zu>\n", _pending_messages.size());
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

void LoggerGroup::processQueuedMessages(lev2::Context* pt) {
  std::vector<PendingMessage> local_queue;
  {
    std::lock_guard<std::mutex> lock(_message_mutex);
    local_queue.swap(_pending_messages);
  }

  for (const auto& msg : local_queue) {
    // Dynamically create channel view if it doesn't exist yet
    bool channel_exists = (_channel_views.find(msg.channel) != _channel_views.end());

    if (!channel_exists && pt) {
      addChannel(msg.channel, pt);
    }

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
