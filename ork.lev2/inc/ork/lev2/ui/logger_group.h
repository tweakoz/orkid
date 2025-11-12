////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/logger.h>
#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/graphview.h>
#include <ork/lev2/ui/dynagrid.h>
#include <ork/lev2/ui/tabs.h>
#include <set>
#include <map>
#include <mutex>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

struct LoggerGroup : public Group {

  LoggerGroup(const std::string& name);
  ~LoggerGroup() override;

  static loggergroup_ptr_t create(
    const std::string& name,
    const std::set<std::string>& allowed_channels
  );

  // Backend registration
  static void registerOnBackend(loggergroup_ptr_t group, logger_backend_ptr_t backend);
  static void unregisterFromBackend(loggergroup_ptr_t group, logger_backend_ptr_t backend);

  // Channel management
  void addChannel(const std::string& name, lev2::Context* pt);
  void removeChannel(const std::string& name);
  bool hasChannel(const std::string& name) const;  // Uses regex pattern matching

  // Message handling (called from backend, any thread)
  void onLogMessage(const std::string& channel, const std::string& msg);
  void onStatus(const std::string& channel, const std::string& subchan, const std::string& msg);
  void onPerfItem(const std::string& channel, const std::string& name, svar64_t value);

  // Process queued messages (called from UI thread)
  void processQueuedMessages(lev2::Context* pt);

  void DoLayout() override;

  fvec4 _background_color;
private:
  void _doGpuInit(lev2::Context* pt) override;
  void _doOnResized() override;
  void DoDraw(ui::drawevent_constptr_t drwev) override;

  // Channel filtering
  std::set<std::string> _allowed_channels;
  std::vector<std::regex> _channel_patterns;  // Precompiled regex patterns

  // Internal layout (decoupled from parent)
  layoutgroup_ptr_t _internal_layout;

  // UI components
  struct ChannelView {
    group_ptr_t _container;          // Container for this channel's UI
    textbox_ptr_t _status_area;
    textbox_ptr_t _log_area;
    dynagrid_ptr_t _perf_grid;       // Grid for performance graphs
    std::map<std::string, graphview_ptr_t> _perf_graphs;
    std::map<std::string, std::string> _status_lines;
  };

  std::shared_ptr<TabWidget> _tab_widget;
  std::map<std::string, ChannelView> _channel_views;

  // Thread-safe message queue
  struct PendingMessage {
    enum Type { LOG, STATUS, PERF } type;
    std::string channel;
    std::string message;
    std::string subchan;
    std::string perf_name;
    svar64_t perf_value;
  };

  std::mutex _message_mutex;
  std::vector<PendingMessage> _pending_messages;

  // UI update helpers (must be called on UI thread)
  void _appendLogToUI(const std::string& channel, const std::string& msg);
  void _updateStatusUI(const std::string& channel, const std::string& subchan, const std::string& msg);
  void _updatePerfGraphUI(const std::string& channel, const std::string& name, svar64_t value, lev2::Context* pt);
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
