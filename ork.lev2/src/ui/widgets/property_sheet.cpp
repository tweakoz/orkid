#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gbi.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/ui/property_sheet.h>
#include <ork/lev2/ui/slider.h>
#include <ork/lev2/ui/checkbox.h>
#include <ork/lev2/ui/lineedit.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/context.h>

namespace ork::ui {

static constexpr float PI = 3.14159265359f;

/////////////////////////////////////////////////////////////////////////
// PropertyRow
/////////////////////////////////////////////////////////////////////////

PropertyRow::PropertyRow(const std::string& name, const std::string& key, int depth)
    : Group(name, 0, 0, 0, 0)
    , _key(key)
    , _depth(depth) {
}

void PropertyRow::setLabel(const std::string& label) {
  _label = label;
}

void PropertyRow::setEditorWidget(widget_ptr_t editor) {
  _editor_widget = editor;
  if (editor) {
    addChild(editor);
  }
}

void PropertyRow::setExpanded(bool expanded) {
  _expanded = expanded;
}

void PropertyRow::DoDraw(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto fbi = tgt->FBI();
  auto mtxi = tgt->MTXI();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  // Get absolute position
  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int iy2 = iy1 + _geometry._h;

  // Draw background with alternating colors
  fvec4 bg = (_row_index % 2 == 1) ? _alt_bg_color : _bg_color;
  _drawColoredBox(drwev, bg);

  mtxi->PushUIMatrix();
  {
    int indent = _depth * _indent_width;

    // Draw disclosure triangle if has children (using theme engine)
    if (_has_children && _uicontext && _uicontext->_theme_engine) {
      auto theme = _uicontext->_theme_engine;

      Style tri_style;
      tri_style._bg_color = fvec4(0.7f, 0.7f, 0.7f, 1.0f);
      tri_style._border_color = fvec4(0.7f, 0.7f, 0.7f, 0.0f);
      tri_style._corner_radius = 0;
      tri_style._border_width = 0;
      tri_style._blend_mode = lev2::BlendingMacro::ALPHA;

      const int tri_size = 10;
      int tri_x = ix1 + indent + (_indent_width - tri_size) / 2;
      int tri_y = iy1 + (_geometry._h - tri_size) / 2;

      // Rotation: 0 = point down (expanded), PI/2 = point right (collapsed)
      float rotation = _expanded ? 0.0f : PI / 2.0f;
      theme->drawTriangle(tri_x, tri_y, tri_size, tri_size, drwev, &tri_style, rotation);
    }

    // Draw label
    auto font = lev2::FontMan::fontForId("i14");
    if (font && !_label.empty()) {
      lev2::FontMan::PushFont(font);
      tgt->PushModColor(_label_color);
      lev2::FontMan::beginTextBlock(tgt, _label.length());
      int label_x = ix1 + indent + (_has_children ? _indent_width : 4);
      int text_y = iy1 + (_geometry._h - font->description().miAdvanceHeight) / 2;
      lev2::FontMan::DrawText(tgt, label_x, text_y, _label.c_str());
      lev2::FontMan::endTextBlock(tgt);
      tgt->PopModColor();
      lev2::FontMan::PopFont();
    }
  }
  mtxi->PopUIMatrix();

  // Draw children (editor widget)
  for (auto& child : _children) {
    child->draw(drwev);
  }
}

Widget* PropertyRow::doRouteUiEvent(event_constptr_t ev) {
  if (!IsEventInside(ev)) {
    // Clear drag capture if event is outside row
    if (ev->_eventcode == EventCode::RELEASE || ev->_eventcode == EventCode::END_DRAG) {
      _drag_capture = nullptr;
    }
    return nullptr;
  }

  // If we have a captured widget (from PUSH), route drag events to it
  if (_drag_capture) {
    if (ev->_eventcode == EventCode::DRAG || ev->_eventcode == EventCode::BEGIN_DRAG) {
      return _drag_capture;
    }
    if (ev->_eventcode == EventCode::RELEASE || ev->_eventcode == EventCode::END_DRAG) {
      Widget* target = _drag_capture;
      _drag_capture = nullptr;
      return target;
    }
  }

  // Route to editor widget if event is in its area
  if (_editor_widget && _editor_widget->IsEventInside(ev)) {
    auto routed = _editor_widget->doRouteUiEvent(ev);
    if (routed) {
      // Capture the widget on PUSH for subsequent drag events
      if (ev->_eventcode == EventCode::PUSH) {
        _drag_capture = routed;
      }
      return routed;
    }
  }

  // Clear drag capture on release/push outside editor
  if (ev->_eventcode == EventCode::PUSH || ev->_eventcode == EventCode::RELEASE) {
    _drag_capture = nullptr;
  }

  // If we have children (expandable), handle clicks on label/arrow area
  if (_has_children) {
    return this;
  }

  return nullptr;
}

HandlerResult PropertyRow::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  if (ev->_eventcode == EventCode::PUSH && _has_children) {
    int localX = 0;
    int localY = 0;
    RootToLocal(ev->miX, ev->miY, localX, localY);

    int indent = _depth * _indent_width;
    int arrow_x = indent;

    // Check if click is on arrow area
    if (localX >= arrow_x && localX < arrow_x + _indent_width) {
      _expanded = !_expanded;
      if (_onExpandToggle) {
        _onExpandToggle();
      }
      result.setHandled(this);
    }
  }

