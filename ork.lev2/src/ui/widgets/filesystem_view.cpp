////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/filesystem_view.h>
#include <ork/lev2/ui/event.h>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

FilesystemView::FilesystemView(const std::string& name, int x, int y, int w, int h)
    : Widget(name, x, y, w, h) {
  _font = lev2::FontMan::fontForId("i14");
  _small_font = lev2::FontMan::fontForId("i12");
  _model = std::make_shared<LocalFilesystemModel>();
  _subscribeToModel();
}

FilesystemView::~FilesystemView() {
}

void FilesystemView::_subscribeToModel() {
  if (_model) {
    _model->_onModelChanged = [this]() {
      _needs_rebuild = true;
    };
    _model->_onDirectoryChanged = [this](const std::string& path) {
      _needs_rebuild = true;
      clearSelection();
      _scroll_offset = 0;
      if (_onDirectoryChanged) {
        _onDirectoryChanged(path);
      }
    };
  }
}

void FilesystemView::setModel(filesystem_model_ptr_t model) {
  _model = model;
  if (!_model) {
    _model = std::make_shared<LocalFilesystemModel>();
  }
  _subscribeToModel();
  _needs_rebuild = true;
  clearSelection();
  _scroll_offset = 0;
}

void FilesystemView::setViewMode(FilesystemViewMode mode) {
  if (_view_mode != mode) {
    _view_mode = mode;
    _needs_rebuild = true;
    _scroll_offset = 0;
  }
}

void FilesystemView::setSelectedPath(const std::string& path) {
  _selected_paths.clear();
  if (!path.empty()) {
    _selected_paths.insert(path);
  }
  if (_onSelect) {
    _onSelect(path);
  }
}

std::string FilesystemView::getSelectedPath() const {
  if (_selected_paths.empty()) {
    return "";
  }
  return *_selected_paths.begin();
}

void FilesystemView::addToSelection(const std::string& path) {
  if (!path.empty() && _allow_multiselect) {
    _selected_paths.insert(path);
    if (_onSelect) {
      _onSelect(path);
    }
  }
}

void FilesystemView::removeFromSelection(const std::string& path) {
  _selected_paths.erase(path);
}

void FilesystemView::clearSelection() {
  _selected_paths.clear();
}

void FilesystemView::selectAll() {
  if (!_allow_multiselect) return;
  _selected_paths.clear();
  for (const auto& item : _visible_items) {
    _selected_paths.insert(item.entry.path);
  }
}

bool FilesystemView::navigateTo(const std::string& path) {
  if (_model) {
    return _model->setCurrentPath(path);
  }
  return false;
}

bool FilesystemView::navigateUp() {
  if (_model) {
    std::string parent = _model->getParentPath();
    if (!parent.empty()) {
      return _model->setCurrentPath(parent);
    }
  }
  return false;
}

void FilesystemView::activateItem(const std::string& path) {
  if (!_model) return;

  if (_model->isDirectory(path)) {
    navigateTo(path);
  } else {
    if (_onActivate) {
      _onActivate(path);
    }
  }
}

void FilesystemView::refresh() {
  _needs_rebuild = true;
  _thumbnail_cache.clear();
}

void FilesystemView::startEditing(const std::string& path) {
  if (path.empty()) return;
  if (_model && _model->isReadOnly()) return;

  for (const auto& item : _visible_items) {
    if (item.entry.path == path) {
      _editing_path = path;
      _edit_value = item.entry.name;
      _original_value = item.entry.name;
      _cursor_pos = _edit_value.length();
      return;
    }
  }
}

void FilesystemView::cancelEditing() {
  _editing_path = "";
  _edit_value = "";
  _original_value = "";
  _cursor_pos = 0;
}

void FilesystemView::commitEditing() {
  if (_editing_path.empty()) return;

  std::string old_path = _editing_path;
  std::string new_name = _edit_value;

  if (new_name != _original_value && !new_name.empty()) {
    if (_model) {
      std::string new_path = _model->renameItem(old_path, new_name);
      if (!new_path.empty()) {
        // Update selection
        _selected_paths.erase(old_path);
        _selected_paths.insert(new_path);

        if (_onRename) {
          _onRename(old_path, new_name);
        }
      }
    }
  }

  cancelEditing();
  _needs_rebuild = true;
}

void FilesystemView::_doOnResized() {
  DoLayout();
}

void FilesystemView::DoLayout() {
  _rebuildVisibleItems();
}

