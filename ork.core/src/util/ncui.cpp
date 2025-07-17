////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/ncui.h>
#include <thread>
#include <atomic>
#include <cstdio>
#include <errno.h>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <sstream>
#include <fstream>
#include <pybind11/pybind11.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>

#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

namespace py = pybind11;

namespace ork::notcurses {

  constexpr bool NC_DEBUG_MODE = false;


////////////////////////////////////////////////////////////////
// RAII wrapper for NotCurses context
////////////////////////////////////////////////////////////////

struct NotCursesRAII {
  struct notcurses* _nc = nullptr;
  FILE* _render_fp = nullptr;
  int _master_fd = -1;
  int _slave_fd = -1;
  std::thread _reader_thread;
  std::atomic<bool> _reader_running{false};
  std::ofstream _output_file;

  NotCursesRAII() = default;

  NotCursesRAII(notcurses_options& opts) {

    if(NC_DEBUG_MODE){
      // Create pseudo-terminal for debug mode
      if (openpty(&_master_fd, &_slave_fd, nullptr, nullptr, nullptr) == 0) {
        _render_fp = fdopen(_slave_fd, "w+");
        if (_render_fp) {
          setvbuf(_render_fp, nullptr, _IOLBF, 0);
          
          // Open output file
          _output_file.open("/tmp/notcurseout.txt", std::ios::out | std::ios::trunc);
          
          // Start reader thread to copy PTY output to file
          _reader_running = true;
          _reader_thread = std::thread([this]() {
            char buffer[4096];
            while (_reader_running) {
              fd_set read_fds;
              FD_ZERO(&read_fds);
              FD_SET(_master_fd, &read_fds);
              
              struct timeval timeout = {0, 100000}; // 100ms timeout
              if (::select(_master_fd + 1, &read_fds, nullptr, nullptr, &timeout) > 0) {
                ssize_t bytes = read(_master_fd, buffer, sizeof(buffer) - 1);
                if (bytes > 0) {
                  buffer[bytes] = '\0';
                  _output_file << buffer;
                  _output_file.flush();
                }
              }
            }
          });
        }
      }
    }
    else{
      _render_fp = fopen("/dev/tty", "w");
    }
    
    _nc = notcurses_init(&opts, _render_fp);
    if (_nc and not NC_DEBUG_MODE) {
      notcurses_mice_enable(_nc, NCMICE_ALL_EVENTS);
    }
  }

  ~NotCursesRAII() {
    if (_nc) {
      if(not NC_DEBUG_MODE){
        notcurses_mice_disable(_nc);
      }
      notcurses_stop(_nc);

      // Explicit terminal input mode reset
      fprintf(_render_fp,"\033[?1l");    // Disable application cursor keys
      fprintf(_render_fp,"\033[>4;0m");  // Reset modifyOtherKeys mode
      fprintf(_render_fp,"\033[?2004l"); // Disable bracketed paste
      fprintf(_render_fp,"\033[?1006l"); // Disable SGR mouse mode
      fprintf(_render_fp,"\033[?1015l"); // Disable urxvt mouse mode
      fprintf(_render_fp,"\033[?1003l"); // Disable mouse tracking
      fflush(_render_fp);

      _nc = nullptr;
    }
    
    if(NC_DEBUG_MODE){
      _reader_running = false;
      if (_reader_thread.joinable()) {
        _reader_thread.join();
      }
      if (_output_file.is_open()) {
        _output_file.close();
      }
      if (_master_fd >= 0) {
        close(_master_fd);
        _master_fd = -1;
      }
    }
    
    if(_render_fp){
      fclose(_render_fp);
      _render_fp = nullptr;
    }
  }

  operator struct notcurses *() const {
    return _nc;
  }
  struct notcurses* get() const {
    return _nc;
  }
  explicit operator bool() const {
    return _nc != nullptr;
  }
};

using ncraii_ptr_t = std::shared_ptr<NotCursesRAII>;

////////////////////////////////////////////////////////////////
// Utility function for clearing rectangular areas
////////////////////////////////////////////////////////////////

void clearRectangle(int x, int y, int width, int height) {
  auto ctx = context();
  if (!ctx || !ctx->_stdplane)
    return;

  ncplane_set_bg_rgb(ctx->_stdplane, 0x000000);
  ncplane_set_fg_rgb(ctx->_stdplane, 0x000000);

  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      ncplane_putchar_yx(ctx->_stdplane, y + row, x + col, ' ');
    }
  }
}

