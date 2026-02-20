////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/property_sheet_model.h>
#include <ork/lev2/ui/style.h>
#include <ork/lev2/ui/scroll_controller.h>
#include <ork/kernel/sigslot2.h>
#include <functional>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// PropertyRow: A single property row with label and editor widget
////////////////////////////////////////////////////////////////////

struct PropertyRow : public Group {
  PropertyRow(const std::string& name, const std::string& key, int depth);

  void setLabel(const std::string& label);
  void setEditorWidget(widget_ptr_t editor);
  void setExpanded(bool expanded);
  bool isExpanded() const { return _expanded; }
  bool hasChildren() const { return _has_children; }
  void setHasChildren(bool has) { _has_children = has; }

  std::string _key;
  std::string _label;
  int _depth = 0;
  int _row_index = 0;  // For alternating row colors
  bool _expanded = true;
  bool _has_children = false;
  widget_ptr_t _editor_widget;

  // Appearance
  fvec4 _label_color = fvec4(0.9f, 0.9f, 0.9f, 1.0f);
  fvec4 _bg_color = fvec4(0.15f, 0.15f, 0.15f, 1.0f);
  fvec4 _alt_bg_color = fvec4(0.12f, 0.12f, 0.12f, 1.0f);  // Alternating row color
  int _label_width = 120;
  int _indent_width = 16;

  // Callbacks
  std::function<void()> _onExpandToggle;

  // Editor refresh (called when external value changes, e.g. manipulator)
  std::function<void(svar128_t new_value)> _refreshEditor;

  // Map property support
  bool _is_map_property = false;
  bool _is_map_const = false;
  std::function<void(event_constptr_t ev)> _onMapAdd;
  std::function<void(event_constptr_t ev)> _onMapRemove;
  std::function<void(event_constptr_t ev)> _onMapSelectItem;

  // Track which widget is being dragged (for proper event routing)
  Widget* _drag_capture = nullptr;

protected:
  void DoDraw(drawevent_constptr_t drwev) override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;
};

using property_row_ptr_t = std::shared_ptr<PropertyRow>;

////////////////////////////////////////////////////////////////////
// DetailEditorBinding: Abstraction for PropertySheet <-> DetailEditor communication
// Allows detail editors to work as overlays, popups, or docked panels
////////////////////////////////////////////////////////////////////

struct DetailEditorBinding {
  std::string property_key;                          // Which property is being edited
  PropertyType property_type = PropertyType::Unknown;
  svar128_t initial_value;                           // Value when editor opened

  // Callbacks from detail editor to property sheet
  std::function<void(svar128_t)> onValueChanged;     // Live updates during editing
  std::function<void(svar128_t)> onValueCommit;      // Final commit (e.g., on close)
  std::function<void()> onCancel;                    // Revert to initial value
  std::function<void()> onClose;                     // Close the detail editor
};

using detail_editor_binding_ptr_t = std::shared_ptr<DetailEditorBinding>;

////////////////////////////////////////////////////////////////////
// EditorFactory: Factory functions for creating property editors
// - Inline factory: Creates compact editor widget for the row
// - Detail factory: Creates full editor for the detail overlay
////////////////////////////////////////////////////////////////////

struct PropertySheet;  // Forward declaration for factory types
using property_sheet_ptr_t = std::shared_ptr<PropertySheet>;

// Inline editor factory: creates widget for property row
// Args: property_sheet, property_key, current_value, annotations
// Returns: widget for inline display (e.g., ColorSwatch, slider, checkbox)
using inline_editor_factory_t = std::function<widget_ptr_t(
    property_sheet_ptr_t sheet,
    const std::string& key,
    svar128_t value,
    varmap::varmap_ptr_t annotations)>;

// Detail editor factory: creates widget for detail overlay
// Args: property_sheet, property_key, current_value, annotations, binding for communication
// Returns: widget for detail editor (e.g., ColorPicker, CurveEditor)
using detail_editor_factory_t = std::function<widget_ptr_t(
    property_sheet_ptr_t sheet,
    const std::string& key,
    svar128_t value,
    varmap::varmap_ptr_t annotations,
    detail_editor_binding_ptr_t binding)>;

struct EditorFactoryPair {
  inline_editor_factory_t inline_factory;
  detail_editor_factory_t detail_factory;
};

////////////////////////////////////////////////////////////////////
// PropSheetEditorPropWidget: Shows an "Edit" button for properties
// with custom editor workflows (e.g. curve editors)
////////////////////////////////////////////////////////////////////

struct PropSheetEditorPropWidget final : public Widget {
  PropSheetEditorPropWidget(const std::string& name, const std::string& label = "Edit");
  HandlerResult DoOnUiEvent(event_constptr_t ev) final;
  void DoDraw(drawevent_constptr_t drwev) override;

  std::function<void()> _onEditRequested;
  std::string _label = "Edit";
  fvec4 _bg_color = fvec4(0.25f, 0.25f, 0.3f, 1.0f);
  fvec4 _hover_color = fvec4(0.35f, 0.35f, 0.4f, 1.0f);
  fvec4 _down_color = fvec4(0.15f, 0.15f, 0.2f, 1.0f);
  fvec4 _fg_color = fvec4(0.9f, 0.9f, 0.9f, 1.0f);
  bool _pressed = false;
  bool _hovering = false;
};

////////////////////////////////////////////////////////////////////
// PropertySheet: A hierarchical property editor widget
// - Uses PropertySheetModel for data
// - Creates appropriate editor widgets for each property type
// - Supports expand/collapse of groups
// - Has detail overlay slot for complex editors (color picker, etc.)
////////////////////////////////////////////////////////////////////