void FilesystemView::_doGpuInit(lev2::Context* ctx) {
  _loadDefaultIcons(ctx);
}

void FilesystemView::_loadDefaultIcons(lev2::Context* ctx) {
  if (_icons_loaded) return;

  // For now, we won't load actual icons - the view will render colored rectangles
  // A full implementation would load icon textures here
  _icons_loaded = true;
}

void FilesystemView::_rebuildVisibleItems() {
  _visible_items.clear();

  if (_model) {
    auto entries = _model->getEntries();
    int index = 0;
    for (const auto& entry : entries) {
      VisibleItem item;
      item.entry = entry;
      item.index = index++;
      item.thumbnail = nullptr;
      item.thumbnail_requested = false;
      item.thumbnail_failed = false;
      _visible_items.push_back(item);
    }
  }

  _needs_rebuild = false;
  _clampScrollOffset();
}

void FilesystemView::_clampScrollOffset() {
  int content_height = 0;

  if (_view_mode == FilesystemViewMode::List) {
    content_height = _visible_items.size() * _item_height;
  } else {
    // Icon mode: calculate grid
    int available_width = _geometry._w - _icon_spacing * 2;
    int cell_width = _icon_size + _icon_spacing;
    int cols = std::max(1, available_width / cell_width);
    int rows = (_visible_items.size() + cols - 1) / cols;
    content_height = rows * (_icon_size + _icon_label_height + _icon_spacing);
  }

  int header_offset = (_draw_path_bar ? _path_bar_height : 0) +
                      ((_draw_header && _view_mode == FilesystemViewMode::List) ? _header_height : 0);
  int max_scroll = std::max(0, content_height - (_geometry._h - header_offset));
  _scroll_offset = std::clamp(_scroll_offset, 0, max_scroll);
}

int FilesystemView::_getItemIndexAt(int local_x, int local_y) const {
  int header_offset = (_draw_path_bar ? _path_bar_height : 0) +
                      ((_draw_header && _view_mode == FilesystemViewMode::List) ? _header_height : 0);

  int content_y = local_y - header_offset + _scroll_offset;
  if (content_y < 0) return -1;

  if (_view_mode == FilesystemViewMode::List) {
    int index = content_y / _item_height;
    if (index >= 0 && index < (int)_visible_items.size()) {
      return index;
    }
  } else {
    // Icon mode
    int available_width = _geometry._w - _icon_spacing * 2;
    int cell_width = _icon_size + _icon_spacing;
    int cell_height = _icon_size + _icon_label_height + _icon_spacing;
    int cols = std::max(1, available_width / cell_width);

    int col = (local_x - _icon_spacing) / cell_width;
    int row = content_y / cell_height;

    if (col >= 0 && col < cols) {
      int index = row * cols + col;
      if (index >= 0 && index < (int)_visible_items.size()) {
        return index;
      }
    }
  }

  return -1;
}

std::string FilesystemView::_getItemPathAt(int local_x, int local_y) const {
  int index = _getItemIndexAt(local_x, local_y);
  if (index >= 0 && index < (int)_visible_items.size()) {
    return _visible_items[index].entry.path;
  }
  return "";
}

Widget* FilesystemView::doRouteUiEvent(event_constptr_t ev) {
  if (IsEventInside(ev)) {
    return this;
  }
  return nullptr;
}

