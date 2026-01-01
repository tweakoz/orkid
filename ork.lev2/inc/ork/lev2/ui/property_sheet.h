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

  // Track which widget is being dragged (for proper event routing)
  Widget* _drag_capture = nullptr;

protected:
  void DoDraw(drawevent_constptr_t drwev) override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;
};

using property_row_ptr_t = std::shared_ptr<PropertyRow>;

////////////////////////////////////////////////////////////////////
// PropertySheet: A hierarchical property editor widget
// - Uses PropertySheetModel for data
// - Creates appropriate editor widgets for each property type
// - Supports expand/collapse of groups
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

private:
  void _subscribeToModel();
  void _rebuildRows();
  void _addRowsRecursive(const std::string& parent_key, int depth, int& y_offset, int& row_index);
  widget_ptr_t _createEditorWidget(const std::string& key, PropertyType type, svar128_t value);
  void _clampScrollOffset();

  property_sheet_model_ptr_t _model;
  std::unordered_set<std::string> _expanded_keys;
  std::unordered_map<std::string, property_row_ptr_t> _rows;
  bool _needs_rebuild = true;
  int _scroll_offset = 0;
  int _total_rows = 0;  // For scroll calculation
};

using property_sheet_ptr_t = std::shared_ptr<PropertySheet>;

} // namespace ork::ui