struct PropertySheet : public Group {
  PropertySheet(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~PropertySheet();

  // Model management
  void setModel(property_sheet_model_ptr_t model);
  property_sheet_model_ptr_t getModel() const { return _model; }

  // VarMap convenience API
  void setData(varmap::varmap_ptr_t data);
  varmap::varmap_ptr_t getData() const;

  // Expand/collapse
  void setExpanded(const std::string& key, bool expanded);
  bool isExpanded(const std::string& key) const;
  void expandAll();
  void collapseAll();

  // Rebuild the widget tree from model
  void rebuild();

  // Refresh a specific row's editor widget value (no rebuild)
  void refreshValue(const std::string& key);

  //////////////////////////////////////////////////////////////
  // Editor Factory Registry
  //////////////////////////////////////////////////////////////

  // Register editor factories for a property type (by CRC token)
  // This allows both built-in and custom property types
  void registerEditorFactory(
      uint32_t type_crc,
      inline_editor_factory_t inline_factory,
      detail_editor_factory_t detail_factory = nullptr);

  // Convenience overload using PropertyType enum
  void registerEditorFactory(
      PropertyType type,
      inline_editor_factory_t inline_factory,
      detail_editor_factory_t detail_factory = nullptr) {
    registerEditorFactory(propertyTypeToCrc(type), inline_factory, detail_factory);
  }

  // Check if a factory is registered for a type
  bool hasEditorFactory(uint32_t type_crc) const;
  bool hasEditorFactory(PropertyType type) const {
    return hasEditorFactory(propertyTypeToCrc(type));
  }

  //////////////////////////////////////////////////////////////
  // Detail Editor Overlay
  //////////////////////////////////////////////////////////////

  // Show a detail editor for a property (overlay mode)
  // The widget will be positioned in the detail area and receive event priority
  void showDetailEditor(const std::string& key, widget_ptr_t editor);

  // Close the active detail editor
  void closeDetailEditor();

  // Check if detail editor is active
  bool isDetailEditorActive() const { return _detail_editor != nullptr; }

  // Get the current binding (for detail editor to communicate back)
  detail_editor_binding_ptr_t getDetailBinding() const { return _detail_binding; }

  // Request detail editor for a property (triggers factory or callback)
  void requestDetailEditor(const std::string& key);

  // Callback when detail editor should be shown (for Python-side creation)
  // Called if no detail factory is registered for the type
  std::function<void(const std::string& key, PropertyType type, svar128_t value)> _onRequestDetailEditor;

  // Callback when a property with editor.custom annotation is clicked
  // Args: property key, editor identifier (annotation value)
  // Python side hooks this to open custom editors
  std::function<void(const std::string& key, const std::string& editor_id)> _onRequestCustomEditor;

  // Callbacks
  std::function<void(const std::string& key, svar128_t value)> _onPropertyChanged;

  // Appearance
  int _row_height = 24;
  int _label_width = 120;
  int _indent_width = 16;
  fvec4 _bgcolor = fvec4(0.1f, 0.1f, 0.1f, 1.0f);
  fvec4 _label_color = fvec4(0.9f, 0.9f, 0.9f, 1.0f);
  fvec4 _group_color = fvec4(0.2f, 0.2f, 0.25f, 1.0f);
  lev2::font_ptr_t _font;

protected:
  void DoDraw(drawevent_constptr_t drwev) override;
  void DoLayout() override;
  void _doOnResized() override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

  // Map view state per map property
  struct MapViewState {
    bool single_mode = true;   // true = show one item, false = show all
    int selected_index = 0;    // which item to show in single mode
  };
  std::unordered_map<std::string, MapViewState>& mapViewStates() { return _map_view_states; }

private:
  void _subscribeToModel();
  void _rebuildRows();
  void _addRowsRecursive(const std::string& parent_key, int depth, int& y_offset, int& row_index);
  void _addSingleChildRecursive(const std::string& child_key, int depth, int& y_offset, int& row_index);
  widget_ptr_t _createEditorWidget(const std::string& key, PropertyType type, svar128_t value);
  std::function<void(svar128_t)> _makeRefreshCallback(widget_ptr_t editor, PropertyType type);
  void _clampScrollOffset();

  property_sheet_model_ptr_t _model;
  std::unordered_set<std::string> _expanded_keys;
  std::unordered_map<std::string, property_row_ptr_t> _rows;
  std::unordered_map<std::string, MapViewState> _map_view_states;
  bool _needs_rebuild = true;
  int _total_rows = 0;  // For scroll calculation
  ScrollController _scroller;

  // Editor factory registry (keyed by property type CRC)
  std::unordered_map<uint32_t, EditorFactoryPair> _editor_factories;

  // Stale widget cache: holds old children for one frame after rebuild
  // so that raw pointers in Context (e.g. _mousefocuswidget) remain valid
  // through the current event processing cycle.
  std::vector<widget_ptr_t> _stale_widgets;

  // Signal connection for external value change notifications
  sigslot2::scoped_connection _external_value_connection;

public:
  // Detail editor overlay (public for Python bindings)
  widget_ptr_t _detail_editor;                       // Currently active detail editor
  detail_editor_binding_ptr_t _detail_binding;       // Communication binding

  // Detail overlay appearance/layout
  float _detail_height_ratio = 0.5f;                 // Portion of height for detail editor (0.0-1.0)
  int _detail_min_height = 100;                      // Minimum height for detail editor
  fvec4 _detail_bg_color = fvec4(0.15f, 0.15f, 0.18f, 1.0f);
};

} // namespace ork::ui
