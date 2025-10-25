#pragma once 
// #define ENABLE_NOTCURSES_UI

#if defined(ENABLE_NOTCURSES_UI)
#include <ork/math/cvector3.h>
#include <ork/util/crc.h>
#include <ork/kernel/svariant.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>
#include <ork/util/ringbuffer.inl>
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <set>
#include <notcurses/notcurses.h>

namespace ork::notcurses {
  struct Context;
  struct Widget;
  struct RootWidget;
  struct Group;
  struct ComboBox;
  struct Box;
  struct Button;
  struct TabGroup;
  struct TextLines;
  struct HorizSplit;
  struct VerticalSplit;
  struct HorizontalPack;
  struct VerticalPack;
  struct OPQVisualizerWidget;
  struct TextEdit;
  struct IntEdit;
  struct FloatEdit;
  struct BoolEdit;

  using context_ptr_t = std::shared_ptr<Context>;
  using widget_ptr_t = std::shared_ptr<Widget>;
  using widget_wkptr_t = std::weak_ptr<Widget>;
  using root_widget_ptr_t = std::shared_ptr<RootWidget>;
  using group_ptr_t = std::shared_ptr<Group>;
  using widget_vect_t = std::vector<std::shared_ptr<Widget>>;
  using combobox_ptr_t = std::shared_ptr<ComboBox>;
  using box_ptr_t = std::shared_ptr<Box>;
  using textlines_ptr_t = std::shared_ptr<TextLines>;
  using button_ptr_t = std::shared_ptr<Button>;
  using tabgroup_ptr_t = std::shared_ptr<TabGroup>;
  using opqviz_ptr_t = std::shared_ptr<OPQVisualizerWidget>;
  using horizontalsplit_ptr_t = std::shared_ptr<HorizSplit>;
  using verticalsplit_ptr_t = std::shared_ptr<VerticalSplit>;
  using horizontalpack_ptr_t = std::shared_ptr<HorizontalPack>;
  using verticalpack_ptr_t = std::shared_ptr<VerticalPack>;
  using textedit_ptr_t = std::shared_ptr<TextEdit>;
  using intedit_ptr_t = std::shared_ptr<IntEdit>;
  using floatedit_ptr_t = std::shared_ptr<FloatEdit>;
  using booledit_ptr_t = std::shared_ptr<BoolEdit>;
  /////////////////////////////////////////
  // Text Alignment Enums
  /////////////////////////////////////////

  enum struct HorizontalAlign : crc_enum_t {
    CrcEnum(left),
    CrcEnum(center), 
    CrcEnum(right),
  };

  enum struct VerticalAlign : crc_enum_t {
    CrcEnum(top),
    CrcEnum(center),
    CrcEnum(bottom),
  };

  /////////////////////////////////////////
  // Context:
  //.  owns the root widget and manages the NotCurses state
  //   can be used to create and manage widgets
  /////////////////////////////////////////

  // Forward declaration for RAII wrapper
  struct NotCursesRAII;
  
  // Global mouse state for proper interactive widget support
  struct GlobalMouseState {
    // Current state
    int mouse_x = 0;
    int mouse_y = 0;
    bool button1_down = false;
    
    // Widget tracking (found via recursive descent)
    std::weak_ptr<Widget> focused_widget;        // Widget mouse is currently over
    std::weak_ptr<Widget> pressed_widget;        // Widget that received button press
    std::weak_ptr<Widget> last_focused_widget;   // For generating leave events
    std::weak_ptr<Widget> keyboard_focused_widget; // Widget that has keyboard focus
  };
  
  struct Context {
    Context(); // Add constructor declaration
    ~Context(); // Add destructor declaration
    void _init(); // initialize the NotCurses context
    void _shutdown(); // shutdown the NotCurses context (threading only)
    void _run(); // run the UI event loop
    bool isReady() const;
    
