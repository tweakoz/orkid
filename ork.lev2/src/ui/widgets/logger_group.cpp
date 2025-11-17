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
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/style.h>
#include <ork/kernel/string/deco.inl>
#include <sstream>
#include <regex>

namespace ork::ui {

  constexpr float BASE_ALPHA = 0.975f;
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

  _tab_widget->_tabBarBackground = fvec4(0.1, 0.1, 0.2, BASE_ALPHA);
  _tab_widget->_contentBackground = fvec4(0.1, 0.1, 0.2, BASE_ALPHA);
  _tab_widget->_draw_background = false;
  _tab_widget->gpuInit(pt);
  }

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::addChannel(const std::string& name, lev2::Context* pt) {
  if (_channel_views.find(name) != _channel_views.end()) {
    return; // Already exists
  }

  printf("LoggerGroup<%s>::addChannel(%s)\n", _name.c_str(), name.c_str());

  // Assign channel-specific color (simple hash-based color generation)
  // TODO: Make this configurable per channel
  uint64_t hash = 0;
  for (char c : name) {
    hash = hash * 31 + c;
  }
  float hue = (hash % 360) / 360.0f;
  // Convert HSV to RGB (simple approximation)
  float r = std::abs(std::sin(hue * 6.28f));
  float g = std::abs(std::sin((hue + 0.33f) * 6.28f));
  float b = std::abs(std::sin((hue + 0.67f) * 6.28f));
  fvec3 channel_color(r * 0.8f + 0.2f, g * 0.8f + 0.2f, b * 0.8f + 0.2f);

  // Create per-channel tab style using CSS-style derivation
  if (_uicontext && _uicontext->_theme_engine) {
    auto base_tab_style = _uicontext->_theme_engine->_styledb->getStyle("tab"_crcu);
    if (base_tab_style) {
      auto channel_tab_style = Style::derive(base_tab_style);
      channel_tab_style->_border_color = fvec4(channel_color,1);        // Tab outline = channel color
      channel_tab_style->_bg_color = fvec4(channel_color * 0.4f,BASE_ALPHA);     // Tab bg = channel color * 0.4

      // Register this derived style in the theme database
      std::string style_name = "tab:" + name;
      uint64_t style_tag = CrcString(style_name.c_str()).hashed();
      _uicontext->_theme_engine->_styledb->registerStyle(style_tag, channel_tab_style);
    }
  }

  ChannelView view;

  // Create container group for this channel
  auto vpack = std::make_shared<VerticalPack>(name, 0, 0, width(), height());
  view._container = vpack;
  _tab_widget->addChild(view._container);
  view._container->gpuInit(pt);
  vpack->_draw_background = false;
  vpack->_fill = true;  // Distribute remaining space to children without fixed height
  vpack->_margin = 0;   // Small margin between sections for visual separation

  // Apply the channel-specific tab style
  if (_uicontext && _uicontext->_theme_engine) {
    std::string style_name = "tab:" + name;
    uint64_t style_tag = CrcString(style_name.c_str()).hashed();
    _tab_widget->_per_tab_style_tags[view._container] = style_tag;
  }

  // Status area at top (shows current status lines)
  // bg = channel_color * 0.2, text = channel_color
  auto statusarea = std::make_shared<TextBox>(
    name + "_status",
    fvec4(channel_color* 0.75f, BASE_ALPHA),  // Background = channel color * 0.2
    ""
  );
  statusarea->_fixed_height = 28;  // Start small, will grow with content
  statusarea->_blending = lev2::BlendingMacro::ALPHA;
  view._status_area = statusarea;
  view._status_area->_color = fvec4(channel_color * 0.25f,BASE_ALPHA);
  view._status_area->_textcolor = fvec4(1,1,1,1);  // Text = channel color
  view._status_area->_halign = ETextAlignH::LEFT;
  view._status_area->_valign = ETextAlignV::TOP;
  vpack->addChild(view._status_area);
  view._status_area->gpuInit(pt);

  // Performance graphs grid in middle
  // bg = channel_color * 0.1 (between status 0.2 and log 0.0)
  auto dynagrid = std::make_shared<DynaGrid>(name + "_perf_grid", 0, 0, width(), 100);
  view._perf_grid = dynagrid;
  vpack->addChild(view._perf_grid);
  view._perf_grid->gpuInit(pt);
  dynagrid->_fixed_height = 100;  // Start small, will grow with content
  dynagrid->_bgcolor = fvec4(channel_color * 0.3f,BASE_ALPHA);  // Background = channel color * 0.1
  dynagrid->_draw_background = true;

  // Log area at bottom (scrolling log text)
  // bg = black, text = channel_color
  auto log_area = std::make_shared<TextBox>(
    name + "_log",
    fvec4(channel_color*0.2, BASE_ALPHA),  // Black background
    ""
  );
  log_area->_blending = lev2::BlendingMacro::ALPHA;
  view._log_area = log_area;
  view._log_area->_textcolor = fvec4(channel_color,1);  // Text = channel color
  view._log_area->_halign = ETextAlignH::LEFT;
  view._log_area->_valign = ETextAlignV::TOP;  // Use TOP instead of BOTTOM for consistent positioning
  view._log_area->_enable_scrolling = true;  // Enable mouse wheel scrolling
  vpack->addChild(view._log_area);
  view._log_area->gpuInit(pt);

  _channel_views[name] = view;
}

