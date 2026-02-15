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
#include <ork/lev2/ui/f32edit.h>
#include <ork/lev2/ui/pack.h>
#include <ork/lev2/ui/overlay_lineedit.h>
#include <ork/lev2/ui/dropdown_menu.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/colorswatch.h>
#include <ork/lev2/ui/coloredit.h>
#include <ork/math/quaternion.h>

namespace ork::ui {

static constexpr float PI = 3.14159265359f;
static constexpr float kRadToDeg = 180.0f / PI;
static constexpr float kDegToRad = PI / 180.0f;

/////////////////////////////////////////////////////////////////////////
// MapItemObjectFactoryWidget
// Shows a clickable button for null object map entries.
// On click, shows a DropdownMenu with available factory classes.
// On selection, creates the object and triggers a rebuild.
/////////////////////////////////////////////////////////////////////////

struct MapItemObjectFactoryWidget : public Widget {
  MapItemObjectFactoryWidget(const std::string& name, const std::vector<std::string>& factory_classes)
      : Widget(name, 0, 0, 0, 0)
      , _factory_classes(factory_classes) {
  }

  std::vector<std::string> _factory_classes;
  std::function<void(const std::string&)> _onFactorySelected;

  fvec4 _bg_color = fvec4(0.25f, 0.2f, 0.3f, 1.0f);
  fvec4 _fg_color = fvec4(0.8f, 0.8f, 0.5f, 1.0f);

  void DoDraw(drawevent_constptr_t drwev) override {
    auto tgt = drwev->GetTarget();
    auto mtxi = tgt->MTXI();
    auto primi = tgt->PRI();
    auto defmtl = lev2::defaultUIMaterial();

    int ix1, iy1;
    LocalToRoot(0, 0, ix1, iy1);
    int ix2 = ix1 + _geometry._w;
    int iy2 = iy1 + _geometry._h;

    mtxi->PushUIMatrix();
    {
      // Draw button background
      defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
      defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
      tgt->PushModColor(_bg_color);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      primi->RenderQuadAtZ(defmtl.get(), ix1 + 1, ix2 - 1, iy1 + 1, iy2 - 1, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();

      // Draw label
      std::string label = _factory_classes.size() == 1
          ? FormatString("Create: %s", _factory_classes[0].c_str())
          : "Select Type...";

      auto font = lev2::FontMan::fontForId("i14");
      if (font) {
        lev2::FontMan::PushFont(font);
        tgt->PushModColor(_fg_color);
        int text_x = ix1 + 6;
        int text_y = iy1 + (_geometry._h - font->description().miAdvanceHeight) / 2;
        lev2::FontMan::beginTextBlock(tgt, label.length());
        lev2::FontMan::DrawText(tgt, text_x, text_y, label.c_str());
        lev2::FontMan::endTextBlock(tgt);
        tgt->PopModColor();
        lev2::FontMan::PopFont();
      }
    }
    mtxi->PopUIMatrix();
  }

  HandlerResult DoOnUiEvent(event_constptr_t ev) override {
    HandlerResult result;

    if (ev->_eventcode == EventCode::PUSH) {
      if (_factory_classes.size() == 1) {
        // Only one factory: instantiate directly
        if (_onFactorySelected) {
          _onFactorySelected(_factory_classes[0]);
        }
        result.setHandled(this);
      } else if (_factory_classes.size() > 1) {
        // Multiple factories: show dropdown
        auto tree = DropdownMenu::buildTreeFromPaths(_factory_classes);
        auto menu = std::make_shared<DropdownMenu>("factory_" + _name, tree->root());
        menu->_onSelected = [this](std::string selected) {
          // Strip leading / from DropdownMenu's SlashTree path
          if (!selected.empty() && selected[0] == '/') {
            selected = selected.substr(1);
          }
          if (_onFactorySelected) {
            _onFactorySelected(selected);
          }
        };
        auto sz = menu->computeSize();
        int sx = ev->miX;
        int sy = ev->miY;
        if (_uicontext) {
          _uicontext->pushOverlay(menu, sx, sy, int(sz.x), int(sz.y), true, nullptr);
        }
        result.setHandled(this);
      }
    }

    return result;
  }
};

/////////////////////////////////////////////////////////////////////////
// ChoicelistWidget
// Shows a dropdown button for properties that have a choice list.
// Displays current value text with a v indicator; opens DropdownMenu on click.
/////////////////////////////////////////////////////////////////////////

struct ChoicelistWidget : public Widget {
  ChoicelistWidget(const std::string& name, const std::string& current_value)
      : Widget(name, 0, 0, 0, 0)
      , _current_value(current_value) {
  }

  std::string _current_value;
  std::function<std::vector<std::string>()> _getChoices;
  std::function<void(const std::string&)> _onChoiceSelected;

  fvec4 _bg_color = fvec4(0.2f, 0.2f, 0.25f, 1.0f);
  fvec4 _fg_color = fvec4(0.8f, 0.8f, 0.8f, 1.0f);
  fvec4 _indicator_color = fvec4(0.5f, 0.6f, 0.8f, 1.0f);