    // Global mouse state management
    std::shared_ptr<Widget> findWidgetAtPosition(int x, int y);
    std::shared_ptr<Widget> findWidgetAtPositionRecursive(std::shared_ptr<Widget> widget, int x, int y);
    void processGlobalMouseEvents(uint32_t c, struct ncinput ni);
    
    // Keyboard focus management
    void setKeyboardFocus(std::shared_ptr<Widget> widget);
    std::shared_ptr<Widget> getKeyboardFocused() const;
    
    // NotCurses context (declared FIRST, destroyed LAST via RAII)
    std::unique_ptr<NotCursesRAII> _notcurses;
    
    // Variant-based storage for flexible backend implementations
    svar64_t _impl;
    
    // Threading state (manual cleanup needed)
    std::atomic<bool> _running{false};
    std::thread _ui_thread; // thread for the UI event loop
    std::mutex _ui_mutex;
    std::condition_variable _cv;
    
    // UI state
    unsigned int _numrows = 0;
    unsigned int _numcols = 0;
    struct ncplane* _stdplane = nullptr; // reference only, not owned
    struct ncplane* _overlay_plane = nullptr; // system overlay plane (owned)
    
    // Widgets (declared LAST, destroyed FIRST before NotCurses)
    root_widget_ptr_t _root_widget;
    
    // Global mouse state for interactive widgets
    GlobalMouseState _global_mouse_state;
    
    // Quit management
    std::atomic<bool> _quit_requested{false};
    void waitForExit();
    void requestQuit();
    
    // System overlay management
    void _createSystemOverlay();
    void _destroySystemOverlay();
    void _drawSystemOverlay();

  };

  /////////////////////////////////////////
  // Widget:
  //  base class for all UI widgets
  //   provides a draw and input methods that can be overridden
  /////////////////////////////////////////

  struct Widget {
    Widget();
    virtual ~Widget() = default;

    void draw(); // template method for drawing
    void resize(int width, int height); // template method for resizing
    void onInput(uint32_t c, struct ncinput ni); // template method for input events
    void setPosition(int x, int y); // set widget position
    int height() const; // get widget height
    void clearArea(); // clear widget's rectangular area
    
    // Widget tree interface for recursive descent
    virtual std::vector<std::shared_ptr<Widget>> children() const { return {}; }
    
    // Parent management interface
    widget_wkptr_t getParent() const;
    widget_wkptr_t getRoot() const;
    std::vector<widget_wkptr_t> getAncestors() const;
    bool isChildOf(widget_wkptr_t potential_parent) const;
    
    // Global mouse event interface (for interactive widgets)
    virtual bool wantsGlobalMouseTracking() const { return false; }
    virtual void onGlobalMouseEvent(const std::string& event_type, int mouse_x, int mouse_y, 
                                   bool button1_down, uint32_t original_key, struct ncinput original_ni) {}
    
    // render the widget
    virtual void _doDraw() = 0;
    virtual void _onLayoutChanged() = 0; // handle resize events (recursive resize of children if present)
    virtual void _onInput(uint32_t c, struct ncinput ni) = 0; // handle input events
    // input handling can be added later
    int _x = 0; // x position
    int _y = 0; // y position
    int _width = 0; // width of the widget
    int _height = 0; // height of the widget
    bool _layout_dirty = true; // flag to indicate if layout needs to be redrawn
    widget_wkptr_t _parent; // parent widget weak pointer

    // Internal parent management methods
    void _setParent(widget_wkptr_t parent);
    void _clearParent();

    void _markLayoutDirty();
    
    // Common drawing helpers
    bool _validateContext() const;
    uint32_t _colorToUint32(const ork::fvec3& color) const;
    void _setColors(const ork::fvec3& fg_color, const ork::fvec3& bg_color) const;
    
    // Drawing primitives
    void drawFilledBox(int x, int y, int w, int h, const ork::fvec3& color) const;
    void drawOutlineBox(int x, int y, int w, int h, const ork::fvec3& color) const;
    void drawOutlineCharBox(int x, int y, int w, int h, const ork::fvec3& bgcolor, const ork::fvec3& fgcolor, char cell) const;
    void drawLine(int x1, int y1, int x2, int y2, const ork::fvec3& color) const;
    void drawHLine(int x, int y, int length, const ork::fvec3& color) const;
    void drawVLine(int x, int y, int length, const ork::fvec3& color) const;