////////////////////////////////////////////////////////////////
// Global singleton context
////////////////////////////////////////////////////////////////

context_ptr_t context() {
  static context_ptr_t ctx = std::make_shared<Context>();
  return ctx;
}

////////////////////////////////////////////////////////////////
// Signal handlers and cleanup functions
////////////////////////////////////////////////////////////////

static std::atomic<bool> _cleanup_in_progress{false};
static std::atomic<bool> _context_was_initialized{false};

static void perform_cleanup() {
  // Prevent multiple cleanup attempts
  if (!_context_was_initialized) {
    return; // Context was never initialized, nothing to clean up
  }

  bool expected = false;
  if (!_cleanup_in_progress.compare_exchange_strong(expected, true)) {
    return; // Cleanup already in progress
  }

  try {
    if (auto ctx = context()) {
      ctx->_shutdown();
    }
  } catch (...) {
    // Ignore exceptions during cleanup
  }
}

////////////////////////////////////////////////////////////////

static void signal_handler(int sig) {
  perform_cleanup();
  // Reset to default handler and re-raise
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}

__attribute__((destructor)) static void destructor_cleanup() {
  perform_cleanup();
}

////////////////////////////////////////////////////////////////
// Context implementation
////////////////////////////////////////////////////////////////

Context::Context() {
  _context_was_initialized = true;
  _init();
}

////////////////////////////////////////////////////////////////

Context::~Context() {
  // Only call _shutdown if cleanup hasn't already been performed
  bool expected = false;
  if (_cleanup_in_progress.compare_exchange_strong(expected, true)) {
    _shutdown();
  }
}

////////////////////////////////////////////////////////////////

void Context::_init() {
  // Install signal handlers for cleanup
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // Initialize NotCurses via RAII wrapper
  notcurses_options opts = {};
  opts.flags             = NCOPTION_SUPPRESS_BANNERS;// | NCOPTION_PRESERVE_CURSOR;

  auto ncraii = _impl.makeShared<NotCursesRAII>(opts);

  _stdplane = notcurses_stdplane(ncraii->get());

  ncplane_dim_yx(_stdplane, &_numrows, &_numcols);
  ncplane_set_fg_rgb(_stdplane, 0xFFFFFF);
  ncplane_set_bg_rgb(_stdplane, 0x000000);

  // Use ncplane_set_base to set default cell
  ncplane_set_base(_stdplane, " ", 0, NCCHANNELS_INITIALIZER(0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00));

  // Enable cursor to show standard arrow cursor even with mouse tracking
  notcurses_cursor_enable(ncraii->get(), 0, 0);

  // Create root widget
  _root_widget = std::make_shared<RootWidget>();
  _root_widget->resize(_numcols, _numrows);

  // Create system overlay plane
  _createSystemOverlay();

  _running   = true;
  _ui_thread = std::thread([this]() { this->_run(); });
}

////////////////////////////////////////////////////////////////

void Context::_shutdown() {
  // Only handle threading cleanup - RAII handles NotCurses cleanup
  if (_running.exchange(false)) {
    _cv.notify_all();
    if (_ui_thread.joinable()) {
      _ui_thread.join();
    }

    // Cleanup system overlay plane
    _destroySystemOverlay();

    // Reset signal handlers to default
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
  }
  // NotCurses cleanup happens automatically via RAII destructor
}

////////////////////////////////////////////////////////////////

bool Context::isReady() const {
  return _running && _impl.isShared<NotCursesRAII>() && _stdplane;
}

////////////////////////////////////////////////////////////////