HandlerResult FilesystemView::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  int localX = 0;
  int localY = 0;
  RootToLocal(ev->miX, ev->miY, localX, localY);

  // Handle keyboard input when editing
  if (isEditing()) {
    switch (ev->_eventcode) {
      case EventCode::KEY_DOWN:
      case EventCode::KEY_REPEAT: {
        int key = ev->miKeyCode;
        switch (key) {
          case 256: // ESC
            cancelEditing();
            break;
          case 257: // Enter
            commitEditing();
            result._widget_finished = true;
            break;
          case 259: // Backspace
            if (_cursor_pos > 0) {
              _edit_value.erase(_cursor_pos - 1, 1);
              _cursor_pos--;
            }
            break;
          case 261: // Delete
            if (_cursor_pos < (int)_edit_value.length()) {
              _edit_value.erase(_cursor_pos, 1);
            }
            break;
          case 263: // Left
            if (_cursor_pos > 0) _cursor_pos--;
            break;
          case 262: // Right
            if (_cursor_pos < (int)_edit_value.length()) _cursor_pos++;
            break;
          default:
            if (key >= 32 && key <= 126) {
              char ch = ev->mbSHIFT ? char(key) : std::tolower(key);
              _edit_value.insert(_cursor_pos, 1, ch);
              _cursor_pos++;
            }
            break;
        }
        result.setHandled(this);
        return result;
      }
      case EventCode::PUSH: {
        std::string clicked_path = _getItemPathAt(localX, localY);
        if (clicked_path != _editing_path) {
          commitEditing();
        }
        result.setHandled(this);
        return result;
      }
      default:
        break;
    }
  }

  switch (ev->_eventcode) {
    case EventCode::PUSH: {
      std::string clicked_path = _getItemPathAt(localX, localY);
      if (!clicked_path.empty()) {
        bool is_already_selected = isSelected(clicked_path);

        if (ev->mbSHIFT && _allow_multiselect) {
          if (is_already_selected) {
            removeFromSelection(clicked_path);
          } else {
            addToSelection(clicked_path);
          }
        } else if (is_already_selected) {
          removeFromSelection(clicked_path);
        } else {
          setSelectedPath(clicked_path);
        }
        result.setHandled(this);
      }
      break;
    }

    case EventCode::DOUBLECLICK: {
      if (ev->mbSHIFT) {
        // Shift+double-click to rename
        std::string clicked_path = _getItemPathAt(localX, localY);
        if (!clicked_path.empty()) {
          startEditing(clicked_path);
          result.setHandled(this);
        }
      } else {
        // Double-click to activate
        std::string clicked_path = _getItemPathAt(localX, localY);
        if (!clicked_path.empty()) {
          activateItem(clicked_path);
          result.setHandled(this);
        }
      }
      break;
    }

    case EventCode::KEY_DOWN: {
      int key = ev->miKeyCode;
      std::string selected = getSelectedPath();

      // Backspace to go up
      if (key == 259 && !ev->mbSHIFT) {
        navigateUp();
        result.setHandled(this);
      }
      // Enter to activate
      else if (key == 257 && !selected.empty()) {
        activateItem(selected);
        result.setHandled(this);
      }
      // Delete selected items
      else if (key == 261 && ev->mbSHIFT && !_selected_paths.empty()) {
        if (_model && !_model->isReadOnly()) {
          std::vector<std::string> to_delete(_selected_paths.begin(), _selected_paths.end());
          clearSelection();
          for (const auto& path : to_delete) {
            _model->deleteItem(path);
            if (_onDelete) {
              _onDelete(path);
            }
          }
          _needs_rebuild = true;
        }
        result.setHandled(this);
      }
      // F2 to rename
      else if (key == 291 && _selected_paths.size() == 1) {
        startEditing(*_selected_paths.begin());
        result.setHandled(this);
      }
      // Arrow navigation
      else if (key == 265 || key == 264) { // Up/Down
        if (!_visible_items.empty()) {
          int current_index = -1;
          if (!selected.empty()) {
            for (size_t i = 0; i < _visible_items.size(); i++) {
              if (_visible_items[i].entry.path == selected) {
                current_index = i;
                break;
              }
            }
          }

          int new_index = current_index;
          if (key == 265) { // Up
            new_index = (current_index <= 0) ? _visible_items.size() - 1 : current_index - 1;
          } else { // Down
            new_index = (current_index < 0 || current_index >= (int)_visible_items.size() - 1) ? 0 : current_index + 1;
          }

          setSelectedPath(_visible_items[new_index].entry.path);
          result.setHandled(this);
        }
      }
      // Ctrl+A to select all
      else if (key == 'A' && ev->mbCTRL) {
        selectAll();
        result.setHandled(this);
      }
      break;
    }

    case EventCode::MOVE: {
      std::string hovered_path = _getItemPathAt(localX, localY);
      if (_hovered_path != hovered_path) {
        _hovered_path = hovered_path;
      }
      break;
    }

    case EventCode::MOUSE_LEAVE: {
      _hovered_path = "";
      break;
    }

    case EventCode::MOUSEWHEEL: {
      _scroll_offset -= ev->miMWY * 3;
      _clampScrollOffset();
      result.setHandled(this);
      break;
    }

    default:
      break;
  }

  return result;
}

void FilesystemView::DoDraw(drawevent_constptr_t drwev) {
  if (_needs_rebuild) {
    _rebuildVisibleItems();
  }

  auto tgt = drwev->GetTarget();
  auto fbi = tgt->FBI();

  // Get absolute position
  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);

  // Setup scissor
  fbi->pushScissor(lev2::ViewportRect(ix1, iy1, _geometry._w, _geometry._h));

  if (_view_mode == FilesystemViewMode::List) {
    _drawListMode(drwev);
  } else {
    _drawIconMode(drwev);
  }

  fbi->popScissor();
}