  return result;
}

/////////////////////////////////////////////////////////////////////////
// PropertySheet
/////////////////////////////////////////////////////////////////////////

PropertySheet::PropertySheet(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
  _font = lev2::FontMan::fontForId("i14");
  _model = std::make_shared<VarMapPropertyModel>();
  _subscribeToModel();
}

PropertySheet::~PropertySheet() {
}

void PropertySheet::_subscribeToModel() {
  if (_model) {
    _model->_onPropertyChanged = [this](const std::string& key) {
      // Don't rebuild on value changes - the editor widget handles its own display.
      // Rebuilding would destroy the widget being dragged and break event routing.
    };
    _model->_onStructureChanged = [this]() {
      _needs_rebuild = true;
      _expanded_keys.clear();
    };
  }
}

void PropertySheet::setModel(property_sheet_model_ptr_t model) {
  _model = model;
  if (!_model) {
    _model = std::make_shared<VarMapPropertyModel>();
  }
  _subscribeToModel();
  _needs_rebuild = true;
}

void PropertySheet::setData(varmap::varmap_ptr_t data) {
  auto varmap_model = std::make_shared<VarMapPropertyModel>(data);
  setModel(varmap_model);
}

varmap::varmap_ptr_t PropertySheet::getData() const {
  if (auto varmap_model = std::dynamic_pointer_cast<VarMapPropertyModel>(_model)) {
    return varmap_model->getData();
  }
  return nullptr;
}

void PropertySheet::setExpanded(const std::string& key, bool expanded) {
  if (expanded) {
    _expanded_keys.insert(key);
  } else {
    _expanded_keys.erase(key);
  }
  _needs_rebuild = true;
}

bool PropertySheet::isExpanded(const std::string& key) const {
  return _expanded_keys.count(key) > 0;
}

void PropertySheet::expandAll() {
  if (!_model) return;

  std::function<void(const std::string&)> expandRecursive = [&](const std::string& parent_key) {
    auto children = _model->getChildren(parent_key);
    for (const auto& key : children) {
      if (_model->hasChildren(key)) {
        _expanded_keys.insert(key);
        expandRecursive(key);
      }
    }
  };

  expandRecursive("");
  _needs_rebuild = true;
}

void PropertySheet::collapseAll() {
  _expanded_keys.clear();
  _needs_rebuild = true;
}

void PropertySheet::rebuild() {
  _needs_rebuild = true;
}

/////////////////////////////////////////////////////////////////////////
// Detail Editor Overlay
/////////////////////////////////////////////////////////////////////////

void PropertySheet::showDetailEditor(const std::string& key, widget_ptr_t editor) {
  // Close any existing detail editor
  closeDetailEditor();

  if (!editor || !_model) return;

  // Create binding for communication
  _detail_binding = std::make_shared<DetailEditorBinding>();
  _detail_binding->property_key = key;
  _detail_binding->property_type = _model->getPropertyType(key);
  _detail_binding->initial_value = _model->getValue(key);

  // Set up binding callbacks
  _detail_binding->onValueChanged = [this, key](svar128_t value) {
    if (_model) {
      _model->setValue(key, value);
      if (_onPropertyChanged) {
        _onPropertyChanged(key, value);
      }
    }
  };

  _detail_binding->onValueCommit = [this, key](svar128_t value) {
    if (_model) {
      _model->setValue(key, value);
      if (_onPropertyChanged) {
        _onPropertyChanged(key, value);
      }
    }
    closeDetailEditor();
  };

  _detail_binding->onCancel = [this, key]() {
    // Revert to initial value
    if (_model && _detail_binding) {
      _model->setValue(key, _detail_binding->initial_value);
      if (_onPropertyChanged) {
        _onPropertyChanged(key, _detail_binding->initial_value);
      }
    }
    closeDetailEditor();
  };

  _detail_binding->onClose = [this]() {
    closeDetailEditor();
  };

  // Set the detail editor
  _detail_editor = editor;
  _detail_editor->_uicontext = _uicontext;
  _detail_editor->_parent = this;

  // Trigger layout to position the detail editor
  DoLayout();
}