    std::string _name;
  };

  /////////////////////////////////////////
  // Group:
  //  a widget that contains other widgets
  //   can manage child widgets
  /////////////////////////////////////////

  struct Group : public Widget {
    Group();
    void _onLayoutChanged() override;
    void _onInput(uint32_t c, struct ncinput ni) override;    
  };

  /////////////////////////////////////////
  // RootWidget:
  //  the top-level widget that contains all other widgets
  //   serves as the entry point for the UI
  /////////////////////////////////////////

  struct RootWidget : public Widget {
    RootWidget();
    void _doDraw() final; // draw the content
    void _onLayoutChanged() final;
    void _onInput(uint32_t c, struct ncinput ni) final;
    void setContent(widget_ptr_t content, widget_ptr_t parent_container);
    widget_ptr_t swapContent(widget_ptr_t new_content, widget_ptr_t parent_container); // Swap current content with new content, returns old content
    
    // Widget tree interface
    widget_vect_t children() const final;
    widget_ptr_t _content; // the main content widget
    
    // System overlay interface (public for Context access)
    bool _isQuitButtonHit(int x, int y); // Check if quit button clicked
    
  private:
    void _setContentParent(widget_ptr_t content, widget_ptr_t parent_widget);
    void _clearContentParent(widget_ptr_t content);
  };


  /////////////////////////////////////////
  // TabGroup:
  //  a group that manages tabs
  //   allows switching between different content panes
  /////////////////////////////////////////
  
  struct TabItem {
    std::string _name; // tab name
    widget_ptr_t content; // content associated with this tab
    ork::fvec3 _color; // tab color
  };
  using tab_item_ptr_t = std::shared_ptr<TabItem>;
  struct TabGroup : public Group {
    TabGroup(); // Add constructor
    ~TabGroup(); // Add destructor
    void _doDraw() final;
    void _onLayoutChanged() final;
    void _onInput(uint32_t c, struct ncinput ni) final;
    void switchToTab(const std::string& name);
    void addTab(const std::string& name, widget_ptr_t content, widget_ptr_t parent_container, const ork::fvec3& color = ork::fvec3(1,1,1));
    widget_ptr_t getActiveContent() const;
    void clearContentArea(); // clear the content area below tab bar
    
    // Widget tree interface
    widget_vect_t children() const final;
    
    std::string _active_tab; 
    std::vector<tab_item_ptr_t> _tabs; // list of tabs
    struct ncplane* _tabbar = nullptr; // owned tab bar plane
    
  };

  /////////////////////////////////////////
  // TextBox:
  // a colored box widget with text
  /////////////////////////////////////////

  struct Box : public Widget {
    Box();
    ork::fvec3 _bgcolor; // color of the box
    ork::fvec3 _fgcolor; // color of the text
    std::string _text; // text to display inside the box
    HorizontalAlign _halign = HorizontalAlign::left; // horizontal alignment
    VerticalAlign _valign = VerticalAlign::top; // vertical alignment
    void _doDraw() final; // draw the box with the specified color
    void _onLayoutChanged() final;
    void _onInput(uint32_t c, struct ncinput ni) final;
    
  private:
    std::pair<int, int> _calculateTextPosition(const std::string& text, 
                                             int box_width, int box_height,
                                             HorizontalAlign halign, 
                                             VerticalAlign valign);
  };

  /////////////////////////////////////////
  // TextLines:
  // a widget that displays multiple lines of text
  //   can be used to display logs or messages
  //.  built in scrolling and text management
  /////////////////////////////////////////