void Context::_run() {
  struct ncinput ni                                 = {};
  bool need_full_redraw                             = true;
  std::chrono::steady_clock::time_point last_redraw = std::chrono::steady_clock::now();
  
  // Key repeat suppression
  uint32_t last_key = 0;
  std::chrono::steady_clock::time_point last_key_time = std::chrono::steady_clock::now();
  
  auto ncraii = _impl.getShared<NotCursesRAII>();
  while (_running) {
    // Wait for input with timeout
    uint32_t c = notcurses_get_nblock(ncraii->get(), &ni);

    if (c == (uint32_t)-1) {
      // Timeout - check for updates
      std::unique_lock<std::mutex> lock(_ui_mutex);
      _cv.wait_for(lock, std::chrono::milliseconds(50));
    } else if (c == NCKEY_RESIZE) {
      ncplane_dim_yx(_stdplane, &_numrows, &_numcols);
      _root_widget->resize(_numcols, _numrows);
      need_full_redraw = true;
    } else {
      // Filter out key release events - only process key press events
      bool is_key_release = (ni.evtype == NCTYPE_RELEASE && ni.id != NCKEY_BUTTON1);
      
      // Key repeat suppression - ignore rapid repeats of the same key
      auto now = std::chrono::steady_clock::now();
      bool is_key_repeat = (c == last_key && 
                           c >= 32 && c <= 126 && // Only suppress printable chars
                           (now - last_key_time) < std::chrono::milliseconds(250));
      
      if (!is_key_release && !is_key_repeat) {
        last_key = c;
        last_key_time = now;
        
        // Phase 1: Process global mouse events for interactive widgets
        processGlobalMouseEvents(c, ni);

        // Phase 2: Route keyboard input to focused widget or traditional widget tree
        auto focused_widget = getKeyboardFocused();
        
        // Check if this is a mouse event (mouse events have specific button IDs and event types)
        bool is_mouse_event = (ni.id == NCKEY_BUTTON1 && (ni.evtype == NCTYPE_PRESS || ni.evtype == NCTYPE_RELEASE)) ||
                              (ni.id == NCKEY_BUTTON4 || ni.id == NCKEY_BUTTON5 || ni.id == NCKEY_SCROLL_UP || ni.id == NCKEY_SCROLL_DOWN);
        
        if (focused_widget && !is_mouse_event) {
          // Send keyboard input directly to focused widget
          focused_widget->onInput(c, ni);
        } else {
          // Route input to traditional widget tree for mouse events or when no focus
          _root_widget->onInput(c, ni);
        }
      }
      
      if (now - last_redraw > std::chrono::milliseconds(500)) {
        need_full_redraw = true;
        last_redraw      = now;
      }
    }

    // Always redraw
    {
      std::lock_guard<std::mutex> lock(_ui_mutex);
      if (need_full_redraw) {
        // Force complete redraw
        notcurses_refresh(ncraii->get(), nullptr, nullptr);
        need_full_redraw = false;
      }
      _root_widget->draw();
      _drawSystemOverlay(); // Draw overlay on top
      notcurses_render(ncraii->get()); // Move render inside mutex for atomicity
    }
  }
}

////////////////////////////////////////////////////////////////
// Global mouse state management - Recursive Descent
////////////////////////////////////////////////////////////////

std::shared_ptr<Widget> Context::findWidgetAtPosition(int x, int y) {
  if (!_root_widget || !_root_widget->_content) {
    return nullptr;
  }
  return findWidgetAtPositionRecursive(_root_widget->_content, x, y);
}

////////////////////////////////////////////////////////////////

std::shared_ptr<Widget> Context::findWidgetAtPositionRecursive(std::shared_ptr<Widget> widget, int x, int y) {
  if (!widget) {
    return nullptr;
  }

  // Bounds check first - widget must contain the point
  if (x < widget->_x || x >= widget->_x + widget->_width || y < widget->_y || y >= widget->_y + widget->_height) {
    return nullptr;
  }

  // Search children depth-first (front-to-back order)
  // Children that come later in the list are drawn on top
  auto children = widget->children();
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    auto& child = *it;
    if (auto found = findWidgetAtPositionRecursive(child, x, y)) {
      return found; // Return the deepest/frontmost widget found
    }
  }

  // If no child found and this widget wants mouse tracking, return it
  if (widget->wantsGlobalMouseTracking()) {
    return widget;
  }

  // Otherwise, this widget doesn't handle mouse events
  return nullptr;
}