void PropertySheet::closeDetailEditor() {
  if (_detail_editor) {
    // Clear context pointers before destroying
    if (_uicontext) {
      _uicontext->clearWidgetPointers(_detail_editor.get());
    }
    _detail_editor = nullptr;
  }
  _detail_binding = nullptr;

  // Trigger rebuild to update inline editors with new values
  _needs_rebuild = true;
}

/////////////////////////////////////////////////////////////////////////
// Editor Factory Registry
/////////////////////////////////////////////////////////////////////////

void PropertySheet::registerEditorFactory(
    uint32_t type_crc,
    inline_editor_factory_t inline_factory,
    detail_editor_factory_t detail_factory) {
  _editor_factories[type_crc] = EditorFactoryPair{inline_factory, detail_factory};
}

bool PropertySheet::hasEditorFactory(uint32_t type_crc) const {
  return _editor_factories.count(type_crc) > 0;
}

void PropertySheet::requestDetailEditor(const std::string& key) {
  if (!_model) return;

  PropertyType type = _model->getPropertyType(key);
  uint32_t type_crc = propertyTypeToCrc(type);
  svar128_t value = _model->getValue(key);
  varmap::varmap_ptr_t annotations = _model->getAnnotations(key);

  // Check if we have a registered detail factory
  auto it = _editor_factories.find(type_crc);
  if (it != _editor_factories.end() && it->second.detail_factory) {
    // Create binding with callbacks already set up
    auto binding = std::make_shared<DetailEditorBinding>();
    binding->property_key = key;
    binding->property_type = type;
    binding->initial_value = value;

    // Set up callbacks that will close the detail editor
    binding->onValueChanged = [this, key](svar128_t val) {
      if (_model) {
        _model->setValue(key, val);
        if (_onPropertyChanged) {
          _onPropertyChanged(key, val);
        }
      }
    };

    binding->onValueCommit = [this, key](svar128_t val) {
      if (_model) {
        _model->setValue(key, val);
        if (_onPropertyChanged) {
          _onPropertyChanged(key, val);
        }
      }
      closeDetailEditor();
    };

    binding->onCancel = [this, key]() {
      // Revert to initial value
      if (_model && _detail_binding) {
        _model->setValue(key, _detail_binding->initial_value);
        if (_onPropertyChanged) {
          _onPropertyChanged(key, _detail_binding->initial_value);
        }
      }
      closeDetailEditor();
    };

    binding->onClose = [this]() {
      closeDetailEditor();
    };

    // Create detail editor using factory
    widget_ptr_t editor = it->second.detail_factory(nullptr, key, value, annotations, binding);
    if (editor) {
      // Store the binding and show the editor (don't create a new binding)
      closeDetailEditor();
      _detail_binding = binding;
      _detail_editor = editor;
      _detail_editor->_uicontext = _uicontext;
      _detail_editor->_parent = this;
      DoLayout();
      return;
    }
  }

  // Fall back to Python callback if no factory or factory returned null
  if (_onRequestDetailEditor) {
    _onRequestDetailEditor(key, type, value);
  }
}