  struct TextLines : public Widget {
    TextLines();
    ork::fvec3 _color; // color of the text
    ork::fvec3 _bg_color; // background color of the text box
    std::vector<std::string> _lines; // lines of text to display
    size_t _max_lines = 1000; // maximum number of lines to keep in memory
    size_t _scroll_offset = 0; // current scroll position
    bool _auto_scroll = true; // auto-scroll to bottom when new lines are added
    bool _user_has_scrolled = false; // track if user has manually scrolled
    void _doDraw() final; // draw the text lines with scrolling support
    void _onLayoutChanged() final;
    void addLine(const std::string& line); // add a new line of text
    void setLines(const std::vector<std::string>& lines); // set all lines at once
    void clear(); // clear all lines
    void setColor(const ork::fvec3& color); // set text color
    void setScrolling(bool enabled); // enable/disable scrolling
    void setAutoScroll(bool enabled); // enable/disable auto-scroll to bottom
    void _scroll(int direction); // scroll up or down
    void _onInput(uint32_t c, struct ncinput ni) final;
   };

  /////////////////////////////////////////
  // HorizSplit:
  // a horizontal split panel that contains two child widgets
  //  can be used to create a split view
  //  draws a vertical line between the two widgets
  // user can adjust the split position with mouse
  /////////////////////////////////////////

  struct HorizSplit : public Group {
    HorizSplit();
    ork::fvec3 _split_color; // background color of the splitter
    widget_ptr_t _left; // left child widget
    widget_ptr_t _right; // right child widget
    float _split_position = 0.5f; // position of the split (0.0 to 1.0)
    void _doDraw() final; // draw the split panel with a horizontal line
    void _onLayoutChanged() final; // handle resizing of the split panel
    void _onInput(uint32_t c, struct ncinput ni) final;
    
    // Widget tree interface
    widget_vect_t children() const final;
    
    // Parent-aware setters
    void setLeft(widget_ptr_t widget, widget_ptr_t parent_container);
    void setRight(widget_ptr_t widget, widget_ptr_t parent_container);
    
  };

  /////////////////////////////////////////
  // VerticalSplit:
  // a vertical split panel that contains two child widgets
  //  can be used to create a split view
  //  draws a horizontal line between the two widgets
  // user can adjust the split position with mouse
  /////////////////////////////////////////

  struct VerticalSplit : public Group {
    VerticalSplit();
    ork::fvec3 _split_color; // background color of the splitter
    widget_ptr_t _top; // top child widget
    widget_ptr_t _bottom; // bottom child widget
    float _split_position = 0.5f; // position of the split (0.0 to 1.0)
    void _doDraw() final; // draw the split panel with a horizontal line
    void _onLayoutChanged() final; // handle resizing of the split panel
    void _onInput(uint32_t c, struct ncinput ni) final;
    
    // Widget tree interface
    widget_vect_t children() const final;
    
    // Parent-aware setters
    void setTop(widget_ptr_t widget, widget_ptr_t parent_container);
    void setBottom(widget_ptr_t widget, widget_ptr_t parent_container);
    
  };

  /////////////////////////////////////////
  // VerticalPack:
  // a group that packs widgets vertically
  // preserves the order of widgets
  // preserves the vertical size of each widget
  // with equal spacing
  /////////////////////////////////////////

  struct VerticalPack : public Group {
    VerticalPack();
    void addChild(widget_ptr_t child, widget_ptr_t parent_container); // add a child widget with explicit parent
    void removeChild(widget_ptr_t child); // remove a child widget
    
    void _doDraw() final; // draw the packed widgets with vertical alignment
    void _onLayoutChanged() final;
    void _onInput(uint32_t c, struct ncinput ni) final;
    widget_vect_t children() const final;

    std::vector<widget_ptr_t> _children; // list of child widgets
    bool _spacing = true; // spacing between widgets (reduced from 5.0f)
    uint64_t _width_mode = "PROPAGATE_DOWN"_crcu;
    
  };

  /////////////////////////////////////////
  // HorizontalPack:
  // a group that packs widgets horizontally
  // preserves the order of widgets
  // preserves the horizontal size of each widget
  // with equal spacing
  /////////////////////////////////////////