void LoggerGroup::DoDraw(drawevent_constptr_t drwev) {

  // Process queued messages on UI/render thread
  auto ctx = drwev->GetTarget();
  processQueuedMessages(ctx);

  // Draw semi-transparent dark background
  //Widget::_drawColoredBox(drwev, _background_color, lev2::BlendingMacro::ALPHA);

  //_tab_widget->_contentBackground = _background_color;

  // Draw children
  drawChildren(drwev);
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::_doOnResized() {
  // Resize tab widget to match LoggerGroup's size
  if (_tab_widget) {
    _tab_widget->SetRect(0, 0, width(), height());
  }

  // Call base class to propagate to children
  Group::_doOnResized();
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
        _updatePerfGraphUI(msg.channel, msg.perf_name, msg.perf_value, pt);
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

    // Content-based sizing: Update height based on number of status lines
    const int line_height = 20;  // Approximate height per line
    const int padding = 10;      // Top + bottom padding
    int num_lines = view._status_lines.size();
    int new_height = std::max(40, num_lines * line_height + padding);  // Minimum 40px
    view._status_area->_fixed_height = new_height;

    // Trigger layout update
    if (view._container) {
      view._container->DoLayout();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void LoggerGroup::_updatePerfGraphUI(const std::string& channel, const std::string& name, svar64_t value, lev2::Context* pt) {
  auto it = _channel_views.find(channel);
  if (it == _channel_views.end())
    return;

  auto& view = it->second;
  if (!view._perf_grid)
    return;

  // Create shared graph if it doesn't exist
  if (!view._shared_graph) {
    view._shared_graph = std::make_shared<GraphView>();
    view._shared_graph->_name = channel + "_perf";
    view._shared_graph->_show_stats = false;
    view._perf_grid->addChild(view._shared_graph);

    // Initialize GPU resources if needed
    if (pt && view._shared_graph->_needsinit) {
      view._shared_graph->gpuInit(pt);
    }

    // Set fixed height for the shared graph
    view._perf_grid->_fixed_height = 200;

    // Trigger layout update
    if (view._container) {
      view._container->DoLayout();
    }
  }

  // Get or create the channel for this perf item
  auto graph_channel = view._shared_graph->channel(name);

  // Check if series already exists for this perf item
  auto series = graph_channel->getSeries(name);
  if (!series) {
    // Generate color based on perf item name (similar to channel color generation)
    uint64_t hash = 0;
    for (char c : name) {
      hash = hash * 31 + c;
    }
    float hue = (hash % 360) / 360.0f;
    float r = std::abs(std::sin(hue * 6.28f));
    float g = std::abs(std::sin((hue + 0.33f) * 6.28f));
    float b = std::abs(std::sin((hue + 0.67f) * 6.28f));
    fvec3 series_color(r * 0.8f + 0.2f, g * 0.8f + 0.2f, b * 0.8f + 0.2f);

    // Create new series
    series = graph_channel->addSeries(name, series_color);
    series->setMaxSamples(100);
  }

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

  // Add sample to series
  series->addSample(float_value);

  // Mark GraphView surface as needing repaint
  view._shared_graph->MarkSurfaceDirty();
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