widget_ptr_t PropertySheet::_createEditorWidget(const std::string& key, PropertyType type, svar128_t value) {
  widget_ptr_t editor;

  // Get annotations for this property
  varmap::varmap_ptr_t annotations = _model ? _model->getAnnotations(key) : nullptr;

  // Check for registered inline factory first
  // Note: We pass nullptr for sheet since Python factories capture their own sheet reference
  // and C++ built-in types use the switch statement below instead of factories
  uint32_t type_crc = propertyTypeToCrc(type);
  auto factory_it = _editor_factories.find(type_crc);
  if (factory_it != _editor_factories.end() && factory_it->second.inline_factory) {
    editor = factory_it->second.inline_factory(nullptr, key, value, annotations);
    if (editor) {
      return editor;
    }
  }

  // Fall back to built-in editors
  switch (type) {
    case PropertyType::Bool: {
      auto checkbox = std::make_shared<Checkbox>("cb_" + key, fvec4(0.2f, 0.2f, 0.2f, 1.0f));
      checkbox->_draw_label = false;  // PropertyRow handles the label
      if (auto b = value.tryAs<bool>()) {
        checkbox->setToggled(b.value());
      }
      checkbox->_onToggled = [this, key, checkbox]() {
        if (_model) {
          _model->setValue(key, svar128_t(checkbox->isToggled()));
          if (_onPropertyChanged) {
            _onPropertyChanged(key, svar128_t(checkbox->isToggled()));
          }
        }
      };
      editor = checkbox;
      break;
    }

    case PropertyType::Int: {
      int min_val = 0, max_val = 100, cur_val = 0;
      if (auto i = value.tryAs<int>()) {
        cur_val = i.value();
      } else if (auto i32 = value.tryAs<int32_t>()) {
        cur_val = i32.value();
      }

      // Check annotations for range
      if (annotations) {
        if (auto min_it = annotations->_themap.find("min"); min_it != annotations->_themap.end()) {
          if (auto m = min_it->second.tryAs<int>()) min_val = m.value();
        }
        if (auto max_it = annotations->_themap.find("max"); max_it != annotations->_themap.end()) {
          if (auto m = max_it->second.tryAs<int>()) max_val = m.value();
        }
      }

      auto slider = std::make_shared<IntSlider>("sl_" + key, fvec4(0.2f, 0.2f, 0.2f, 1.0f), min_val, max_val, cur_val);
      slider->_draw_label = false;  // PropertyRow handles the label
      slider->_update_on_drag = true;
      slider->_onValueChanged = [this, key, slider]() {
        if (_model) {
          _model->setValue(key, svar128_t(slider->value()));
          if (_onPropertyChanged) {
            _onPropertyChanged(key, svar128_t(slider->value()));
          }
        }
      };
      editor = slider;
      break;
    }

    case PropertyType::Float: {
      float min_val = 0.0f, max_val = 1.0f, cur_val = 0.0f;
      if (auto f = value.tryAs<float>()) {
        cur_val = f.value();
      } else if (auto d = value.tryAs<double>()) {
        cur_val = float(d.value());
      }

      // Check annotations for range
      if (annotations) {
        if (auto min_it = annotations->_themap.find("min"); min_it != annotations->_themap.end()) {
          if (auto m = min_it->second.tryAs<float>()) min_val = m.value();
        }
        if (auto max_it = annotations->_themap.find("max"); max_it != annotations->_themap.end()) {
          if (auto m = max_it->second.tryAs<float>()) max_val = m.value();
        }
      }

      auto slider = std::make_shared<FloatSlider>("sl_" + key, fvec4(0.2f, 0.2f, 0.2f, 1.0f), min_val, max_val, cur_val);
      slider->_draw_label = false;  // PropertyRow handles the label
      slider->_update_on_drag = true;
      slider->_onValueChanged = [this, key, slider]() {
        if (_model) {
          _model->setValue(key, svar128_t(slider->value()));
          if (_onPropertyChanged) {
            _onPropertyChanged(key, svar128_t(slider->value()));
          }
        }
      };
      editor = slider;
      break;
    }

    case PropertyType::String: {
      auto lineedit = std::make_shared<LineEdit>("le_" + key, fvec4(0.2f, 0.2f, 0.2f, 1.0f));
      if (auto s = value.tryAs<std::string>()) {
        lineedit->setValue(s.value());
      }
      editor = lineedit;
      break;
    }

    default: {
      // No built-in editor for this type and no registered factory
      auto type_crc = propertyTypeToCrc(type);
      auto color_crc = propertyTypeToCrc(PropertyType::Color);
      auto vec4_crc = propertyTypeToCrc(PropertyType::Vec4);
      printf("PropertySheet: No editor for key<%s> type_crc<0x%08x>\n", key.c_str(), type_crc);
      printf("  For reference: Color<0x%08x> Vec4<0x%08x>\n", color_crc, vec4_crc);
      printf("  Registered factories:\n");
      for (const auto& [crc, factory] : _editor_factories) {
        printf("    crc<0x%08x> has_inline<%d> has_detail<%d>\n",
               crc, factory.inline_factory != nullptr, factory.detail_factory != nullptr);
      }
      OrkAssert(false); // No editor registered for property type
      break;
    }
  }

  return editor;
}