////////////////////////////////////////////////////////////////

void Context::processGlobalMouseEvents(uint32_t c, struct ncinput ni) {
  // Only process mouse events
  if (ni.id != NCKEY_BUTTON1 && ni.evtype != NCTYPE_RELEASE && ni.evtype != NCTYPE_PRESS) {
    return;
  }

  // Update global mouse state
  int old_x             = _global_mouse_state.mouse_x;
  int old_y             = _global_mouse_state.mouse_y;
  bool old_button1_down = _global_mouse_state.button1_down;

  _global_mouse_state.mouse_x = ni.x;
  _global_mouse_state.mouse_y = ni.y;

  // Update button state
  if (ni.id == NCKEY_BUTTON1) {
    if (ni.evtype == NCTYPE_PRESS) {
      _global_mouse_state.button1_down = true;
    } else if (ni.evtype == NCTYPE_RELEASE) {
      _global_mouse_state.button1_down = false;
    }
  }
  
  // Check for quit button click
  if (ni.id == NCKEY_BUTTON1 && ni.evtype == NCTYPE_RELEASE) {
    if (_root_widget && _root_widget->_isQuitButtonHit(ni.x, ni.y)) {
      requestQuit();
      return;
    }
  }

  // Find widget at current position
  auto current_widget = findWidgetAtPosition(ni.x, ni.y);
  auto last_focused   = _global_mouse_state.last_focused_widget.lock();
  auto pressed_widget = _global_mouse_state.pressed_widget.lock();

  // Generate synthetic events
  std::vector<std::pair<std::shared_ptr<Widget>, std::string>> events;

  // Mouse enter/leave events
  if (current_widget != last_focused) {
    if (last_focused) {
      events.push_back({last_focused, "mouse_leave"});
    }
    if (current_widget) {
      events.push_back({current_widget, "mouse_enter"});
    }
    _global_mouse_state.last_focused_widget = current_widget;
  }

  // Mouse press/release events
  if (ni.id == NCKEY_BUTTON1) {
    if (ni.evtype == NCTYPE_PRESS && current_widget) {
      _global_mouse_state.pressed_widget = current_widget;
      events.push_back({current_widget, "mouse_press"});
    } else if (ni.evtype == NCTYPE_RELEASE && pressed_widget) {
      events.push_back({pressed_widget, "mouse_release"});
      _global_mouse_state.pressed_widget.reset();
    }
  }

  // Mouse move events (for widgets that are pressed or focused)
  if (ni.x != old_x || ni.y != old_y) {
    if (current_widget) {
      events.push_back({current_widget, "mouse_move"});
    }
    if (pressed_widget && pressed_widget != current_widget) {
      events.push_back({pressed_widget, "mouse_move"});
    }
  }

  // Dispatch events to widgets
  for (const auto& [widget, event_type] : events) {
    if (widget && widget->wantsGlobalMouseTracking()) {
      widget->onGlobalMouseEvent(event_type, ni.x, ni.y, _global_mouse_state.button1_down, c, ni);
    }
  }
}

////////////////////////////////////////////////////////////////
// Keyboard focus management
////////////////////////////////////////////////////////////////

void Context::setKeyboardFocus(std::shared_ptr<Widget> widget) {
  // Clear focus from previous widget if it exists
  auto previous_focused = _global_mouse_state.keyboard_focused_widget.lock();
  if (previous_focused) {
    // Check if it's a TextEdit and clear its focus flag
    if (auto text_edit = std::dynamic_pointer_cast<TextEdit>(previous_focused)) {
      text_edit->_has_focus = false;
    } else if (auto bool_edit = std::dynamic_pointer_cast<BoolEdit>(previous_focused)) {
      bool_edit->_has_focus = false;
    }
  }
  
  // Set new focused widget
  _global_mouse_state.keyboard_focused_widget = widget;
  
  // Set focus flag for new widget if it's an edit widget
  if (widget) {
    if (auto text_edit = std::dynamic_pointer_cast<TextEdit>(widget)) {
      text_edit->_has_focus = true;
    } else if (auto bool_edit = std::dynamic_pointer_cast<BoolEdit>(widget)) {
      bool_edit->_has_focus = true;
    }
  }
}