void FilesystemView::_drawListMode(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int iy2 = iy1 + _geometry._h;

  mtxi->PushUIMatrix();
  {
    // Draw background
    if (_draw_background) {
      auto rs = defmtl->_rasterstate;
      rs->setBlendingMacro(lev2::BlendingMacro::OFF);
      rs->setDepthTest(lev2::EDepthTest::OFF);
      fxi->pushRasterState(rs);
      tgt->PushModColor(_bgcolor);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      primi->RenderQuadAtZ(defmtl.get(), ix1, ix2, iy1, iy2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();
      fxi->popRasterState();
    }

    int y_offset = 0;

    // Draw path bar
    if (_draw_path_bar) {
      _drawPathBar(drwev, y_offset);
    }

    // Draw column header
    if (_draw_header) {
      _drawHeader(drwev, y_offset);
    }

    // Draw items
    int content_y = y_offset - _scroll_offset;
    for (size_t i = 0; i < _visible_items.size(); i++) {
      int item_y = iy1 + content_y + i * _item_height;

      // Skip items outside visible area
      if (item_y + _item_height < iy1 + y_offset) continue;
      if (item_y > iy2) break;

      bool selected = isSelected(_visible_items[i].entry.path);
      bool hovered = (_visible_items[i].entry.path == _hovered_path);

      _drawListItem(drwev, _visible_items[i], item_y, selected, hovered);
    }
  }
  mtxi->PopUIMatrix();
}

void FilesystemView::_drawIconMode(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int iy2 = iy1 + _geometry._h;

  mtxi->PushUIMatrix();
  {
    // Draw background
    if (_draw_background) {
      auto rs = defmtl->_rasterstate;
      rs->setBlendingMacro(lev2::BlendingMacro::OFF);
      rs->setDepthTest(lev2::EDepthTest::OFF);
      fxi->pushRasterState(rs);
      tgt->PushModColor(_bgcolor);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      primi->RenderQuadAtZ(defmtl.get(), ix1, ix2, iy1, iy2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();
      fxi->popRasterState();
    }

    int y_offset = 0;

    // Draw path bar
    if (_draw_path_bar) {
      _drawPathBar(drwev, y_offset);
    }

    // Calculate grid
    int available_width = _geometry._w - _icon_spacing * 2;
    int cell_width = _icon_size + _icon_spacing;
    int cell_height = _icon_size + _icon_label_height + _icon_spacing;
    int cols = std::max(1, available_width / cell_width);

    // Draw items
    for (size_t i = 0; i < _visible_items.size(); i++) {
      int col = i % cols;
      int row = i / cols;

      int item_x = ix1 + _icon_spacing + col * cell_width;
      int item_y = iy1 + y_offset - _scroll_offset + row * cell_height;

      // Skip items outside visible area
      if (item_y + cell_height < iy1 + y_offset) continue;
      if (item_y > iy2) break;

      bool selected = isSelected(_visible_items[i].entry.path);
      bool hovered = (_visible_items[i].entry.path == _hovered_path);

      _drawIconItem(drwev, _visible_items[i], item_x, item_y, selected, hovered);
    }
  }
  mtxi->PopUIMatrix();
}

void FilesystemView::_drawPathBar(drawevent_constptr_t drwev, int& y_offset) {
  auto tgt = drwev->GetTarget();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int bar_y1 = iy1 + y_offset;
  int bar_y2 = bar_y1 + _path_bar_height;

  // Draw background
  auto rs = defmtl->_rasterstate;
  rs->setBlendingMacro(lev2::BlendingMacro::OFF);
  rs->setDepthTest(lev2::EDepthTest::OFF);
  fxi->pushRasterState(rs);
  tgt->PushModColor(_header_bgcolor);
  defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
  primi->RenderQuadAtZ(defmtl.get(), ix1, ix2, bar_y1, bar_y2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
  tgt->PopModColor();
  fxi->popRasterState();

  // Draw path text
  if (_font && _model) {
    std::string path = _model->getCurrentPath();
    lev2::FontMan::PushFont(_font);
    tgt->PushModColor(_text_color);
    lev2::FontMan::beginTextBlock(tgt, path.length());
    int text_y = bar_y1 + (_path_bar_height - _font->description().miAdvanceHeight) / 2;
    lev2::FontMan::DrawText(tgt, ix1 + 8, text_y, path.c_str());
    lev2::FontMan::endTextBlock(tgt);
    tgt->PopModColor();
    lev2::FontMan::PopFont();
  }

  y_offset += _path_bar_height;
}

void FilesystemView::_drawHeader(drawevent_constptr_t drwev, int& y_offset) {
  auto tgt = drwev->GetTarget();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int header_y1 = iy1 + y_offset;
  int header_y2 = header_y1 + _header_height;

  // Draw background
  auto rs = defmtl->_rasterstate;
  rs->setBlendingMacro(lev2::BlendingMacro::OFF);
  rs->setDepthTest(lev2::EDepthTest::OFF);
  fxi->pushRasterState(rs);
  tgt->PushModColor(_header_bgcolor * 0.9f);
  defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
  primi->RenderQuadAtZ(defmtl.get(), ix1, ix2, header_y1, header_y2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
  tgt->PopModColor();
  fxi->popRasterState();

  // Draw column headers
  if (_font) {
    lev2::FontMan::PushFont(_font);
    tgt->PushModColor(_text_color * 0.8f);

    int x = ix1 + _icon_column_width;
    int text_y = header_y1 + (_header_height - _font->description().miAdvanceHeight) / 2;

    lev2::FontMan::beginTextBlock(tgt, 64);
    lev2::FontMan::DrawText(tgt, x, text_y, "Name");
    x += _name_column_width;

    if (_show_size_column) {
      lev2::FontMan::DrawText(tgt, x, text_y, "Size");
      x += _size_column_width;
    }
    if (_show_type_column) {
      lev2::FontMan::DrawText(tgt, x, text_y, "Type");
      x += _type_column_width;
    }
    if (_show_date_column) {
      lev2::FontMan::DrawText(tgt, x, text_y, "Modified");
    }
    lev2::FontMan::endTextBlock(tgt);

    tgt->PopModColor();
    lev2::FontMan::PopFont();
  }

  y_offset += _header_height;
}

void FilesystemView::_drawListItem(drawevent_constptr_t drwev, const VisibleItem& item,
                                    int y_pos, bool selected, bool hovered) {
  auto tgt = drwev->GetTarget();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;

  // Draw selection/hover background
  if (selected || hovered) {
    fvec4 bg_color = selected ? _selected_color : _hover_color;
    auto rs = defmtl->_rasterstate;
    rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    rs->setDepthTest(lev2::EDepthTest::OFF);
    fxi->pushRasterState(rs);
    tgt->PushModColor(bg_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(defmtl.get(), ix1, ix2, y_pos, y_pos + _item_height,
                         0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    tgt->PopModColor();
    fxi->popRasterState();
  }

  // Draw icon placeholder (colored rectangle)
  {
    fvec4 icon_color = (item.entry.type == FileType::Directory) ? _directory_color : fvec4(0.6f, 0.6f, 0.6f, 1.0f);
    auto rs = defmtl->_rasterstate;
    rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    fxi->pushRasterState(rs);
    tgt->PushModColor(icon_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    int icon_size = _item_height - 4;
    primi->RenderQuadAtZ(defmtl.get(),
                         ix1 + 2, ix1 + 2 + icon_size,
                         y_pos + 2, y_pos + 2 + icon_size,
                         0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    tgt->PopModColor();
    fxi->popRasterState();
  }

  // Draw text
  if (_font) {
    fvec4 text_col = (item.entry.type == FileType::Directory) ? _directory_color : _text_color;
    lev2::FontMan::PushFont(_font);
    tgt->PushModColor(text_col);

    int x = ix1 + _icon_column_width;
    int text_y = y_pos + (_item_height - _font->description().miAdvanceHeight) / 2;

    size_t char_count = item.entry.name.length();
    if (_show_size_column) char_count += 16;
    if (_show_type_column) char_count += item.entry.extension.length() + 8;
    if (_show_date_column) char_count += 24;

    lev2::FontMan::beginTextBlock(tgt, char_count);

    // Name
    lev2::FontMan::DrawText(tgt, x, text_y, item.entry.name.c_str());
    x += _name_column_width;

    // Size
    if (_show_size_column) {
      if (item.entry.type == FileType::File) {
        std::string size_str = _formatSize(item.entry.size);
        lev2::FontMan::DrawText(tgt, x, text_y, size_str.c_str());
      }
      x += _size_column_width;
    }

    // Type
    if (_show_type_column) {
      std::string type_str = (item.entry.type == FileType::Directory) ? "Folder" : item.entry.extension;
      lev2::FontMan::DrawText(tgt, x, text_y, type_str.c_str());
      x += _type_column_width;
    }

    // Date
    if (_show_date_column) {
      std::string date_str = _formatDate(item.entry.modified_time);
      lev2::FontMan::DrawText(tgt, x, text_y, date_str.c_str());
    }

    lev2::FontMan::endTextBlock(tgt);
    tgt->PopModColor();
    lev2::FontMan::PopFont();
  }
}

void FilesystemView::_drawIconItem(drawevent_constptr_t drwev, const VisibleItem& item,
                                    int x_pos, int y_pos, bool selected, bool hovered) {
  auto tgt = drwev->GetTarget();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int cell_width = _icon_size + _icon_spacing;
  int cell_height = _icon_size + _icon_label_height;

  // Draw selection/hover background
  if (selected || hovered) {
    fvec4 bg_color = selected ? _selected_color : _hover_color;
    auto rs = defmtl->_rasterstate;
    rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    rs->setDepthTest(lev2::EDepthTest::OFF);
    fxi->pushRasterState(rs);
    tgt->PushModColor(bg_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(defmtl.get(),
                         x_pos, x_pos + cell_width - _icon_spacing,
                         y_pos, y_pos + cell_height,
                         0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    tgt->PopModColor();
    fxi->popRasterState();
  }

  // Draw icon placeholder
  {
    fvec4 icon_color = (item.entry.type == FileType::Directory) ? _directory_color : fvec4(0.5f, 0.5f, 0.5f, 1.0f);
    auto rs = defmtl->_rasterstate;
    rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    fxi->pushRasterState(rs);
    tgt->PushModColor(icon_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(defmtl.get(),
                         x_pos + 4, x_pos + _icon_size - 4,
                         y_pos + 4, y_pos + _icon_size - 4,
                         0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    tgt->PopModColor();
    fxi->popRasterState();
  }

  // Draw label
  if (_small_font) {
    fvec4 text_col = (item.entry.type == FileType::Directory) ? _directory_color : _text_color;
    lev2::FontMan::PushFont(_small_font);
    tgt->PushModColor(text_col);

    // Truncate name if too long
    std::string label = item.entry.name;
    int max_chars = (cell_width - 4) / _small_font->description().miAdvanceWidth;
    if ((int)label.length() > max_chars && max_chars > 3) {
      label = label.substr(0, max_chars - 3) + "...";
    }

    int text_x = x_pos + (cell_width - _icon_spacing - label.length() * _small_font->description().miAdvanceWidth) / 2;
    int text_y = y_pos + _icon_size + 4;

    lev2::FontMan::beginTextBlock(tgt, label.length());
    lev2::FontMan::DrawText(tgt, text_x, text_y, label.c_str());
    lev2::FontMan::endTextBlock(tgt);

    tgt->PopModColor();
    lev2::FontMan::PopFont();
  }
}

std::string FilesystemView::_formatSize(size_t bytes) {
  const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  int unit_index = 0;
  double size = bytes;

  while (size >= 1024 && unit_index < 4) {
    size /= 1024;
    unit_index++;
  }

  std::ostringstream ss;
  if (unit_index == 0) {
    ss << bytes << " B";
  } else {
    ss << std::fixed << std::setprecision(1) << size << " " << units[unit_index];
  }
  return ss.str();
}

std::string FilesystemView::_formatDate(time_t time) {
  if (time == 0) return "";

  std::tm* tm = std::localtime(&time);
  if (!tm) return "";

  std::ostringstream ss;
  ss << std::put_time(tm, "%Y-%m-%d %H:%M");
  return ss.str();
}

lev2::texture_ptr_t FilesystemView::_getDefaultIcon(FileType type, const std::string& extension) {
  // For now, return nullptr - icons rendered as colored rectangles
  return nullptr;
}

void FilesystemView::_requestThumbnail(VisibleItem& item) {
  if (item.thumbnail_requested || item.thumbnail_failed) return;
  if (!_model) return;

  item.thumbnail_requested = true;

  auto provider = _model->getThumbnailProvider(item.entry.path, _icon_size);
  if (!provider) {
    item.thumbnail_failed = true;
    return;
  }

  // For now, synchronously get thumbnail
  // A production version would use async loading
  auto image = provider->_func();
  if (image) {
    // Would convert image to texture here
    // For now, just mark as having thumbnail
  } else {
    item.thumbnail_failed = true;
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