  struct HorizontalPack : public Group {
    HorizontalPack();
    void addChild(widget_ptr_t child, widget_ptr_t parent_container); // add a child widget with explicit parent
    void removeChild(widget_ptr_t child); // remove a child widget

    void _doDraw() final; // draw the packed widgets with horizontal alignment
    void _onLayoutChanged() final;
    widget_vect_t children() const final;
    void _onInput(uint32_t c, struct ncinput ni) final;

    
    std::vector<widget_ptr_t> _children; // list of child widgets
    bool _spacing = true; // spacing between widgets (reduced from 5.0f)
    uint64_t _height_mode = "PROPAGATE_DOWN"_crcu;
  };

  /////////////////////////////////////////
  // Button:
  // an interactive button widget with click handling
  // supports different visual states and callbacks
  /////////////////////////////////////////

  struct Button : public Widget {
    Button();
    
    void _doDraw() final;
    void _onLayoutChanged() final;
    void _onInput(uint32_t c, struct ncinput ni) final;
    void _updateState();
    void _invokeCallback();
    
    // Global mouse event interface
    bool wantsGlobalMouseTracking() const final;
    void onGlobalMouseEvent(const std::string& event_type, int mouse_x, int mouse_y, 
                           bool button1_down, uint32_t original_key, struct ncinput original_ni) final;
    
    std::pair<int, int> _calculateTextPosition();

    // Visual properties
    std::string _text;
    ork::fvec3 _normal_bg_color = ork::fvec3(0.3f, 0.3f, 0.3f);
    ork::fvec3 _normal_fg_color = ork::fvec3(1.0f, 1.0f, 1.0f);
    ork::fvec3 _hover_bg_color = ork::fvec3(0.4f, 0.4f, 0.4f);
    ork::fvec3 _hover_fg_color = ork::fvec3(1.0f, 1.0f, 1.0f);
    ork::fvec3 _pressed_bg_color = ork::fvec3(0.2f, 0.2f, 0.2f);
    ork::fvec3 _pressed_fg_color = ork::fvec3(0.8f, 0.8f, 0.8f);
    ork::fvec3 _disabled_bg_color = ork::fvec3(0.1f, 0.1f, 0.1f);
    ork::fvec3 _disabled_fg_color = ork::fvec3(0.5f, 0.5f, 0.5f);
    
    // State management
    enum class State { Normal, Hover, Pressed, Disabled };
    State _state = State::Normal;
    bool _enabled = true;
    
    // Alignment
    HorizontalAlign _halign = HorizontalAlign::center;
    VerticalAlign _valign = VerticalAlign::center;
    
    // Callback system
    std::function<void()> _on_click;
    svar64_t _python_callback; // For Python integration
    
    // Mouse tracking
    bool _mouse_down = false;
    bool _mouse_over = false;

  };

  /////////////////////////////////////////
  // ComboBox:
  // a dropdown selection widget with keyboard and mouse support
  // displays a list of items and allows selection
  /////////////////////////////////////////

  struct ComboBox : public Widget {
    ComboBox();
    // Data model
    std::vector<std::string> _items;
    int _selected_index = -1;
    
    // Visual properties
    ork::fvec3 _bg_color = ork::fvec3(0.2f, 0.2f, 0.2f);
    ork::fvec3 _fg_color = ork::fvec3(1.0f, 1.0f, 1.0f);
    ork::fvec3 _selected_bg_color = ork::fvec3(0.1f, 0.3f, 0.6f);
    ork::fvec3 _selected_fg_color = ork::fvec3(1.0f, 1.0f, 1.0f);
    ork::fvec3 _dropdown_bg_color = ork::fvec3(0.25f, 0.25f, 0.25f);
    ork::fvec3 _dropdown_fg_color = ork::fvec3(1.0f, 1.0f, 1.0f);
    ork::fvec3 _hover_bg_color = ork::fvec3(0.3f, 0.3f, 0.3f);
    ork::fvec3 _hover_fg_color = ork::fvec3(1.0f, 1.0f, 1.0f);
    
