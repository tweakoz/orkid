////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/logger.h>
#include <ork/kernel/mutex.h>
#include <fstream>
#include <deque>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace ork {

////////////////////////////////////////////////////////////////
// HTML Backend Implementation
//
// Architecture: Single HTML file with inline script tags
// - Header written once at startup (CSS, JS, E() function)
// - Each log entry written as <script>E({...})</script>
// - Crash-resilient: browsers tolerate incomplete tags at EOF
// - Works with file:// protocol (no XHR needed)
////////////////////////////////////////////////////////////////

static const char* HTML_HEADER = R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>Orkid Log</title>
<style>
:root {
  --bg: #0d0d0d;
  --fg: #e0e0e0;
  --entry-bg: #1a1a1a;
  --border: #333;
  --controls-bg: #141414;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  background: var(--bg);
  color: var(--fg);
  font-family: 'SF Mono', 'Monaco', 'Menlo', 'Consolas', monospace;
  font-size: 12px;
  line-height: 1.4;
}
#controls {
  position: sticky;
  top: 0;
  background: var(--controls-bg);
  padding: 10px 12px;
  border-bottom: 1px solid var(--border);
  z-index: 100;
}
#top-bar {
  display: flex;
  align-items: center;
  gap: 10px;
  margin-bottom: 8px;
  padding-bottom: 8px;
  border-bottom: 1px solid #222;
}
#top-bar .title {
  font-weight: 600;
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  color: #666;
}
.btn {
  padding: 4px 12px;
  background: #222;
  border: 1px solid #444;
  border-radius: 3px;
  color: #aaa;
  cursor: pointer;
  font-size: 10px;
  font-weight: 500;
  text-transform: uppercase;
  letter-spacing: 0.3px;
  transition: all 0.15s ease;
}
.btn:hover { background: #333; color: #fff; border-color: #555; }
.btn:active { background: #444; }
#entry-count {
  color: #555;
  font-size: 10px;
  margin-left: auto;
  font-variant-numeric: tabular-nums;
}
#channel-toggles {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}
.toggle-row {
  display: flex;
  gap: 6px;
  width: 100%;
}
.channel-toggle {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  padding: 4px 8px;
  border-radius: 3px;
  cursor: pointer;
  user-select: none;
  font-size: 11px;
  font-weight: 500;
  transition: all 0.15s ease;
  border: 1px solid transparent;
}
.channel-toggle:hover { filter: brightness(1.2); }
.channel-toggle input[type="checkbox"] {
  -webkit-appearance: none;
  appearance: none;
  width: 12px;
  height: 12px;
  border-radius: 2px;
  cursor: pointer;
  position: relative;
  transition: all 0.15s ease;
}
.channel-toggle input[type="checkbox"]:checked::after {
  content: '';
  position: absolute;
  left: 3px;
  top: 1px;
  width: 4px;
  height: 7px;
  border: solid #000;
  border-width: 0 2px 2px 0;
  transform: rotate(45deg);
}
.channel-toggle.dark-text input[type="checkbox"]:checked::after {
  border-color: #000;
}
.channel-toggle.light-text input[type="checkbox"]:checked::after {
  border-color: #fff;
}
#log { padding: 8px; }
.entry {
  padding: 3px 8px;
  border-left: 3px solid #888;
  margin: 1px 0;
  background: var(--entry-bg);
  border-radius: 0 2px 2px 0;
  white-space: pre-wrap;
  word-break: break-all;
}
.entry .ts { color: #555; margin-right: 10px; font-variant-numeric: tabular-nums; }
.entry .ch { font-weight: 600; margin-right: 10px; }
.entry .sub { color: #777; margin-right: 8px; }
.entry.warn { background: #1a1500; border-left-color: #b58900 !important; }
.entry.error { background: #1a0a0a; border-left-color: #dc322f !important; }
.entry.status { background: #0a1a0a; }
.hidden { display: none !important; }
</style>
</head>
<body>
<div id="controls">
  <div id="top-bar">
    <span class="title">Channels</span>
    <button class="btn" id="btn-all">All</button>
    <button class="btn" id="btn-none">None</button>
    <span id="entry-count"></span>
  </div>
  <div id="channel-toggles"></div>
</div>
<div id="log"></div>
<script>
const entries = [];
const channels = new Map();
const logDiv = document.getElementById('log');
const togglesDiv = document.getElementById('channel-toggles');
const countSpan = document.getElementById('entry-count');
const TOGGLES_PER_ROW = 8;

function hexToRgb(hex) {
  const r = parseInt(hex.slice(1,3), 16);
  const g = parseInt(hex.slice(3,5), 16);
  const b = parseInt(hex.slice(5,7), 16);
  return {r, g, b};
}

function getLuminance(r, g, b) {
  const [rs, gs, bs] = [r, g, b].map(c => {
    c = c / 255;
    return c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
  });
  return 0.2126 * rs + 0.7152 * gs + 0.0722 * bs;
}

function getContrastColors(hex) {
  const {r, g, b} = hexToRgb(hex);
  const lum = getLuminance(r, g, b);

  if (lum > 0.4) {
    return {
      text: hex,
      bg: '#0d0d0d',
      border: hex + '40',
      checkBg: hex + '30',
      checkBorder: hex + '60',
      isDark: false
    };
  } else if (lum > 0.15) {
    const brighten = 1.3;
    const nr = Math.min(255, Math.round(r * brighten));
    const ng = Math.min(255, Math.round(g * brighten));
    const nb = Math.min(255, Math.round(b * brighten));
    const brightHex = '#' + [nr, ng, nb].map(c => c.toString(16).padStart(2,'0')).join('');
    return {
      text: brightHex,
      bg: '#0d0d0d',
      border: hex + '50',
      checkBg: hex + '40',
      checkBorder: hex + '70',
      isDark: false
    };
  } else {
    const lighten = 0.85;
    const nr = Math.round(255 - (255 - r) * (1 - lighten));
    const ng = Math.round(255 - (255 - g) * (1 - lighten));
    const nb = Math.round(255 - (255 - b) * (1 - lighten));
    const lightBg = '#' + [nr, ng, nb].map(c => c.toString(16).padStart(2,'0')).join('');
    return {
      text: '#111',
      bg: lightBg,
      border: hex,
      checkBg: '#fff',
      checkBorder: hex,
      isDark: true
    };
  }
}

function E(e) {
  entries.push(e);

  if (!channels.has(e.ch)) {
    channels.set(e.ch, { visible: true, color: e.color || '#888' });
    updateToggles();
  }

  const div = document.createElement('div');
  let cls = 'entry';
  if (e.type === 'warn') cls += ' warn';
  if (e.type === 'error') cls += ' error';
  if (e.type === 'status') cls += ' status';
  div.className = cls;
  div.dataset.channel = e.ch;
  div.style.borderLeftColor = e.color || '#888';

  let html = '<span class="ts">' + esc(e.ts) + '</span>';
  html += '<span class="ch" style="color:' + (e.color || '#888') + '">[' + esc(e.ch) + ']</span>';
  if (e.sub) html += '<span class="sub">' + esc(e.sub) + ':</span>';
  html += esc(e.msg);
  div.innerHTML = html;

  if (!channels.get(e.ch).visible) div.classList.add('hidden');
  logDiv.appendChild(div);
  updateCount();
}

function esc(s) {
  if (!s) return '';
  return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

function updateToggles() {
  togglesDiv.innerHTML = '';
  const channelArr = Array.from(channels.entries());

  for (let i = 0; i < channelArr.length; i += TOGGLES_PER_ROW) {
    const row = document.createElement('div');
    row.className = 'toggle-row';

    const rowChannels = channelArr.slice(i, i + TOGGLES_PER_ROW);
    rowChannels.forEach(([ch, info]) => {
      const colors = getContrastColors(info.color);

      const label = document.createElement('label');
      label.className = 'channel-toggle ' + (colors.isDark ? 'dark-text' : 'light-text');
      label.style.color = colors.text;
      label.style.background = colors.bg;
      label.style.borderColor = colors.border;

      const cb = document.createElement('input');
      cb.type = 'checkbox';
      cb.checked = info.visible;
      cb.style.background = colors.checkBg;
      cb.style.border = '1px solid ' + colors.checkBorder;
      cb.onchange = () => toggle(ch, cb.checked);

      label.appendChild(cb);
      label.appendChild(document.createTextNode(ch));
      row.appendChild(label);
    });

    togglesDiv.appendChild(row);
  }
}

function toggle(ch, visible) {
  const info = channels.get(ch);
  if (info) info.visible = visible;
  document.querySelectorAll('.entry[data-channel="' + ch + '"]').forEach(el => {
    el.classList.toggle('hidden', !visible);
  });
  updateCount();
}

function setAll(visible) {
  channels.forEach((info, ch) => {
    info.visible = visible;
  });
  document.querySelectorAll('.entry').forEach(el => {
    el.classList.toggle('hidden', !visible);
  });
  updateToggles();
  updateCount();
}

function updateCount() {
  const total = entries.length;
  const visible = document.querySelectorAll('.entry:not(.hidden)').length;
  countSpan.textContent = visible.toLocaleString() + ' / ' + total.toLocaleString();
}

document.getElementById('btn-all').onclick = () => setAll(true);
document.getElementById('btn-none').onclick = () => setAll(false);
</script>
)HTML";

struct HtmlBackendImpl;
using html_backend_impl_ptr_t = std::shared_ptr<HtmlBackendImpl>;

struct HtmlBackendImpl {
  std::string _path;
  float _flush_interval_ms = 100.0f;
  std::ofstream _file;
  LockedResource<std::deque<std::string>> _queue;
  std::thread _writer_thread;
  std::atomic<bool> _running{false};
  std::mutex _cv_mutex;
  std::condition_variable _cv;
  std::atomic<bool> _immediate_flush{false};

  HtmlBackendImpl(const std::string& path, float flush_interval_ms)
      : _path(path)
      , _flush_interval_ms(flush_interval_ms) {
  }

  ~HtmlBackendImpl() {
    stop();
  }

  void start() {
    _file.open(_path, std::ios::out | std::ios::trunc);
    if (!_file.is_open()) {
      fprintf(stderr, "HtmlBackend: Failed to open %s\n", _path.c_str());
      return;
    }

    // Write HTML header with all CSS/JS
    _file << HTML_HEADER;
    _file.flush();

    _running = true;
    _writer_thread = std::thread([this]() { writerLoop(); });
  }

  void stop() {
    if (_running) {
      _running = false;
      _cv.notify_all();
      if (_writer_thread.joinable()) {
        _writer_thread.join();
      }
      flushQueue();
      _file.close();
    }
  }

  void enqueue(const std::string& script_line) {
    _queue.atomicOp([&script_line](std::deque<std::string>& q) {
      q.push_back(script_line);
    });
    _cv.notify_one();
  }

  void enqueueImmediate(const std::string& script_line) {
    _queue.atomicOp([&script_line](std::deque<std::string>& q) {
      q.push_back(script_line);
    });
    _immediate_flush = true;
    _cv.notify_one();
  }

  void writerLoop() {
    while (_running) {
      std::unique_lock<std::mutex> lock(_cv_mutex);
      _cv.wait_for(lock, std::chrono::milliseconds(static_cast<int>(_flush_interval_ms)));
      flushQueue();
    }
  }

  void flushQueue() {
    std::deque<std::string> to_write;
    _queue.atomicOp([&to_write](std::deque<std::string>& q) {
      to_write.swap(q);
    });

    if (!to_write.empty() && _file.is_open()) {
      for (const auto& line : to_write) {
        _file << line;
      }
      _file.flush();
    }
    _immediate_flush = false;
  }

  static std::string escapeJsonString(const std::string& text) {
    std::string result;
    result.reserve(text.size() * 1.2);
    for (char c : text) {
      switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
          if (static_cast<unsigned char>(c) < 32) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
            result += buf;
          } else {
            result += c;
          }
          break;
      }
    }
    return result;
  }

  static std::string colorToHex(const fvec3& color) {
    char buf[8];
    int r = static_cast<int>(color.x * 255);
    int g = static_cast<int>(color.y * 255);
    int b = static_cast<int>(color.z * 255);
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return std::string(buf);
  }

  static std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf;
    localtime_r(&time, &tm_buf);

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
             tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
             static_cast<int>(ms.count()));
    return std::string(buf);
  }

  std::string formatEntry(const LogChannel* chan, const std::string& msg,
                          const std::string& type = "",
                          const std::string& subchannel = "") {
    std::ostringstream ss;
    ss << "<script>E({";
    ss << "\"ts\":\"" << getTimestamp() << "\"";
    ss << ",\"ch\":\"" << escapeJsonString(chan->_name) << "\"";
    ss << ",\"color\":\"" << colorToHex(chan->_color) << "\"";
    if (!type.empty()) {
      ss << ",\"type\":\"" << type << "\"";
    }
    if (!subchannel.empty()) {
      ss << ",\"sub\":\"" << escapeJsonString(subchannel) << "\"";
    }
    ss << ",\"msg\":\"" << escapeJsonString(msg) << "\"";
    ss << "})</script>\n";
    return ss.str();
  }
};

////////////////////////////////////////////////////////////////
// HTML backend function pointers
// Try _backend_impl first (set by fork backend), fall back to logger's backend
////////////////////////////////////////////////////////////////

static html_backend_impl_ptr_t getHtmlImpl(const LogChannel* chan) {
  auto impl = chan->_backend_impl.tryAs<html_backend_impl_ptr_t>();
  if (impl) return impl.value();
  impl = chan->_logger->_backend->_impl.tryAs<html_backend_impl_ptr_t>();
  if (impl) return impl.value();
  return nullptr;
}

static void htmlLogFn(const LogChannel* chan, const std::string& str) {
  auto impl = getHtmlImpl(chan);
  if (impl) {
    impl->enqueue(impl->formatEntry(chan, str));
  }
}

static void htmlWarnFn(const LogChannel* chan, const std::string& str) {
  auto impl = getHtmlImpl(chan);
  if (impl) {
    impl->enqueueImmediate(impl->formatEntry(chan, str, "warn"));
  }
}

static void htmlErrorFn(const LogChannel* chan, const std::string& str) {
  auto impl = getHtmlImpl(chan);
  if (impl) {
    impl->enqueueImmediate(impl->formatEntry(chan, str, "error"));
  }
}

static void htmlStatusFn(const LogChannel* chan, std::string subchannel, const std::string& str) {
  auto impl = getHtmlImpl(chan);
  if (impl) {
    impl->enqueue(impl->formatEntry(chan, str, "status", subchannel));
  }
}

static void htmlPerfItemFn(const LogChannel* chan, std::string name, svar64_t& data) {
  // Performance items could be logged but skip for now
}

////////////////////////////////////////////////////////////////
// Factory function
////////////////////////////////////////////////////////////////

logger_backend_ptr_t createHtmlBackend(const std::string& path, float flush_interval_ms) {
  auto backend = std::make_shared<LoggerBackend>();
  auto impl = std::make_shared<HtmlBackendImpl>(path, flush_interval_ms);

  impl->start();

  backend->_impl = impl;
  backend->_add_log_line = htmlLogFn;
  backend->_begin_log_line = htmlLogFn;
  backend->_continue_log_line = htmlLogFn;
  backend->_end_log_line = htmlLogFn;
  backend->_warn = htmlWarnFn;
  backend->_error = htmlErrorFn;
  backend->_status = htmlStatusFn;
  backend->_on_perf_item = htmlPerfItemFn;

  return backend;
}

////////////////////////////////////////////////////////////////
} // namespace ork
