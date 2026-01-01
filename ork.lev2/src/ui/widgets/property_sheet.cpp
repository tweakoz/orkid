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

namespace ork::ui {

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

  // Draw background
  _drawColoredBox(drwev, _bg_color);

  mtxi->PushUIMatrix();
  {
    // Draw expand/collapse arrow if has children
    int indent = _depth * _indent_width;
    if (_has_children) {
      auto rs = defmtl->_rasterstate;
      auto omacro = rs->_blendingMacro;
      auto omode = defmtl->meUIColorMode;
      rs->setBlendingMacro(lev2::BlendingMacro::OFF);
      rs->setDepthTest(lev2::EDepthTest::OFF);
      fxi->pushRasterState(rs);
      tgt->PushModColor(fvec4(0.7f, 0.7f, 0.7f, 1.0f));
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);

      int arrow_x = ix1 + indent + 4;
      int arrow_y = iy1 + _geometry._h / 2 - 4;
      primi->RenderQuadAtZ(defmtl.get(), arrow_x, arrow_x + 8, arrow_y, arrow_y + 8,
                           0.0f, 0.0f, 1.0f, 0.0f, 1.0f);

      tgt->PopModColor();
      fxi->popRasterState();
      rs->_blendingMacro = omacro;
      defmtl->meUIColorMode = omode;
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
    return nullptr;
  }

  // Route to editor widget if event is in its area
  if (_editor_widget && _editor_widget->IsEventInside(ev)) {
    auto routed = _editor_widget->doRouteUiEvent(ev);
    if (routed) {
      return routed;
    }
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
      _needs_rebuild = true;
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

widget_ptr_t PropertySheet::_createEditorWidget(const std::string& key, PropertyType type, svar128_t value) {
  widget_ptr_t editor;

  // Get annotations for this property
  varmap::varmap_ptr_t annotations = _model ? _model->getAnnotations(key) : nullptr;

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

    default:
      break;
  }

  return editor;
}

void PropertySheet::_addRowsRecursive(const std::string& parent_key, int depth, int& y_offset) {
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
    row->_bg_color = has_children ? _group_color : _bgcolor;

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
      _addRowsRecursive(key, depth + 1, y_offset);
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
  _addRowsRecursive("", 0, y_offset);
}

void PropertySheet::_doOnResized() {
  DoLayout();
}

void PropertySheet::DoLayout() {
  if (_needs_rebuild) {
    _rebuildRows();
  }

  // Layout children vertically like VerticalPack
  int y = 0;
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

  // Find which child the event is inside
  int y = 0;
  for (auto& child : _children) {
    int child_height = child->height();
    if (localY >= y && localY < y + child_height) {
      if (child->IsEventInside(ev)) {
        auto routed = child->doRouteUiEvent(ev);
        if (routed) {
          return routed;
        }
      }
      break;
    }
    y += child_height;
  }

  return nullptr;
}

HandlerResult PropertySheet::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  switch (ev->_eventcode) {
    case EventCode::MOUSEWHEEL: {
      _scroll_offset -= ev->miMWY * 3;
      int content_height = _children.size() * _row_height;
      int max_scroll = std::max(0, content_height - _geometry._h);
      _scroll_offset = std::clamp(_scroll_offset, 0, max_scroll);
      result.setHandled(this);
      break;
    }

    default:
      break;
  }

  return result;
}

void PropertySheet::DoDraw(drawevent_constptr_t drwev) {
  if (_needs_rebuild) {
    _rebuildRows();
    DoLayout();
  }

  auto tgt = drwev->GetTarget();
  auto fbi = tgt->FBI();

  // Get absolute position for scissor
  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);

  // Push scissor
  fbi->pushScissor(ix1, iy1, _geometry._w, _geometry._h);

  // Draw background
  _drawColoredBox(drwev, _bgcolor);

  // Draw children
  for (auto& child : _children) {
    child->draw(drwev);
  }

  fbi->popScissor();
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