  void DoDraw(drawevent_constptr_t drwev) override {
    auto tgt = drwev->GetTarget();
    auto mtxi = tgt->MTXI();
    auto primi = tgt->PRI();
    auto defmtl = lev2::defaultUIMaterial();

    int ix1, iy1;
    LocalToRoot(0, 0, ix1, iy1);
    int ix2 = ix1 + _geometry._w;
    int iy2 = iy1 + _geometry._h;

    mtxi->PushUIMatrix();
    {
      // Draw background
      defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
      defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
      tgt->PushModColor(_bg_color);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      primi->RenderQuadAtZ(defmtl.get(), ix1 + 1, ix2 - 1, iy1 + 1, iy2 - 1, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();

      // Draw current value text
      auto font = lev2::FontMan::fontForId("i14");
      if (font) {
        lev2::FontMan::PushFont(font);
        tgt->PushModColor(_fg_color);
        int text_x = ix1 + 6;
        int text_y = iy1 + (_geometry._h - font->description().miAdvanceHeight) / 2;
        lev2::FontMan::beginTextBlock(tgt, _current_value.length() + 2);
        lev2::FontMan::DrawText(tgt, text_x, text_y, _current_value.c_str());
        lev2::FontMan::endTextBlock(tgt);
        tgt->PopModColor();

        // Draw v indicator on the right
        tgt->PushModColor(_indicator_color);
        std::string indicator = "v";
        int ind_x = ix2 - 18;
        lev2::FontMan::beginTextBlock(tgt, 1);
        lev2::FontMan::DrawText(tgt, ind_x, text_y, indicator.c_str());
        lev2::FontMan::endTextBlock(tgt);
        tgt->PopModColor();

        lev2::FontMan::PopFont();
      }
    }
    mtxi->PopUIMatrix();
  }

  HandlerResult DoOnUiEvent(event_constptr_t ev) override {
    HandlerResult result;

    if (ev->_eventcode == EventCode::PUSH) {
      if (_getChoices) {
        auto choices = _getChoices();
        if (!choices.empty()) {
          // Prepend / so DropdownMenu slash-tree works correctly
          std::vector<std::string> paths;
          for (const auto& c : choices) {
            if (c.empty()) continue;
            // If the choice already starts with /, use as-is; otherwise prepend /
            if (c[0] == '/') {
              paths.push_back(c);
            } else {
              paths.push_back("/" + c);
            }
          }
          auto tree = DropdownMenu::buildTreeFromPaths(paths);
          auto menu = std::make_shared<DropdownMenu>("choicelist_" + _name, tree->root());
          menu->_onSelected = [this](std::string selected) {
            // Strip leading / that was prepended for the slash tree
            if (!selected.empty() && selected[0] == '/') {
              selected = selected.substr(1);
            }
            if (_onChoiceSelected) {
              _onChoiceSelected(selected);
            }
          };
          auto sz = menu->computeSize();
          int sx = ev->miX;
          int sy = ev->miY;
          if (_uicontext) {
            _uicontext->pushOverlay(menu, sx, sy, int(sz.x), int(sz.y), true, nullptr);
          }
        }
      }
      result.setHandled(this);
    }

    return result;
  }
};

/////////////////////////////////////////////////////////////////////////
// PropertyRow
/////////////////////////////////////////////////////////////////////////

PropertyRow::PropertyRow(const std::string& name, const std::string& key, int depth)
    : Group(name, 0, 0, 0, 0)
    , _key(key)
    , _depth(depth) {
  // Propagate _uicontext to editor widgets when this row is
  // added to the PropertySheet (so overlays like DropdownMenu work).
  _propagate_on_parent_change = true;
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

    // Draw [+][-] buttons for mutable map properties (right-aligned)
    if (_is_map_property && !_is_map_const) {
      const int btn_size = 12;
      const int btn_spacing = 4;
      const int btn_margin = 8;
      int btn_y = iy1 + (_geometry._h - btn_size) / 2;
      int btn2_right = ix1 + _geometry._w - btn_margin;
      int btn2_x_draw = btn2_right - btn_size;
      int btn_x = btn2_x_draw - btn_spacing - btn_size;

      defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
      defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);

      // [+] button - outline box with cross
      fvec4 btn_color(0.5f, 0.8f, 0.5f, 1.0f);
      tgt->PushModColor(btn_color);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      // Outline: top
      primi->RenderQuadAtZ(defmtl.get(), btn_x, btn_x + btn_size, btn_y, btn_y + 1, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      // Outline: bottom
      primi->RenderQuadAtZ(defmtl.get(), btn_x, btn_x + btn_size, btn_y + btn_size - 1, btn_y + btn_size, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      // Outline: left
      primi->RenderQuadAtZ(defmtl.get(), btn_x, btn_x + 1, btn_y, btn_y + btn_size, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      // Outline: right
      primi->RenderQuadAtZ(defmtl.get(), btn_x + btn_size - 1, btn_x + btn_size, btn_y, btn_y + btn_size, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      // Horizontal bar of +
      int cx = btn_x + btn_size / 2;
      int cy = btn_y + btn_size / 2;
      primi->RenderQuadAtZ(defmtl.get(), btn_x + 2, btn_x + btn_size - 2, cy, cy + 1, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      // Vertical bar of +
      primi->RenderQuadAtZ(defmtl.get(), cx, cx + 1, btn_y + 2, btn_y + btn_size - 2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();

      // [-] button - outline box with horizontal line
      int btn2_x = btn2_x_draw;
      fvec4 btn2_color(0.8f, 0.5f, 0.5f, 1.0f);
      tgt->PushModColor(btn2_color);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      // Outline: top
      primi->RenderQuadAtZ(defmtl.get(), btn2_x, btn2_x + btn_size, btn_y, btn_y + 1, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      // Outline: bottom
      primi->RenderQuadAtZ(defmtl.get(), btn2_x, btn2_x + btn_size, btn_y + btn_size - 1, btn_y + btn_size, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      // Outline: left
      primi->RenderQuadAtZ(defmtl.get(), btn2_x, btn2_x + 1, btn_y, btn_y + btn_size, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      // Outline: right
      primi->RenderQuadAtZ(defmtl.get(), btn2_x + btn_size - 1, btn2_x + btn_size, btn_y, btn_y + btn_size, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      // Horizontal bar of -
      int cy2 = btn_y + btn_size / 2;
      primi->RenderQuadAtZ(defmtl.get(), btn2_x + 2, btn2_x + btn_size - 2, cy2, cy2 + 1, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();
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

  // If we have children (expandable) or are a map property, handle clicks on label/arrow area
  if (_has_children || _is_map_property) {
    return this;
  }

  return nullptr;
}

HandlerResult PropertyRow::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  if (ev->_eventcode == EventCode::PUSH && (_has_children || _is_map_property)) {
    int localX = 0;
    int localY = 0;
    RootToLocal(ev->miX, ev->miY, localX, localY);

    int indent = _depth * _indent_width;

    // Check if click is on map buttons [+] or [-] (right-aligned)
    if (_is_map_property && !_is_map_const) {
      const int btn_size = 12;
      const int btn_spacing = 4;
      const int btn_margin = 8;
      int btn_y_top = (_geometry._h - btn_size) / 2;
      int btn_y_bot = btn_y_top + btn_size;
      int btn2_x = _geometry._w - btn_margin - btn_size;
      int btn1_x = btn2_x - btn_spacing - btn_size;

      if (localY >= btn_y_top && localY <= btn_y_bot) {
        // [+] button
        if (localX >= btn1_x && localX < btn1_x + btn_size) {
          if (_onMapAdd) {
            _onMapAdd(ev);
          }
          result.setHandled(this);
          return result;
        }
        // [-] button
        if (localX >= btn2_x && localX < btn2_x + btn_size) {
          if (_onMapRemove) {
            _onMapRemove(ev);
          }
          result.setHandled(this);
          return result;
        }
      }
    }

    // Check if click is on arrow area
    int arrow_x = indent;
    if (localX >= arrow_x && localX < arrow_x + _indent_width) {
      _expanded = !_expanded;
      if (_onExpandToggle) {
        _onExpandToggle();
      }
      result.setHandled(this);
    }
  }

  // Double-click on map row header for item selection
  if (ev->_eventcode == EventCode::DOUBLECLICK && _is_map_property && _onMapSelectItem) {
    _onMapSelectItem(ev);
    result.setHandled(this);
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
  // Disconnect previous signal connection
  _external_value_connection = sigslot2::scoped_connection();

  if (_model) {
    _model->_onPropertyChanged = [this](const std::string& key) {
      // Don't rebuild on value changes - the editor widget handles its own display.
      // Rebuilding would destroy the widget being dragged and break event routing.
    };
    _model->_onStructureChanged = [this]() {
      _needs_rebuild = true;
      _expanded_keys.clear();
    };

    // Connect to external value change signal for live sync (e.g. manipulators)
    _external_value_connection = _model->_sigExternalValueChanged.connect([this](std::string key) {
      refreshValue(key);
    });
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

void PropertySheet::refreshValue(const std::string& key) {
  if (!_model) return;

  if (key.empty()) {
    // Refresh all rows
    for (auto& [rkey, row] : _rows) {
      if (row->_refreshEditor) {
        svar128_t val = _model->getValue(rkey);
        row->_refreshEditor(val);
      }
    }
  } else {
    // Refresh the specific row
    auto it = _rows.find(key);
    if (it != _rows.end() && it->second->_refreshEditor) {
      svar128_t val = _model->getValue(key);
      it->second->_refreshEditor(val);
    }
    // Also refresh any children (compound sub-properties)
    auto children = _model->getChildren(key);
    for (const auto& child_key : children) {
      auto cit = _rows.find(child_key);
      if (cit != _rows.end() && cit->second->_refreshEditor) {
        svar128_t val = _model->getValue(child_key);
        cit->second->_refreshEditor(val);
      }
    }
  }
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

  // Check for choice list — if present, show a dropdown regardless of type
  if (_model) {
    auto choices = _model->getChoices(key);
    if (!choices.empty()) {
      std::string cur_text;
      if (auto s = value.tryAs<std::string>()) {
        cur_text = s.value();
      } else {
        cur_text = "(none)";
      }
      auto cw = std::make_shared<ChoicelistWidget>("cl_" + key, cur_text);
      cw->_getChoices = [this, key]() -> std::vector<std::string> {
        return _model ? _model->getChoices(key) : std::vector<std::string>{};
      };
      std::weak_ptr<ChoicelistWidget> cw_weak = cw;
      cw->_onChoiceSelected = [this, key, cw_weak](const std::string& selected) {
        if (_model) {
          svar128_t val;
          val.set<std::string>(selected);
          _model->setValue(key, val);
          if (auto cw_locked = cw_weak.lock()) {
            cw_locked->_current_value = selected;
          }
          if (_onPropertyChanged) {
            _onPropertyChanged(key, val);
          }
        }
      };
      return cw;
    }
  }

  // Get annotations for this property
  varmap::varmap_ptr_t annotations = _model ? _model->getAnnotations(key) : nullptr;

  // Check for registered inline factory first
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
      checkbox->_draw_label = false;
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
      if (annotations) {
        if (auto min_it = annotations->_themap.find("min"); min_it != annotations->_themap.end()) {
          if (auto m = min_it->second.tryAs<int>()) min_val = m.value();
        }
        if (auto max_it = annotations->_themap.find("max"); max_it != annotations->_themap.end()) {
          if (auto m = max_it->second.tryAs<int>()) max_val = m.value();
        }
      }
      auto slider = std::make_shared<IntSlider>("sl_" + key, fvec4(0.2f, 0.2f, 0.2f, 1.0f), min_val, max_val, cur_val);
      slider->_draw_label = false;
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
      float cur_val = 0.0f;
      float min_val = -1e30f, max_val = 1e30f;
      if (auto f = value.tryAs<float>()) {
        cur_val = f.value();
      }
      if (annotations) {
        if (auto min_it = annotations->_themap.find("min"); min_it != annotations->_themap.end()) {
          if (auto m = min_it->second.tryAs<float>()) min_val = m.value();
        }
        if (auto max_it = annotations->_themap.find("max"); max_it != annotations->_themap.end()) {
          if (auto m = max_it->second.tryAs<float>()) max_val = m.value();
        }
      }
      auto f32 = std::make_shared<F32Edit>("f32_" + key, "", cur_val, min_val, max_val);
      f32->_drag_rate = 0.01f;
      f32->_onValueChanged = [this, key, f32](float v) {
        if (_model) {
          svar128_t val;
          val.set<float>(v);
          _model->setValue(key, val);
          if (_onPropertyChanged) {
            _onPropertyChanged(key, val);
          }
        }
      };
      editor = f32;
      break;
    }

    case PropertyType::Vec3: {
      // HorizontalPack with 3 F32Edit fields (X, Y, Z) - matches sgedit TransformEdit
      fvec3 v(0, 0, 0);
      if (auto vec = value.tryAs<fvec3>()) {
        v = vec.value();
      }
      auto hpack = std::make_shared<HorizontalPack>("hp_" + key);
      hpack->_draw_background = false;
      hpack->_uniform = true;
      hpack->_margin = 2;

      auto fx = std::make_shared<F32Edit>("fx_" + key, "X", v.x);
      auto fy = std::make_shared<F32Edit>("fy_" + key, "Y", v.y);
      auto fz = std::make_shared<F32Edit>("fz_" + key, "Z", v.z);
      fx->_drag_rate = 0.01f;
      fy->_drag_rate = 0.01f;
      fz->_drag_rate = 0.01f;

      // Wire callbacks: read-modify-write the Vec3
      auto writeVec3 = [this, key, fx, fy, fz](float) {
        if (_model) {
          fvec3 nv(fx->getValue(), fy->getValue(), fz->getValue());
          svar128_t val;
          val.set<fvec3>(nv);
          _model->setValue(key, val);
          if (_onPropertyChanged) {
            _onPropertyChanged(key, val);
          }
        }
      };
      fx->_onValueChanged = writeVec3;
      fy->_onValueChanged = writeVec3;
      fz->_onValueChanged = writeVec3;

      hpack->addChild(fx);
      hpack->addChild(fy);
      hpack->addChild(fz);
      editor = hpack;
      break;
    }

    case PropertyType::Quat: {
      // HorizontalPack: [Ang] [Axis X] [Y] [Z] - matches sgedit TransformEdit
      fquat q;
      if (auto qv = value.tryAs<fquat>()) {
        q = qv.value();
      }
      fvec4 aa = q.toAxisAngle();
      float angle_deg = aa.w * kRadToDeg;

      auto hpack = std::make_shared<HorizontalPack>("hp_" + key);
      hpack->_draw_background = false;
      hpack->_margin = 2;
      hpack->_item_width = 80;
      hpack->_fill = true;

      // Angle field (fixed width first)
      auto fang = std::make_shared<F32Edit>("fang_" + key, "deg", angle_deg, -360.0f, 360.0f);
      fang->_drag_rate = 1.0f;
      fang->_precision = 1;

      // Axis fields (fill remaining space)
      auto haxis = std::make_shared<HorizontalPack>("haxis_" + key);
      haxis->_draw_background = false;
      haxis->_uniform = true;
      haxis->_margin = 2;

      auto fax = std::make_shared<F32Edit>("fax_" + key, "X", aa.x, -1.0f, 1.0f);
      auto fay = std::make_shared<F32Edit>("fay_" + key, "Y", aa.y, -1.0f, 1.0f);
      auto faz = std::make_shared<F32Edit>("faz_" + key, "Z", aa.z, -1.0f, 1.0f);
      fax->_drag_rate = 0.01f;
      fay->_drag_rate = 0.01f;
      faz->_drag_rate = 0.01f;

      haxis->addChild(fax);
      haxis->addChild(fay);
      haxis->addChild(faz);

      // Write callback: recompose quaternion from axis-angle
      auto writeQuat = [this, key, fang, fax, fay, faz](float) {
        if (_model) {
          fvec4 naa(fax->getValue(), fay->getValue(), faz->getValue(), fang->getValue() * kDegToRad);
          fquat nq;
          nq.fromAxisAngle(naa);
          svar128_t val;
          val.set<fquat>(nq);
          _model->setValue(key, val);
          if (_onPropertyChanged) {
            _onPropertyChanged(key, val);
          }
        }
      };
      fang->_onValueChanged = writeQuat;
      fax->_onValueChanged = writeQuat;
      fay->_onValueChanged = writeQuat;
      faz->_onValueChanged = writeQuat;

      // Normalize axis on commit (like sgedit)
      auto normalizeAxis = [fax, fay, faz, writeQuat](float) {
        float ax = fax->getValue(), ay = fay->getValue(), az = faz->getValue();
        float len = std::sqrt(ax * ax + ay * ay + az * az);
        if (len > 0.0001f) {
          fax->setValue(ax / len);
          fay->setValue(ay / len);
          faz->setValue(az / len);
          writeQuat(0);
        }
      };
      fax->_onValueCommitted = normalizeAxis;
      fay->_onValueCommitted = normalizeAxis;
      faz->_onValueCommitted = normalizeAxis;

      hpack->addChild(fang);
      hpack->addChild(haxis);
      hpack->_fill_widget = haxis;
      editor = hpack;
      break;
    }

    case PropertyType::Vec4: {
      fvec4 v(0, 0, 0, 1);
      if (auto vec = value.tryAs<fvec4>()) {
        v = vec.value();
      }

      // Check for color semantic
      bool is_color = false;
      if (annotations) {
        auto sem_it = annotations->_themap.find("editor.semantic");
        if (sem_it != annotations->_themap.end()) {
          if (auto s = sem_it->second.tryAs<std::string>()) {
            is_color = (s.value() == "color");
          }
        }
      }

      if (is_color) {
        auto hpack = std::make_shared<HorizontalPack>("hp_" + key);
        hpack->_draw_background = false;
        hpack->_margin = 2;
        hpack->_item_width = 24;
        hpack->_fill = true;

        auto swatch = std::make_shared<ColorSwatch>("sw_" + key, v);

        auto hfields = std::make_shared<HorizontalPack>("hf_" + key);
        hfields->_draw_background = false;
        hfields->_uniform = true;
        hfields->_margin = 2;

        auto fr = std::make_shared<F32Edit>("fr_" + key, "R", v.x, 0.0f, 1.0f);
        auto fg = std::make_shared<F32Edit>("fg_" + key, "G", v.y, 0.0f, 1.0f);
        auto fb = std::make_shared<F32Edit>("fb_" + key, "B", v.z, 0.0f, 1.0f);
        auto fa = std::make_shared<F32Edit>("fa_" + key, "A", v.w, 0.0f, 1.0f);
        fr->_drag_rate = 0.005f;
        fg->_drag_rate = 0.005f;
        fb->_drag_rate = 0.005f;
        fa->_drag_rate = 0.005f;
        fr->_precision = 3;
        fg->_precision = 3;
        fb->_precision = 3;
        fa->_precision = 3;

        auto writeColor = [this, key, fr, fg, fb, fa, swatch](float) {
          if (_model) {
            fvec4 nv(fr->getValue(), fg->getValue(), fb->getValue(), fa->getValue());
            swatch->setColor(nv);
            svar128_t val;
            val.set<fvec4>(nv);
            _model->setValue(key, val);
            if (_onPropertyChanged) {
              _onPropertyChanged(key, val);
            }
          }
        };
        fr->_onValueChanged = writeColor;
        fg->_onValueChanged = writeColor;
        fb->_onValueChanged = writeColor;
        fa->_onValueChanged = writeColor;

        // Click swatch → open ColorEdit as detail editor
        std::weak_ptr<ColorSwatch> swatch_weak = swatch;
        std::weak_ptr<F32Edit> fr_weak = fr;
        std::weak_ptr<F32Edit> fg_weak = fg;
        std::weak_ptr<F32Edit> fb_weak = fb;
        std::weak_ptr<F32Edit> fa_weak = fa;
        swatch->_onClick = [this, key, swatch_weak, fr_weak, fg_weak, fb_weak, fa_weak]() {
          auto sw = swatch_weak.lock();
          if (!sw) return;
          auto coloredit = std::make_shared<ColorEdit>("ce_" + key, sw->color());
          coloredit->_originalColor = sw->color();
          coloredit->_onColorChanged = [this, key, swatch_weak, fr_weak, fg_weak, fb_weak, fa_weak](fvec4 newcolor) {
            if (auto sw2 = swatch_weak.lock()) sw2->setColor(newcolor);
            if (auto r = fr_weak.lock()) r->setValue(newcolor.x);
            if (auto g = fg_weak.lock()) g->setValue(newcolor.y);
            if (auto b = fb_weak.lock()) b->setValue(newcolor.z);
            if (auto a = fa_weak.lock()) a->setValue(newcolor.w);
            if (_model) {
              svar128_t val;
              val.set<fvec4>(newcolor);
              _model->setValue(key, val);
              if (_onPropertyChanged) {
                _onPropertyChanged(key, val);
              }
            }
          };
          coloredit->_onFinished = [this](bool accepted) {
            closeDetailEditor();
          };
          showDetailEditor(key, coloredit);
        };

        hfields->addChild(fr);
        hfields->addChild(fg);
        hfields->addChild(fb);
        hfields->addChild(fa);

        hpack->addChild(swatch);
        hpack->addChild(hfields);
        hpack->_fill_widget = hfields;
        editor = hpack;
      } else {
        // Regular Vec4: XYZW fields
        auto hpack = std::make_shared<HorizontalPack>("hp_" + key);
        hpack->_draw_background = false;
        hpack->_uniform = true;
        hpack->_margin = 2;

        auto fx = std::make_shared<F32Edit>("fx_" + key, "X", v.x);
        auto fy = std::make_shared<F32Edit>("fy_" + key, "Y", v.y);
        auto fz = std::make_shared<F32Edit>("fz_" + key, "Z", v.z);
        auto fw = std::make_shared<F32Edit>("fw_" + key, "W", v.w);
        fx->_drag_rate = 0.01f;
        fy->_drag_rate = 0.01f;
        fz->_drag_rate = 0.01f;
        fw->_drag_rate = 0.01f;

        auto writeVec4 = [this, key, fx, fy, fz, fw](float) {
          if (_model) {
            fvec4 nv(fx->getValue(), fy->getValue(), fz->getValue(), fw->getValue());
            svar128_t val;
            val.set<fvec4>(nv);
            _model->setValue(key, val);
            if (_onPropertyChanged) {
              _onPropertyChanged(key, val);
            }
          }
        };
        fx->_onValueChanged = writeVec4;
        fy->_onValueChanged = writeVec4;
        fz->_onValueChanged = writeVec4;
        fw->_onValueChanged = writeVec4;

        hpack->addChild(fx);
        hpack->addChild(fy);
        hpack->addChild(fz);
        hpack->addChild(fw);
        editor = hpack;
      }
      break;
    }

    case PropertyType::String: {
      auto lineedit = std::make_shared<LineEdit>("le_" + key, fvec4(0.2f, 0.2f, 0.2f, 1.0f));
      lineedit->_draw_label = false;
      if (auto s = value.tryAs<std::string>()) {
        lineedit->setValue(s.value());
      }
      lineedit->_onTextCommitted = [this, key](const std::string& text) {
        if (_model) {
          svar128_t val;
          val.set<std::string>(text);
          _model->setValue(key, val);
          if (_onPropertyChanged) {
            _onPropertyChanged(key, val);
          }
        }
      };
      editor = lineedit;
      break;
    }

    case PropertyType::Asset: {
      auto lineedit = std::make_shared<LineEdit>("le_" + key, fvec4(0.2f, 0.2f, 0.25f, 1.0f));
      lineedit->_draw_label = false;
      if (auto s = value.tryAs<std::string>()) {
        lineedit->setValue(s.value());
      }
      lineedit->_onTextCommitted = [this, key](const std::string& text) {
        if (_model) {
          svar128_t val;
          val.set<std::string>(text);
          _model->setValue(key, val);
          if (_onPropertyChanged) {
            _onPropertyChanged(key, val);
          }
        }
      };
      editor = lineedit;
      break;
    }

    default: {
      auto lineedit = std::make_shared<LineEdit>("le_" + key, fvec4(0.2f, 0.2f, 0.2f, 1.0f));
      lineedit->_draw_label = false;
      lineedit->setValue("(no editor)");
      editor = lineedit;
      break;
    }
  }

  return editor;
}

std::function<void(svar128_t)> PropertySheet::_makeRefreshCallback(widget_ptr_t editor, PropertyType type) {
  // ChoicelistWidget: update displayed text on external value change
  auto cw = std::dynamic_pointer_cast<ChoicelistWidget>(editor);
  if (cw) {
    return [cw](svar128_t new_value) {
      if (auto s = new_value.tryAs<std::string>()) {
        cw->_current_value = s.value();
      }
    };
  }

  if (type == PropertyType::Float) {
    auto f32 = std::dynamic_pointer_cast<F32Edit>(editor);
    if (f32) {
      return [f32](svar128_t new_value) {
        if (f32->_editing || f32->_dragging) return;  // Skip if user is interacting
        if (auto f = new_value.tryAs<float>()) {
          f32->setValue(f.value());
        }
      };
    }
  } else if (type == PropertyType::Vec3) {
    // HorizontalPack with 3 F32Edit children
    auto hpack = std::dynamic_pointer_cast<HorizontalPack>(editor);
    if (hpack && hpack->_children.size() >= 3) {
      auto fx = std::dynamic_pointer_cast<F32Edit>(hpack->_children[0]);
      auto fy = std::dynamic_pointer_cast<F32Edit>(hpack->_children[1]);
      auto fz = std::dynamic_pointer_cast<F32Edit>(hpack->_children[2]);
      if (fx && fy && fz) {
        return [fx, fy, fz](svar128_t new_value) {
          if (auto v = new_value.tryAs<fvec3>()) {
            if (!fx->_editing && !fx->_dragging) fx->setValue(v.value().x);
            if (!fy->_editing && !fy->_dragging) fy->setValue(v.value().y);
            if (!fz->_editing && !fz->_dragging) fz->setValue(v.value().z);
          }
        };
      }
    }
  } else if (type == PropertyType::Quat) {
    // HorizontalPack: [fang, haxis_pack(fax, fay, faz)]
    auto hpack = std::dynamic_pointer_cast<HorizontalPack>(editor);
    if (hpack && hpack->_children.size() >= 2) {
      auto fang = std::dynamic_pointer_cast<F32Edit>(hpack->_children[0]);
      auto haxis = std::dynamic_pointer_cast<HorizontalPack>(hpack->_children[1]);
      if (fang && haxis && haxis->_children.size() >= 3) {
        auto fax = std::dynamic_pointer_cast<F32Edit>(haxis->_children[0]);
        auto fay = std::dynamic_pointer_cast<F32Edit>(haxis->_children[1]);
        auto faz = std::dynamic_pointer_cast<F32Edit>(haxis->_children[2]);
        if (fax && fay && faz) {
          return [fang, fax, fay, faz](svar128_t new_value) {
            // Skip all orientation fields if any is being edited (like sgedit)
            bool any_busy = fang->_editing || fang->_dragging
                         || fax->_editing || fax->_dragging
                         || fay->_editing || fay->_dragging
                         || faz->_editing || faz->_dragging;
            if (any_busy) return;
            if (auto q = new_value.tryAs<fquat>()) {
              fvec4 aa = q.value().toAxisAngle();
              fax->setValue(aa.x);
              fay->setValue(aa.y);
              faz->setValue(aa.z);
              fang->setValue(aa.w * (180.0f / 3.14159265359f));
            }
          };
        }
      }
    }
  } else if (type == PropertyType::Vec4) {
    // Color: HorizontalPack [swatch, hfields_pack(fr, fg, fb, fa)]
    // Regular: HorizontalPack [fx, fy, fz, fw]
    auto hpack = std::dynamic_pointer_cast<HorizontalPack>(editor);
    if (hpack && hpack->_children.size() >= 2) {
      // Check if first child is a ColorSwatch (color mode)
      auto swatch = std::dynamic_pointer_cast<ColorSwatch>(hpack->_children[0]);
      auto hfields = std::dynamic_pointer_cast<HorizontalPack>(hpack->_children[1]);
      if (swatch && hfields && hfields->_children.size() >= 4) {
        auto fr = std::dynamic_pointer_cast<F32Edit>(hfields->_children[0]);
        auto fg = std::dynamic_pointer_cast<F32Edit>(hfields->_children[1]);
        auto fb = std::dynamic_pointer_cast<F32Edit>(hfields->_children[2]);
        auto fa = std::dynamic_pointer_cast<F32Edit>(hfields->_children[3]);
        if (fr && fg && fb && fa) {
          return [swatch, fr, fg, fb, fa](svar128_t new_value) {
            bool any_busy = fr->_editing || fr->_dragging
                         || fg->_editing || fg->_dragging
                         || fb->_editing || fb->_dragging
                         || fa->_editing || fa->_dragging;
            if (any_busy) return;
            if (auto v = new_value.tryAs<fvec4>()) {
              fr->setValue(v.value().x);
              fg->setValue(v.value().y);
              fb->setValue(v.value().z);
              fa->setValue(v.value().w);
              swatch->setColor(v.value());
            }
          };
        }
      }
      // Regular Vec4: 4 F32Edit children
      if (hpack->_children.size() >= 4) {
        auto fx = std::dynamic_pointer_cast<F32Edit>(hpack->_children[0]);
        auto fy = std::dynamic_pointer_cast<F32Edit>(hpack->_children[1]);
        auto fz = std::dynamic_pointer_cast<F32Edit>(hpack->_children[2]);
        auto fw = std::dynamic_pointer_cast<F32Edit>(hpack->_children[3]);
        if (fx && fy && fz && fw) {
          return [fx, fy, fz, fw](svar128_t new_value) {
            if (auto v = new_value.tryAs<fvec4>()) {
              if (!fx->_editing && !fx->_dragging) fx->setValue(v.value().x);
              if (!fy->_editing && !fy->_dragging) fy->setValue(v.value().y);
              if (!fz->_editing && !fz->_dragging) fz->setValue(v.value().z);
              if (!fw->_editing && !fw->_dragging) fw->setValue(v.value().w);
            }
          };
        }
      }
    }
  } else if (type == PropertyType::Int) {
    auto islider = std::dynamic_pointer_cast<IntSlider>(editor);
    if (islider) {
      return [islider](svar128_t new_value) {
        if (auto i = new_value.tryAs<int>()) {
          islider->setValue(i.value());
        }
      };
    }
  } else if (type == PropertyType::Asset || type == PropertyType::String) {
    auto le = std::dynamic_pointer_cast<LineEdit>(editor);
    if (le) {
      return [le](svar128_t new_value) {
        if (auto s = new_value.tryAs<std::string>()) {
          le->setValue(s.value());
        }
      };
    }
  }
  return nullptr;
}

void PropertySheet::_addRowsRecursive(const std::string& parent_key, int depth, int& y_offset, int& row_index) {
  if (!_model) return;

  auto children = _model->getChildren(parent_key);

  for (const auto& key : children) {
    std::string display_name = _model->getDisplayName(key);
    PropertyType type = _model->getPropertyType(key);
    bool has_children = _model->hasChildren(key);
    bool is_map = _model->isMapProperty(key);

    // For map properties, use MapViewState for expand/collapse (single vs all mode)
    // For normal properties, use _expanded_keys
    bool is_expanded;
    if (is_map) {
      // Map rows: always show children, _expanded means "all mode" (▼)
      is_expanded = _expanded_keys.count(key) > 0;
      // Ensure map rows always have an entry in expanded keys (always show children)
      if (!is_expanded && _map_view_states.find(key) == _map_view_states.end()) {
        _map_view_states[key] = MapViewState{true, 0};
      }
    } else {
      is_expanded = _expanded_keys.count(key) > 0;
    }

    // Create row
    auto row = std::make_shared<PropertyRow>("row_" + key, key, depth);
    row->setLabel(display_name);
    row->setHasChildren(has_children);
    row->setExpanded(is_expanded);
    row->_indent_width = _indent_width;
    row->_label_width = _label_width;
    row->_label_color = _label_color;
    row->_row_index = row_index++;
    row->_bg_color = (has_children || is_map) ? _group_color : _bgcolor;
    row->_alt_bg_color = (has_children || is_map) ? (_group_color * 0.9f) : (_bgcolor * 0.85f);
    row->_alt_bg_color.w = 1.0f;  // Keep full alpha

    // Map property support
    if (is_map) {
      row->_is_map_property = true;
      row->_is_map_const = _model->isMapConst(key);

      // [+] button: push OverlayLineEdit for adding new element
      row->_onMapAdd = [this, key](event_constptr_t ev) {
        auto map_children = _model->getChildren(key);
        std::string default_name = FormatString("item-%zu", map_children.size());
        auto lineedit = std::make_shared<OverlayLineEdit>("add_" + key, default_name);
        lineedit->_onCommit = [this, key](const std::string& name) {
          _model->addMapElement(key, name);
          rebuild();
          expandAll();
        };
        int sx = ev->miX;
        int sy = ev->miY;
        _uicontext->pushOverlay(lineedit, sx, sy, 200, 28, true, nullptr);
      };

      // [-] button: push DropdownMenu to select element to remove
      row->_onMapRemove = [this, key](event_constptr_t ev) {
        auto map_children = _model->getChildren(key);
        std::vector<std::string> names;
        for (auto& ck : map_children) {
          names.push_back(_model->getDisplayName(ck));
        }
        if (names.empty()) return;
        auto tree = DropdownMenu::buildTreeFromPaths(names);
        auto menu = std::make_shared<DropdownMenu>("remove_" + key, tree->root());
        menu->_onSelected = [this, key](std::string selected) {
          if (!selected.empty() && selected[0] == '/')
            selected = selected.substr(1);
          _model->removeMapElement(key, selected);
          rebuild();
          expandAll();
        };
        auto sz = menu->computeSize();
        int sx = ev->miX;
        int sy = ev->miY;
        _uicontext->pushOverlay(menu, sx, sy, int(sz.x), int(sz.y), true, nullptr);
      };

      // Double-click: push DropdownMenu to select current item (single mode)
      row->_onMapSelectItem = [this, key](event_constptr_t ev) {
        auto map_children = _model->getChildren(key);
        std::vector<std::string> names;
        for (auto& ck : map_children) {
          names.push_back(_model->getDisplayName(ck));
        }
        if (names.empty()) return;
        auto tree = DropdownMenu::buildTreeFromPaths(names);
        auto menu = std::make_shared<DropdownMenu>("select_" + key, tree->root());
        menu->_onSelected = [this, key, map_children](std::string selected) {
          for (int i = 0; i < (int)map_children.size(); i++) {
            if (_model->getDisplayName(map_children[i]) == selected) {
              _map_view_states[key].selected_index = i;
              _map_view_states[key].single_mode = true;
              // Switch to single mode (collapsed triangle)
              _expanded_keys.erase(key);
              rebuild();
              break;
            }
          }
        };
        auto sz = menu->computeSize();
        int sx = ev->miX;
        int sy = ev->miY;
        _uicontext->pushOverlay(menu, sx, sy, int(sz.x), int(sz.y), true, nullptr);
      };

      // Override expand toggle for map rows: toggle single/all mode
      row->_onExpandToggle = [this, key]() {
        bool was_expanded = _expanded_keys.count(key) > 0;
        if (was_expanded) {
          // Switch to single mode
          _expanded_keys.erase(key);
          _map_view_states[key].single_mode = true;
        } else {
          // Switch to all mode
          _expanded_keys.insert(key);
          _map_view_states[key].single_mode = false;
        }
        _needs_rebuild = true;
      };
    } else {
      // Normal expand/collapse callback
      row->_onExpandToggle = [this, key, is_expanded]() {
        setExpanded(key, !is_expanded);
      };
    }

    // Create editor widget for non-group, non-compound properties
    // Vec3 and Quat get inline compound editors (they look like leaf rows, not expandable groups)
    if (type == PropertyType::Vec3 || type == PropertyType::Vec4 || type == PropertyType::Quat) {
      svar128_t value = _model->getValue(key);
      auto editor = _createEditorWidget(key, type, value);
      if (editor) {
        row->setEditorWidget(editor);
        row->setHasChildren(false);  // Don't show disclosure triangle
        row->_refreshEditor = _makeRefreshCallback(editor, type);
      }
    } else if (!has_children && type != PropertyType::Group) {
      svar128_t value = _model->getValue(key);
      auto editor = _createEditorWidget(key, type, value);
      if (editor) {
        row->setEditorWidget(editor);
        row->_refreshEditor = _makeRefreshCallback(editor, type);
      }
    }

    // For null object map entries, show a factory widget instead of an empty group
    if (_model->isNullObjectMapEntry(key)) {
      auto factory_classes = _model->getFactoryClasses(key);
      if (!factory_classes.empty()) {
        auto factory_widget = std::make_shared<MapItemObjectFactoryWidget>("factory_" + key, factory_classes);
        factory_widget->_onFactorySelected = [this, key](const std::string& class_name) {
          _model->setMapElementFromFactory(key, class_name);
          rebuild();
          expandAll();
        };
        row->setEditorWidget(factory_widget);
      }
    }

    // For null direct object properties, show a factory widget
    if (_model->isNullDirectObjectEntry(key)) {
      auto factory_classes = _model->getDirectObjectFactoryClasses(key);
      if (!factory_classes.empty()) {
        auto factory_widget = std::make_shared<MapItemObjectFactoryWidget>("dfactory_" + key, factory_classes);
        factory_widget->_onFactorySelected = [this, key](const std::string& class_name) {
          _model->setDirectObjectFromFactory(key, class_name);
          rebuild();
          expandAll();
        };
        row->setEditorWidget(factory_widget);
      }
    }

    addChild(row);
    _rows[key] = row;

    y_offset += _row_height;

    // Recurse into children
    if (has_children) {
      if (is_map) {
        // Map properties: always show children (single or all mode)
        // Use is_expanded (from _expanded_keys) as source of truth
        auto map_children = _model->getChildren(key);
        if (!is_expanded && !map_children.empty()) {
          // Single mode (triangle right): show only selected child
          auto& mvs = _map_view_states[key];
          int idx = std::clamp(mvs.selected_index, 0, int(map_children.size()) - 1);
          _addSingleChildRecursive(map_children[idx], depth + 1, y_offset, row_index);
        } else {
          // All mode (triangle down): show all children
          _addRowsRecursive(key, depth + 1, y_offset, row_index);
        }
      } else if (is_expanded) {
        _addRowsRecursive(key, depth + 1, y_offset, row_index);
      }
    }
  }
}

void PropertySheet::_addSingleChildRecursive(const std::string& child_key, int depth, int& y_offset, int& row_index) {
  if (!_model) return;

  std::string display_name = _model->getDisplayName(child_key);
  PropertyType type = _model->getPropertyType(child_key);
  bool has_children = _model->hasChildren(child_key);
  bool is_expanded = _expanded_keys.count(child_key) > 0;

  // Create row
  auto row = std::make_shared<PropertyRow>("row_" + child_key, child_key, depth);
  row->setLabel(display_name);
  row->setHasChildren(has_children);
  row->setExpanded(is_expanded);
  row->_indent_width = _indent_width;
  row->_label_width = _label_width;
  row->_label_color = _label_color;
  row->_row_index = row_index++;
  row->_bg_color = has_children ? _group_color : _bgcolor;
  row->_alt_bg_color = has_children ? (_group_color * 0.9f) : (_bgcolor * 0.85f);
  row->_alt_bg_color.w = 1.0f;

  // Create editor widget for non-group, non-compound properties
  if (type == PropertyType::Vec3 || type == PropertyType::Vec4 || type == PropertyType::Quat) {
    svar128_t value = _model->getValue(child_key);
    auto editor = _createEditorWidget(child_key, type, value);
    if (editor) {
      row->setEditorWidget(editor);
      row->setHasChildren(false);
      row->_refreshEditor = _makeRefreshCallback(editor, type);
    }
  } else if (!has_children && type != PropertyType::Group) {
    svar128_t value = _model->getValue(child_key);
    auto editor = _createEditorWidget(child_key, type, value);
    if (editor) {
      row->setEditorWidget(editor);
      row->_refreshEditor = _makeRefreshCallback(editor, type);
    }
  }

  // For null object map entries, show a factory widget instead of an empty group
  if (_model->isNullObjectMapEntry(child_key)) {
    auto factory_classes = _model->getFactoryClasses(child_key);
    if (!factory_classes.empty()) {
      auto factory_widget = std::make_shared<MapItemObjectFactoryWidget>("factory_" + child_key, factory_classes);
      factory_widget->_onFactorySelected = [this, child_key](const std::string& class_name) {
        _model->setMapElementFromFactory(child_key, class_name);
        rebuild();
        expandAll();
      };
      row->setEditorWidget(factory_widget);
    }
  }

  // For null direct object properties, show a factory widget
  if (_model->isNullDirectObjectEntry(child_key)) {
    auto factory_classes = _model->getDirectObjectFactoryClasses(child_key);
    if (!factory_classes.empty()) {
      auto factory_widget = std::make_shared<MapItemObjectFactoryWidget>("dfactory_" + child_key, factory_classes);
      factory_widget->_onFactorySelected = [this, child_key](const std::string& class_name) {
        _model->setDirectObjectFromFactory(child_key, class_name);
        rebuild();
        expandAll();
      };
      row->setEditorWidget(factory_widget);
    }
  }

  // Set up expand/collapse callback
  row->_onExpandToggle = [this, child_key, is_expanded]() {
    setExpanded(child_key, !is_expanded);
  };

  addChild(row);
  _rows[child_key] = row;

  y_offset += _row_height;

  // Recurse if expanded
  if (has_children && is_expanded) {
    _addRowsRecursive(child_key, depth + 1, y_offset, row_index);
  }
}

void PropertySheet::_rebuildRows() {
  // Set this FIRST to prevent recursion (addChild/removeChild trigger layout)
  _needs_rebuild = false;

  // Release previous stale widgets (they've survived at least one full frame)
  _stale_widgets.clear();

  // Move old children to stale cache so they survive through the current
  // event processing cycle. This prevents dangling pointer crashes when
  // Context holds raw pointers (e.g. _mousefocuswidget, _evpushtarget)
  // to widgets that would otherwise be freed during rebuild.
  for (auto& child : _children) {
    _stale_widgets.push_back(child);
  }

  // Remove all existing children from the Group
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