std::shared_ptr<Widget> Context::getKeyboardFocused() const {
  return _global_mouse_state.keyboard_focused_widget.lock();
}

////////////////////////////////////////////////////////////////
// Quit management
////////////////////////////////////////////////////////////////

void Context::waitForExit() {
  while (!_quit_requested && _running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

void Context::requestQuit() {
  _quit_requested = true;
}

////////////////////////////////////////////////////////////////
// System overlay management
////////////////////////////////////////////////////////////////

void Context::_createSystemOverlay() {
  if (!_stdplane) return;
  
  // Create a small overlay plane for the quit button
  struct ncplane_options overlay_opts = {
    .y = 1, .x = 0,               // Position at (0,1) where user moved it
    .rows = 1, .cols = 2,         // Small 1x2 plane for "X "
    .userptr = nullptr,
    .name = "system_overlay",
    .resizecb = nullptr,
    .flags = 0
  };
  
  _overlay_plane = ncplane_create(_stdplane, &overlay_opts);
  
  if (_overlay_plane) {
    // Set default styling for overlay plane
    ncplane_set_fg_rgb(_overlay_plane, 0xffffff); // White text
    ncplane_set_bg_rgb(_overlay_plane, 0x800000); // Dark red background
  }
}

void Context::_destroySystemOverlay() {
  if (_overlay_plane) {
    ncplane_destroy(_overlay_plane);
    _overlay_plane = nullptr;
  }
}

void Context::_drawSystemOverlay() {
  if (!_overlay_plane) return;
  
  // Clear the overlay plane
  ncplane_erase(_overlay_plane);
  
  // Set colors
  ncplane_set_fg_rgb(_overlay_plane, 0xffffff); // White text  
  ncplane_set_bg_rgb(_overlay_plane, 0x800000); // Dark red background
  
  // Draw the quit button
  ncplane_putstr_yx(_overlay_plane, 0, 0, "X");
  
  // Always move overlay to top to ensure visibility
  ncplane_move_top(_overlay_plane);
}

////////////////////////////////////////////////////////////////
// Widget base class implementation
////////////////////////////////////////////////////////////////

Widget::Widget() {
}

void Widget::_setParent(widget_wkptr_t parent) {
  _parent = parent;
}

void Widget::_clearParent() {
  _parent.reset();
}

widget_wkptr_t Widget::getParent() const {
  return _parent;
}

widget_wkptr_t Widget::getRoot() const {
  widget_wkptr_t current_weak = _parent;
  widget_ptr_t current        = nullptr;

  // Start from this widget's parent and walk up
  while (!current_weak.expired()) {
    if (auto parent = current_weak.lock()) {
      current      = parent;
      current_weak = parent->_parent;
    } else {
      break;
    }
  }

  // If we have a current parent, return it as weak_ptr
  // If no parent found, return empty weak_ptr (this widget is root)
  return current ? widget_wkptr_t(current) : widget_wkptr_t();
}

std::vector<widget_wkptr_t> Widget::getAncestors() const {
  std::vector<widget_wkptr_t> ancestors;
  widget_wkptr_t current = _parent;

  while (!current.expired()) {
    ancestors.push_back(current);
    if (auto locked = current.lock()) {
      current = locked->_parent;
    } else {
      break;
    }
  }
  return ancestors;
}

bool Widget::isChildOf(widget_wkptr_t potential_parent) const {
  if (potential_parent.expired()) {
    return false;
  }

  auto potential_locked = potential_parent.lock();
  if (!potential_locked) {
    return false;
  }

  widget_wkptr_t current = _parent;
  while (!current.expired()) {
    if (auto locked = current.lock()) {
      if (locked == potential_locked) {
        return true;
      }
      current = locked->_parent;
    } else {
      break;
    }
  }
  return false;
}

void Widget::draw() {
  _onLayoutChanged();    // Resize children if needed
  clearArea();
  _doDraw();
}

void Widget::resize(int width, int height) {
  _width        = width;
  _height       = height;
  _layout_dirty = true; // Mark layout as dirty for redraw
  _onLayoutChanged();
}

void Widget::onInput(uint32_t c, struct ncinput ni) {
  _onInput(c, ni);
}

void Widget::setPosition(int x, int y) {
  _x = x;
  _y = y;
}

int Widget::height() const {
  return _height;
}

void Widget::clearArea() {
  clearRectangle(_x, _y, _width, _height);
}

void Widget::_markLayoutDirty() {
  _layout_dirty = true;
}

bool Widget::_validateContext() const {
  auto ctx = context();
  return ctx && ctx->isReady();
}

uint32_t Widget::_colorToUint32(const ork::fvec3& color) const {
  return ((uint32_t)(color.x * 255) << 16) | 
         ((uint32_t)(color.y * 255) << 8) | 
         ((uint32_t)(color.z * 255));
}

void Widget::_setColors(const ork::fvec3& fg_color, const ork::fvec3& bg_color) const {
  auto ctx = context();
  if (!ctx || !ctx->_stdplane) return;
  
  uint32_t fg = _colorToUint32(fg_color);
  uint32_t bg = _colorToUint32(bg_color);
  
  ncplane_set_fg_rgb(ctx->_stdplane, fg);
  ncplane_set_bg_rgb(ctx->_stdplane, bg);
}

////////////////////////////////////////////////////////////////
// Drawing primitives implementation
////////////////////////////////////////////////////////////////

void Widget::drawFilledBox(int x, int y, int w, int h, const ork::fvec3& color) const {
  auto ctx = context();
  if (!ctx || !ctx->_stdplane) return;
  
  uint32_t color_uint = _colorToUint32(color);
  ncplane_set_fg_rgb(ctx->_stdplane, color_uint);
  ncplane_set_bg_rgb(ctx->_stdplane, color_uint);
  
  // Optimized filled rectangle - use string of spaces for better performance
  std::string spaces(w, ' ');
  for (int row = 0; row < h; ++row) {
    ncplane_putstr_yx(ctx->_stdplane, y + row, x, spaces.c_str());
  }
}

void Widget::drawOutlineBox(int x, int y, int w, int h, const ork::fvec3& color) const {
  auto ctx = context();
  if (!ctx || !ctx->_stdplane) return;
  
  uint32_t color_uint = _colorToUint32(color);
  ncplane_set_fg_rgb(ctx->_stdplane, color_uint);
  ncplane_set_bg_rgb(ctx->_stdplane, 0x000000); // Black background
  
  if (w <= 0 || h <= 0) return;
  
  // Use Unicode box-drawing characters for better appearance
  const char* top_left = "┌";
  const char* top_right = "┐";
  const char* bottom_left = "└";
  const char* bottom_right = "┘";
  const char* horizontal = "─";
  const char* vertical = "│";
  
  // Top border
  ncplane_putstr_yx(ctx->_stdplane, y, x, top_left);
  for (int col = 1; col < w - 1; ++col) {
    ncplane_putstr_yx(ctx->_stdplane, y, x + col, horizontal);
  }
  if (w > 1) {
    ncplane_putstr_yx(ctx->_stdplane, y, x + w - 1, top_right);
  }
  
  // Side borders
  for (int row = 1; row < h - 1; ++row) {
    ncplane_putstr_yx(ctx->_stdplane, y + row, x, vertical);
    if (w > 1) {
      ncplane_putstr_yx(ctx->_stdplane, y + row, x + w - 1, vertical);
    }
  }
  
  // Bottom border
  if (h > 1) {
    ncplane_putstr_yx(ctx->_stdplane, y + h - 1, x, bottom_left);
    for (int col = 1; col < w - 1; ++col) {
      ncplane_putstr_yx(ctx->_stdplane, y + h - 1, x + col, horizontal);
    }
    if (w > 1) {
      ncplane_putstr_yx(ctx->_stdplane, y + h - 1, x + w - 1, bottom_right);
    }
  }
}

void Widget::drawOutlineCharBox(int x, int y, int w, int h, const ork::fvec3& bgcolor, const ork::fvec3& fgcolor, char cell) const {
  auto ctx = context();
  if (!ctx || !ctx->_stdplane) return;
  
  ncplane_set_fg_rgb(ctx->_stdplane, _colorToUint32(fgcolor));
  ncplane_set_bg_rgb(ctx->_stdplane, _colorToUint32(bgcolor)); // Black background  
  if (w <= 0 || h <= 0) return;
  
  // draw outline (not filled) box with only specified character 'cell'
  for(int row = 0; row < h; ++row) {
    for(int col = 0; col < w; ++col) {
      if(row == 0 || row == h - 1 || col == 0 || col == w - 1) {
        ncplane_putstr_yx(ctx->_stdplane, y + row, x + col, &cell);
      } else {
        //ncplane_putstr_yx(ctx->_stdplane, y + row, x + col, " ");
      }
    }
  }


}

void Widget::drawLine(int x1, int y1, int x2, int y2, const ork::fvec3& color) const {
  auto ctx = context();
  if (!ctx || !ctx->_stdplane) return;
  
  uint32_t color_uint = _colorToUint32(color);
  ncplane_set_fg_rgb(ctx->_stdplane, color_uint);
  ncplane_set_bg_rgb(ctx->_stdplane, 0x000000); // Black background
  
  // Check for special cases (horizontal/vertical lines) for optimization
  if (y1 == y2) {
    // Horizontal line
    drawHLine(std::min(x1, x2), y1, std::abs(x2 - x1) + 1, color);
    return;
  } else if (x1 == x2) {
    // Vertical line
    drawVLine(x1, std::min(y1, y2), std::abs(y2 - y1) + 1, color);
    return;
  }
  
  // General Bresenham's line algorithm
  int dx = std::abs(x2 - x1);
  int dy = std::abs(y2 - y1);
  int x_step = (x1 < x2) ? 1 : -1;
  int y_step = (y1 < y2) ? 1 : -1;
  int err = dx - dy;
  
  int x = x1;
  int y = y1;
  
  while (true) {
    ncplane_putstr_yx(ctx->_stdplane, y, x, "█");
    
    if (x == x2 && y == y2) break;
    
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x += x_step;
    }
    if (e2 < dx) {
      err += dx;
      y += y_step;
    }
  }
}

void Widget::drawHLine(int x, int y, int length, const ork::fvec3& color) const {
  auto ctx = context();
  if (!ctx || !ctx->_stdplane) return;
  
  uint32_t color_uint = _colorToUint32(color);
  ncplane_set_fg_rgb(ctx->_stdplane, color_uint);
  ncplane_set_bg_rgb(ctx->_stdplane, 0x000000); // Black background
  
  // Optimized horizontal line using repeated string
  std::string line;
  line.reserve(length * 3); // Unicode chars are typically 3 bytes
  for (int i = 0; i < length; ++i) {
    line += "─";
  }
  ncplane_putstr_yx(ctx->_stdplane, y, x, line.c_str());
}

void Widget::drawVLine(int x, int y, int length, const ork::fvec3& color) const {
  auto ctx = context();
  if (!ctx || !ctx->_stdplane) return;
  
  uint32_t color_uint = _colorToUint32(color);
  ncplane_set_fg_rgb(ctx->_stdplane, color_uint);
  ncplane_set_bg_rgb(ctx->_stdplane, 0x000000); // Black background
  
  // Optimized vertical line
  for (int i = 0; i < length; ++i) {
    ncplane_putstr_yx(ctx->_stdplane, y + i, x, "│");
  }
}

////////////////////////////////////////////////////////////////
// Group implementation
////////////////////////////////////////////////////////////////

Group::Group() {
  _name = "Group";
}
void Group::_onLayoutChanged() {
  // Base implementation - subclasses override
}

void Group::_onInput(uint32_t c, struct ncinput ni) {
  // Base implementation - subclasses override
}


} // namespace ork::notcurses