    // State management
    bool _is_open = false;
    int _hover_index = -1;
    int _max_visible_items = 8;
    int _scroll_offset = 0;
    
    // Callbacks
    std::function<void(int, const std::string&)> _on_selection_changed;
    svar64_t _python_callback; // For Python integration
    
    // Mouse tracking
    bool _mouse_over_dropdown = false;
    
    void _doDraw() final;
    void _onLayoutChanged() final;
    void _onInput(uint32_t c, struct ncinput ni) final;
    void _openDropdown();
    void _closeDropdown();
    void _selectItem(int index);
    void _scrollDropdown(int direction);
    void _invokeCallback(int index, const std::string& value);
    int _getDropdownHeight() const;
    bool _isMouseInDropdown(int mouse_x, int mouse_y) const;
    
    // Global mouse event interface
    bool wantsGlobalMouseTracking() const final;
    void onGlobalMouseEvent(const std::string& event_type, int mouse_x, int mouse_y, 
                           bool button1_down, uint32_t original_key, struct ncinput original_ni) override;
    
  private:
    void _drawClosed();
    void _drawOpen();
    void _drawDropdownItem(int item_index, int draw_y, bool is_hovered, bool is_selected);
  };

  /////////////////////////////////////////
  // OPQVisualizerWidget:
  // a widget that displays OPQ (Operations Queue) performance metrics
  // shows ops/sec, latency, thread count, and queue status in real-time
  /////////////////////////////////////////

  struct OPQVisualizerWidget : public Widget {
    OPQVisualizerWidget();
    ~OPQVisualizerWidget();
    
    void setTargetOPQ(opq::opq_ptr_t opq_ptr);
    void setUpdateInterval(float interval); 
        
    void _doDraw() final;
    void _onLayoutChanged() final;
    void _onInput(uint32_t c, struct ncinput ni) final;
    
    // Sparkline generation helpers
    std::string _generateSparkline(const RingBuffer<float>& history, float min_override = -1.0f, float max_override = -1.0f);
    std::string _generateSparklineInt(const RingBuffer<int>& history, int min_override = -1, int max_override = -1);
    
    opq::opq_ptr_t _target_opq;
    Timer _update_timer;
    float _update_interval = 1.0f;
    bool _compact_mode = true;
    
    // Performance data caching
    float _perf_update_interval = 0.5f;  // Cache update interval
    float _last_perf_update = 0.0f;      // Last cache update time
    opq::opq_perfdata_ptr_t _cached_perf_data; // Cached performance data
    
    // Historical data for sparklines (40 samples = 10 seconds @ 0.25s interval)
    RingBuffer<float> _history_ops_per_sec{40};
    RingBuffer<float> _history_avg_latency{40};
    RingBuffer<int> _history_thread_count{40};
    RingBuffer<int> _history_pending_ops{40};
    
    // Display state
    std::vector<std::string> _display_lines;
    ork::fvec3 _border_color = ork::fvec3(0.5f, 0.7f, 1.0f);
    ork::fvec3 _text_color = ork::fvec3(1.0f, 1.0f, 1.0f);
    ork::fvec3 _value_color = ork::fvec3(0.0f, 1.0f, 0.5f);
    ork::fvec3 _warning_color = ork::fvec3(1.0f, 1.0f, 0.0f);
    ork::fvec3 _error_color = ork::fvec3(1.0f, 0.0f, 0.0f);
        
  };

  /////////////////////////////////////////
  // TextEdit:
  // text input widget with cursor and keyboard focus
  // base class for type-specific edit widgets
  /////////////////////////////////////////

  struct TextEdit : public Widget, public std::enable_shared_from_this<TextEdit> {
    TextEdit();
    
    void _doDraw() final;
    void _onLayoutChanged() final;
    void _onInput(uint32_t c, struct ncinput ni) final;
    
    // Global mouse event interface
    bool wantsGlobalMouseTracking() const final;
    void onGlobalMouseEvent(const std::string& event_type, int mouse_x, int mouse_y, 
                           bool button1_down, uint32_t original_key, struct ncinput original_ni) final;
    