void PropertySheet::_addRowsRecursive(const std::string& parent_key, int depth, int& y_offset, int& row_index) {
  if (!_model) return;

  auto children = _model->getChildren(parent_key);

  for (const auto& key : children) {
    std::string display_name = _model->getDisplayName(key);
    PropertyType type = _model->getPropertyType(key);
    bool has_children = _model->hasChildren(key);
    bool is_expanded = _expanded_keys.count(key) > 0;

    // Create row
    auto row = std::make_shared<PropertyRow>("row_" + key, key, depth);
    row->setLabel(display_name);
    row->setHasChildren(has_children);
    row->setExpanded(is_expanded);
    row->_indent_width = _indent_width;
    row->_label_width = _label_width;
    row->_label_color = _label_color;
    row->_row_index = row_index++;
    row->_bg_color = has_children ? _group_color : _bgcolor;
    row->_alt_bg_color = has_children ? (_group_color * 0.9f) : (_bgcolor * 0.85f);
    row->_alt_bg_color.w = 1.0f;  // Keep full alpha

    // Create editor widget for non-group properties
    if (!has_children && type != PropertyType::Group) {
      svar128_t value = _model->getValue(key);
      auto editor = _createEditorWidget(key, type, value);
      if (editor) {
        row->setEditorWidget(editor);
      }
    }

    // Set up expand/collapse callback
    row->_onExpandToggle = [this, key, is_expanded]() {
      setExpanded(key, !is_expanded);
    };

    addChild(row);
    _rows[key] = row;

    y_offset += _row_height;

    // Recurse if expanded
    if (has_children && is_expanded) {
      _addRowsRecursive(key, depth + 1, y_offset, row_index);
    }
  }
}

void PropertySheet::_rebuildRows() {
  // Set this FIRST to prevent recursion (addChild/removeChild trigger layout)
  _needs_rebuild = false;

  // Remove all existing children
  while (!_children.empty()) {
    removeChild(_children.back());
  }
  _rows.clear();

  // Rebuild from model
  int y_offset = 0;
  int row_index = 0;
  _addRowsRecursive("", 0, y_offset, row_index);
  _total_rows = row_index;
  _clampScrollOffset();
}

void PropertySheet::_clampScrollOffset() {
  // Calculate available height for rows (subtract detail editor if active)
  int rows_area_height = _geometry._h;
  if (_detail_editor) {
    int detail_height = std::max(_detail_min_height, int(_geometry._h * _detail_height_ratio));
    rows_area_height = _geometry._h - detail_height;
  }

  int content_height = _total_rows * _row_height;
  int max_scroll = std::max(0, content_height - rows_area_height);
  _scroll_offset = std::clamp(_scroll_offset, 0, max_scroll);
}

void PropertySheet::_doOnResized() {
  DoLayout();
}

void PropertySheet::DoLayout() {
  // Don't layout if we don't have valid geometry yet
  if (_geometry._w <= 0 || _geometry._h <= 0) {
    return;
  }

  // Always clamp scroll offset on resize, even if rebuild is pending
  _clampScrollOffset();

  // Don't rebuild here - only in DoDraw to avoid destroying widgets during event handling
  // which would invalidate _evpushtarget/_evdragtarget in Context
  if (_needs_rebuild) {
    return;  // Wait for DoDraw to rebuild
  }

  // Calculate detail editor area if active
  int detail_height = 0;
  int rows_area_height = _geometry._h;
  if (_detail_editor) {
    detail_height = std::max(_detail_min_height, int(_geometry._h * _detail_height_ratio));
    rows_area_height = _geometry._h - detail_height;

    // Position detail editor at bottom of property sheet
    _detail_editor->SetRect(0, rows_area_height, _geometry._w, detail_height);
  }

  // Layout children vertically with scroll offset (in rows area)
  int y = -_scroll_offset;
  for (auto& child : _children) {
    child->SetRect(0, y, _geometry._w, _row_height);

    // Layout editor widget within the row
    if (auto row = std::dynamic_pointer_cast<PropertyRow>(child)) {
      if (row->_editor_widget) {
        int editor_x = _label_width + row->_depth * _indent_width;
        int editor_w = _geometry._w - editor_x - 4;
        int editor_h = _row_height - 4;
        row->_editor_widget->SetRect(editor_x, 2, editor_w, editor_h);
      }
    }

    y += _row_height;
  }
}