    // Text manipulation
    virtual void _insertChar(char c);
    virtual void _deleteChar();
    virtual void _backspace();
    virtual void _moveCursor(int delta);
    virtual bool _isValidContent() const;
    
    // Public members
    std::string _text;
    size_t _cursor_pos = 0;
    bool _has_focus = false;
    bool _is_valid = true;
    
    // Visual properties
    ork::fvec3 _normal_bg_color = ork::fvec3(0.1f, 0.1f, 0.1f);
    ork::fvec3 _normal_fg_color = ork::fvec3(1.0f, 1.0f, 1.0f);
    ork::fvec3 _focused_bg_color = ork::fvec3(0.2f, 0.2f, 0.2f);
    ork::fvec3 _focused_fg_color = ork::fvec3(1.0f, 1.0f, 1.0f);
    ork::fvec3 _invalid_bg_color = ork::fvec3(0.3f, 0.1f, 0.1f);
    ork::fvec3 _invalid_fg_color = ork::fvec3(1.0f, 0.8f, 0.8f);
    ork::fvec3 _border_color = ork::fvec3(0.5f, 0.5f, 0.5f);
    ork::fvec3 _focused_border_color = ork::fvec3(0.0f, 0.8f, 1.0f);
    ork::fvec3 _invalid_border_color = ork::fvec3(1.0f, 0.3f, 0.3f);
  };

  /////////////////////////////////////////
  // IntEdit:
  // integer input widget with validation
  /////////////////////////////////////////

  struct IntEdit : public TextEdit {
    IntEdit();
    
    bool _isValidContent() const final;
    
    // Value access
    int getValue() const;
    void setValue(int value);
    
    // Public members
    int _min_value = INT_MIN;
    int _max_value = INT_MAX;
  };

  /////////////////////////////////////////
  // FloatEdit:
  // float input widget with validation
  /////////////////////////////////////////

  struct FloatEdit : public TextEdit {
    FloatEdit();
    
    bool _isValidContent() const final;
    
    // Value access
    float getValue() const;
    void setValue(float value);
    
    // Public members
    float _min_value = -FLT_MAX;
    float _max_value = FLT_MAX;
    int _decimal_places = 2;
  };

  /////////////////////////////////////////
  // BoolEdit:
  // boolean toggle widget (checkbox style)
  /////////////////////////////////////////

  struct BoolEdit : public Widget, public std::enable_shared_from_this<BoolEdit> {
    BoolEdit();
    
    void _doDraw() final;
    void _onLayoutChanged() final;
    void _onInput(uint32_t c, struct ncinput ni) final;
    
    // Global mouse event interface  
    bool wantsGlobalMouseTracking() const final;
    void onGlobalMouseEvent(const std::string& event_type, int mouse_x, int mouse_y, 
                           bool button1_down, uint32_t original_key, struct ncinput original_ni) final;
    
    void _toggle();
    
    // Public members
    bool _value = false;
    std::string _label = "Checkbox";
    bool _has_focus = false;
    
    // Visual properties
    ork::fvec3 _normal_bg_color = ork::fvec3(0.1f, 0.1f, 0.1f);
    ork::fvec3 _normal_fg_color = ork::fvec3(1.0f, 1.0f, 1.0f);
    ork::fvec3 _focused_bg_color = ork::fvec3(0.2f, 0.2f, 0.2f);
    ork::fvec3 _focused_fg_color = ork::fvec3(1.0f, 1.0f, 1.0f);
    ork::fvec3 _checked_color = ork::fvec3(0.0f, 1.0f, 0.0f);
    ork::fvec3 _border_color = ork::fvec3(0.5f, 0.5f, 0.5f);
    ork::fvec3 _focused_border_color = ork::fvec3(0.0f, 0.8f, 1.0f);
  };

  void clearRectangle(int x, int y, int width, int height);
  context_ptr_t context();

}
#endif