Widget* PropertySheet::doRouteUiEvent(event_constptr_t ev) {
  if (!IsEventInside(ev)) {
    return nullptr;
  }

  // Convert event coordinates to local space
  int localX = 0;
  int localY = 0;
  RootToLocal(ev->miX, ev->miY, localX, localY);

  // PRIORITY: Route to detail editor first if active and event is inside it
  if (_detail_editor) {
    int detail_height = std::max(_detail_min_height, int(_geometry._h * _detail_height_ratio));
    int detail_y = _geometry._h - detail_height;

    if (localY >= detail_y) {
      // Event is in detail editor area
      if (_detail_editor->IsEventInside(ev)) {
        auto routed = _detail_editor->doRouteUiEvent(ev);
        if (routed) {
          return routed;
        }
        return _detail_editor.get();
      }
    }
  }

  // Calculate rows area height
  int rows_area_height = _geometry._h;
  if (_detail_editor) {
    int detail_height = std::max(_detail_min_height, int(_geometry._h * _detail_height_ratio));
    rows_area_height = _geometry._h - detail_height;
  }

  // Find which child the event is inside (scroll-aware)
  // Children are positioned at y = -_scroll_offset + row_index * _row_height
  int y = -_scroll_offset;
  for (auto& child : _children) {
    int child_height = child->height();
    // Skip children that are scrolled out of view (and outside rows area)
    if (y + child_height > 0 && y < rows_area_height) {
      if (localY >= y && localY < y + child_height && localY < rows_area_height) {
        auto routed = child->doRouteUiEvent(ev);
        if (routed) {
          return routed;
        }
        // Return child itself if it has children (for disclosure triangle clicks)
        if (auto row = std::dynamic_pointer_cast<PropertyRow>(child)) {
          if (row->hasChildren()) {
            return child.get();
          }
        }
        break;
      }
    }
    y += child_height;
  }

  // Return this for scroll wheel handling
  return this;
}

HandlerResult PropertySheet::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  switch (ev->_eventcode) {
    case EventCode::MOUSEWHEEL: {
      _scroll_offset -= ev->miMWY * _row_height;  // Scroll by row height
      _clampScrollOffset();
      DoLayout();  // Re-layout children with new scroll offset
      result.setHandled(this);
      break;
    }

    default:
      break;
  }

  return result;
}

void PropertySheet::DoDraw(drawevent_constptr_t drwev) {
  // Don't draw/rebuild if we don't have valid geometry yet
  if (_geometry._w <= 0 || _geometry._h <= 0) {
    return;
  }

  if (_needs_rebuild) {
    _rebuildRows();
    DoLayout();
  }

  auto tgt = drwev->GetTarget();
  auto fbi = tgt->FBI();

  // Get absolute position for scissor
  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);

  // Calculate rows area if detail editor is active
  int rows_area_height = _geometry._h;
  int detail_height = 0;
  if (_detail_editor) {
    detail_height = std::max(_detail_min_height, int(_geometry._h * _detail_height_ratio));
    rows_area_height = _geometry._h - detail_height;
  }

  // Draw background
  _drawColoredBox(drwev, _bgcolor);

  // Push scissor for rows area only
  fbi->pushScissor(ix1, iy1, _geometry._w, rows_area_height);

  // Draw row children
  for (auto& child : _children) {
    child->draw(drwev);
  }

  fbi->popScissor();

  // Draw detail editor on top (if active)
  if (_detail_editor) {
    // Push scissor for detail area
    fbi->pushScissor(ix1, iy1 + rows_area_height, _geometry._w, detail_height);

    // Draw detail editor background
    auto mtxi = tgt->MTXI();
    auto primi = tgt->PRI();
    auto defmtl = lev2::defaultUIMaterial();

    mtxi->PushUIMatrix();
    {
      int dx1 = ix1;
      int dy1 = iy1 + rows_area_height;
      int dx2 = ix1 + _geometry._w;
      int dy2 = iy1 + _geometry._h;

      tgt->PushModColor(_detail_bg_color);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      primi->RenderQuadAtZ(defmtl.get(), dx1, dx2, dy1, dy2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();
    }
    mtxi->PopUIMatrix();

    // Draw the detail editor widget
    _detail_editor->draw(drwev);

    fbi->popScissor();
  }
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
