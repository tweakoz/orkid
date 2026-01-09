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
#include <algorithm>
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
      clearIconCache();  // Clear icon cache on directory change
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
  clearIconCache();  // Clear icon cache when model changes
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

void FilesystemView::setFolderIcon(lev2::image_ptr_t img) {
  _folder_icon_image = img;
  _folder_icon_texture = nullptr;  // Invalidate cached texture
}

void FilesystemView::setFileIcon(lev2::image_ptr_t img) {
  _file_icon_image = img;
  _file_icon_texture = nullptr;  // Invalidate cached texture
}

///////////////////////////////////////////////////////////////////////////////
// Favorites management
///////////////////////////////////////////////////////////////////////////////

void FilesystemView::addCurrentAsFavorite(const std::string& display_name) {
  if (!_model) return;

  auto entry = FavoriteEntry::fromModel(_model.get(), display_name);
  auto favorites = FavoritesManager::instance();
  favorites->addFavoriteEntry(_model->modelIdentifier(), entry);
}

void FilesystemView::removeCurrentFromFavorites() {
  if (!_model) return;

  auto favorites = FavoritesManager::instance();
  favorites->removeFavoriteEntry(_model->modelIdentifier(), _model->getCurrentPath());
}

bool FilesystemView::isCurrentFavorite() const {
  if (!_model) return false;

  auto favorites = FavoritesManager::instance();
  return favorites->isFavorite(_model->modelIdentifier(), _model->getCurrentPath());
}

void FilesystemView::applyFavorite(favorite_entry_ptr_t entry) {
  if (!_model || !entry) return;

  entry->applyToModel(_model.get());
  _needs_rebuild = true;
  _scroll_offset = 0;
  clearSelection();
}

favorite_entry_ptr_t FilesystemView::getCurrentAsFavoriteEntry(const std::string& display_name) const {
  if (!_model) return nullptr;

  return FavoriteEntry::fromModel(_model.get(), display_name);
}

///////////////////////////////////////////////////////////////////////////////

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
  // Proportionally resize columns when widget is resized
  int content_width = _geometry._w - _icon_column_width;

  if (_last_content_width > 0 && content_width > 0 && content_width != _last_content_width) {
    float scale = (float)content_width / (float)_last_content_width;

    _name_column_width = std::max(60, (int)(_name_column_width * scale));
    _size_column_width = std::max(50, (int)(_size_column_width * scale));
    _type_column_width = std::max(50, (int)(_type_column_width * scale));
    _date_column_width = std::max(80, (int)(_date_column_width * scale));
  }

  _last_content_width = content_width;
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

bool FilesystemView::_isInHeaderArea(int local_y) const {
  if (!_draw_header || _view_mode != FilesystemViewMode::List) {
    return false;
  }
  int header_start = _draw_path_bar ? _path_bar_height : 0;
  int header_end = header_start + _header_height;
  return local_y >= header_start && local_y < header_end;
}

FilesystemModel::SortField FilesystemView::_getSortFieldAtX(int local_x) const {
  int x = _icon_column_width;

  // Name column
  if (local_x < x + _name_column_width) {
    return FilesystemModel::SortField::Name;
  }
  x += _name_column_width;

  // Size column
  if (_show_size_column) {
    if (local_x < x + _size_column_width) {
      return FilesystemModel::SortField::Size;
    }
    x += _size_column_width;
  }

  // Type column
  if (_show_type_column) {
    if (local_x < x + _type_column_width) {
      return FilesystemModel::SortField::Type;
    }
    x += _type_column_width;
  }

  // Date column
  if (_show_date_column) {
    return FilesystemModel::SortField::ModifiedTime;
  }

  return FilesystemModel::SortField::Name;
}

void FilesystemView::_handleHeaderClick(int local_x) {
  if (!_model) return;

  auto clicked_field = _getSortFieldAtX(local_x);
  auto current_field = _model->getSortField();
  auto current_order = _model->getSortOrder();

  if (clicked_field == current_field) {
    // Same column - toggle order
    auto new_order = (current_order == FilesystemModel::SortOrder::Ascending)
                       ? FilesystemModel::SortOrder::Descending
                       : FilesystemModel::SortOrder::Ascending;
    _model->setSortOrder(new_order);
  } else {
    // Different column - set field and default to ascending
    _model->setSortField(clicked_field);
    _model->setSortOrder(FilesystemModel::SortOrder::Ascending);
  }

  _needs_rebuild = true;
}

int FilesystemView::_getColumnSeparatorAt(int local_x, int local_y) const {
  if (!_isInHeaderArea(local_y)) {
    return -1;
  }

  int x = _icon_column_width;

  // Check name column separator
  int sep_x = x + _name_column_width;
  if (local_x >= sep_x - _resize_grip_width && local_x <= sep_x + _resize_grip_width) {
    return 0;  // Name column
  }
  x = sep_x;

  // Check size column separator
  if (_show_size_column) {
    sep_x = x + _size_column_width;
    if (local_x >= sep_x - _resize_grip_width && local_x <= sep_x + _resize_grip_width) {
      return 1;  // Size column
    }
    x = sep_x;
  }

  // Check type column separator
  if (_show_type_column) {
    sep_x = x + _type_column_width;
    if (local_x >= sep_x - _resize_grip_width && local_x <= sep_x + _resize_grip_width) {
      return 2;  // Type column
    }
    x = sep_x;
  }

  // Date column doesn't have a separator on the right (it extends to edge)
  return -1;
}

int* FilesystemView::_getColumnWidthPtr(int column_index) {
  switch (column_index) {
    case 0: return &_name_column_width;
    case 1: return &_size_column_width;
    case 2: return &_type_column_width;
    case 3: return &_date_column_width;
    default: return nullptr;
  }
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
      // Check if click is on column separator (for resizing)
      int sep_col = _getColumnSeparatorAt(localX, localY);
      if (sep_col >= 0) {
        _resize_column = sep_col;
        _resize_start_x = localX;
        int* width_ptr = _getColumnWidthPtr(sep_col);
        _resize_start_width = width_ptr ? *width_ptr : 0;
        result.setHandled(this);
        break;
      }

      // Check if click is on header (for sorting)
      if (_isInHeaderArea(localY)) {
        _handleHeaderClick(localX);
        result.setHandled(this);
        break;
      }

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
      // Escape to clear selection
      else if (key == 256) {
        clearSelection();
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

    case EventCode::DRAG: {
      // Handle column resize drag
      if (_resize_column >= 0) {
        int delta = localX - _resize_start_x;
        int new_width = std::max(40, _resize_start_width + delta);  // Minimum 40px
        int* width_ptr = _getColumnWidthPtr(_resize_column);
        if (width_ptr) {
          *width_ptr = new_width;
        }
        result.setHandled(this);
      }
      break;
    }

    case EventCode::RELEASE: {
      // End column resize
      if (_resize_column >= 0) {
        _resize_column = -1;
        result.setHandled(this);
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

      _drawListItem(drwev, _visible_items[i], item_y, selected, hovered, i);
    }

    // Draw column separator lines (subtle for content area)
    {
      auto rs = defmtl->_rasterstate;
      rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
      rs->setDepthTest(lev2::EDepthTest::OFF);
      fxi->pushRasterState(rs);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);

      fvec4 sep_color = fvec4(0.25f, 0.25f, 0.28f, 1.0f);  // Subtle for content
      tgt->PushModColor(sep_color);

      int content_top = iy1 + y_offset;
      int sep_x = ix1 + _icon_column_width + _name_column_width;
      primi->RenderQuadAtZ(defmtl.get(), sep_x, sep_x + 1, content_top, iy2,
                           0.0f, 0.0f, 1.0f, 0.0f, 1.0f);

      if (_show_size_column) {
        sep_x += _size_column_width;
        primi->RenderQuadAtZ(defmtl.get(), sep_x, sep_x + 1, content_top, iy2,
                             0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      }

      if (_show_type_column) {
        sep_x += _type_column_width;
        primi->RenderQuadAtZ(defmtl.get(), sep_x, sep_x + 1, content_top, iy2,
                             0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      }

      tgt->PopModColor();
      fxi->popRasterState();
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

  // Draw column headers with sort indicators
  if (_font) {
    lev2::FontMan::PushFont(_font);

    // Get current sort state
    auto sort_field = _model ? _model->getSortField() : FilesystemModel::SortField::Name;
    auto sort_order = _model ? _model->getSortOrder() : FilesystemModel::SortOrder::Ascending;
    const char* arrow_up = " ^";
    const char* arrow_down = " v";

    const int col_padding = 8;  // Padding after column divider
    int x = ix1 + _icon_column_width;
    int text_y = header_y1 + (_header_height - _font->description().miAdvanceHeight) / 2;

    lev2::FontMan::beginTextBlock(tgt, 80);

    // Name column
    bool is_name_sorted = (sort_field == FilesystemModel::SortField::Name);
    tgt->PushModColor(is_name_sorted ? _text_color : _text_color * 0.7f);
    if (is_name_sorted) {
      std::string label = std::string("Name") + (sort_order == FilesystemModel::SortOrder::Ascending ? arrow_up : arrow_down);
      lev2::FontMan::DrawText(tgt, x, text_y, label.c_str());
    } else {
      lev2::FontMan::DrawText(tgt, x, text_y, "Name");
    }
    tgt->PopModColor();
    x += _name_column_width;

    // Size column
    if (_show_size_column) {
      bool is_size_sorted = (sort_field == FilesystemModel::SortField::Size);
      tgt->PushModColor(is_size_sorted ? _text_color : _text_color * 0.7f);
      if (is_size_sorted) {
        std::string label = std::string("Size") + (sort_order == FilesystemModel::SortOrder::Ascending ? arrow_up : arrow_down);
        lev2::FontMan::DrawText(tgt, x + col_padding, text_y, label.c_str());
      } else {
        lev2::FontMan::DrawText(tgt, x + col_padding, text_y, "Size");
      }
      tgt->PopModColor();
      x += _size_column_width;
    }

    // Type column
    if (_show_type_column) {
      bool is_type_sorted = (sort_field == FilesystemModel::SortField::Type);
      tgt->PushModColor(is_type_sorted ? _text_color : _text_color * 0.7f);
      if (is_type_sorted) {
        std::string label = std::string("Type") + (sort_order == FilesystemModel::SortOrder::Ascending ? arrow_up : arrow_down);
        lev2::FontMan::DrawText(tgt, x + col_padding, text_y, label.c_str());
      } else {
        lev2::FontMan::DrawText(tgt, x + col_padding, text_y, "Type");
      }
      tgt->PopModColor();
      x += _type_column_width;
    }

    // Date column
    if (_show_date_column) {
      bool is_date_sorted = (sort_field == FilesystemModel::SortField::ModifiedTime);
      tgt->PushModColor(is_date_sorted ? _text_color : _text_color * 0.7f);
      if (is_date_sorted) {
        std::string label = std::string("Modified") + (sort_order == FilesystemModel::SortOrder::Ascending ? arrow_up : arrow_down);
        lev2::FontMan::DrawText(tgt, x + col_padding, text_y, label.c_str());
      } else {
        lev2::FontMan::DrawText(tgt, x + col_padding, text_y, "Modified");
      }
      tgt->PopModColor();
    }

    lev2::FontMan::endTextBlock(tgt);
    lev2::FontMan::PopFont();
  }

  // Draw column separator lines (brighter in header)
  {
    auto rs = defmtl->_rasterstate;
    rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    rs->setDepthTest(lev2::EDepthTest::OFF);
    fxi->pushRasterState(rs);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);

    fvec4 sep_color = fvec4(0.4f, 0.4f, 0.45f, 1.0f);  // Brighter for header
    tgt->PushModColor(sep_color);

    int sep_x = ix1 + _icon_column_width + _name_column_width;
    primi->RenderQuadAtZ(defmtl.get(), sep_x - 1, sep_x + 1, header_y1 + 2, header_y2 - 2,
                         0.0f, 0.0f, 1.0f, 0.0f, 1.0f);

    if (_show_size_column) {
      sep_x += _size_column_width;
      primi->RenderQuadAtZ(defmtl.get(), sep_x - 1, sep_x + 1, header_y1 + 2, header_y2 - 2,
                           0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    }

    if (_show_type_column) {
      sep_x += _type_column_width;
      primi->RenderQuadAtZ(defmtl.get(), sep_x - 1, sep_x + 1, header_y1 + 2, header_y2 - 2,
                           0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    }

    tgt->PopModColor();
    fxi->popRasterState();
  }

  y_offset += _header_height;
}

void FilesystemView::_drawListItem(drawevent_constptr_t drwev, const VisibleItem& item,
                                    int y_pos, bool selected, bool hovered, int row_index) {
  auto tgt = drwev->GetTarget();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;

  // Draw alternating row background
  if (!selected && !hovered && (row_index % 2 == 1)) {
    fvec4 alt_bg_color = _bgcolor * 1.15f;  // Slightly lighter for odd rows
    alt_bg_color.w = 1.0f;
    auto rs = defmtl->_rasterstate;
    rs->setBlendingMacro(lev2::BlendingMacro::OFF);
    rs->setDepthTest(lev2::EDepthTest::OFF);
    fxi->pushRasterState(rs);
    tgt->PushModColor(alt_bg_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(defmtl.get(), ix1, ix2, y_pos, y_pos + _item_height,
                         0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    tgt->PopModColor();
    fxi->popRasterState();
  }

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

  // Draw icon
  {
    int icon_size = _item_height - 4;
    bool is_directory = (item.entry.type == FileType::Directory);
    auto icon_texture = _getIconForPath(tgt, item.entry.path, item.entry.type, icon_size);

    if (icon_texture) {
      // Lazy init textured material
      if (!_tex_material) {
        _tex_material = std::make_shared<lev2::GfxMaterialUITextured>(tgt, "uitextured");
      }

      // Render texture icon with alpha blending
      auto rs = _tex_material->_rasterstate;
      rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
      rs->setDepthTest(lev2::EDepthTest::OFF);
      rs->_priority = 1;
      fxi->pushRasterState(rs);
      tgt->PushModColor(fvec4(1, 1, 1, 1));
      _tex_material->SetTexture(lev2::ETEXDEST_DIFFUSE, icon_texture.get());
      primi->RenderQuadAtZ(_tex_material.get(),
                           ix1 + 2, ix1 + 2 + icon_size,
                           y_pos + 2, y_pos + 2 + icon_size,
                           0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();
      fxi->popRasterState();
    } else {
      // Fallback: colored rectangle
      fvec4 icon_color = is_directory ? _directory_color : fvec4(0.6f, 0.6f, 0.6f, 1.0f);
      auto rs = defmtl->_rasterstate;
      rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
      fxi->pushRasterState(rs);
      tgt->PushModColor(icon_color);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      primi->RenderQuadAtZ(defmtl.get(),
                           ix1 + 2, ix1 + 2 + icon_size,
                           y_pos + 2, y_pos + 2 + icon_size,
                           0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();
      fxi->popRasterState();
    }
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

    const int col_padding = 8;  // Padding after column divider

    // Name (truncate to column width)
    std::string name_str = _truncateToWidth(item.entry.name, _name_column_width);
    lev2::FontMan::DrawText(tgt, x, text_y, name_str.c_str());
    x += _name_column_width;

    // Size
    if (_show_size_column) {
      if (item.entry.type == FileType::File) {
        std::string size_str = _formatSize(item.entry.size);
        size_str = _truncateToWidth(size_str, _size_column_width - col_padding);
        lev2::FontMan::DrawText(tgt, x + col_padding, text_y, size_str.c_str());
      }
      x += _size_column_width;
    }

    // Type
    if (_show_type_column) {
      std::string type_str = (item.entry.type == FileType::Directory) ? "Folder" : item.entry.extension;
      type_str = _truncateToWidth(type_str, _type_column_width - col_padding);
      lev2::FontMan::DrawText(tgt, x + col_padding, text_y, type_str.c_str());
      x += _type_column_width;
    }

    // Date
    if (_show_date_column) {
      std::string date_str = _formatDate(item.entry.modified_time);
      date_str = _truncateToWidth(date_str, _date_column_width - col_padding);
      lev2::FontMan::DrawText(tgt, x + col_padding, text_y, date_str.c_str());
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

  // Draw icon
  {
    bool is_directory = (item.entry.type == FileType::Directory);
    auto icon_texture = _getIconForPath(tgt, item.entry.path, item.entry.type, _icon_size);

    if (icon_texture) {
      // Lazy init textured material
      if (!_tex_material) {
        _tex_material = std::make_shared<lev2::GfxMaterialUITextured>(tgt, "uitextured");
      }

      // Render texture icon with alpha blending
      auto rs = _tex_material->_rasterstate;
      rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
      rs->setDepthTest(lev2::EDepthTest::OFF);
      rs->_priority = 1;
      fxi->pushRasterState(rs);
      tgt->PushModColor(fvec4(1, 1, 1, 1));
      _tex_material->SetTexture(lev2::ETEXDEST_DIFFUSE, icon_texture.get());
      primi->RenderQuadAtZ(_tex_material.get(),
                           x_pos + 4, x_pos + _icon_size - 4,
                           y_pos + 4, y_pos + _icon_size - 4,
                           0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();
      fxi->popRasterState();
    } else {
      // Fallback: colored rectangle
      fvec4 icon_color = is_directory ? _directory_color : fvec4(0.5f, 0.5f, 0.5f, 1.0f);
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

std::string FilesystemView::_truncateToWidth(const std::string& text, int max_width) const {
  if (!_font || text.empty()) return text;

  int char_width = _font->description().miAdvanceWidth;
  if (char_width <= 0) return text;

  int max_chars = (max_width - 8) / char_width;  // 8px padding
  if (max_chars <= 0) return "";
  if ((int)text.length() <= max_chars) return text;
  if (max_chars <= 3) return text.substr(0, max_chars);

  return text.substr(0, max_chars - 3) + "...";
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

lev2::texture_ptr_t FilesystemView::_getIconForPath(lev2::Context* ctx, const std::string& path, FileType type, int size) {
  auto txi = ctx->TXI();

  // Lazily convert default icon images to textures
  if (_folder_icon_image && !_folder_icon_texture) {
    _folder_icon_texture = std::make_shared<lev2::Texture>();
    txi->initTextureFromImage(_folder_icon_texture.get(), _folder_icon_image, true);
  }
  if (_file_icon_image && !_file_icon_texture) {
    _file_icon_texture = std::make_shared<lev2::Texture>();
    txi->initTextureFromImage(_file_icon_texture.get(), _file_icon_image, true);
  }

  if (!_model) {
    return (type == FileType::Directory) ? _folder_icon_texture : _file_icon_texture;
  }

  // Check cache first
  auto it = _icon_cache.find(path);
  if (it != _icon_cache.end()) {
    return it->second;
  }

  // Try to get icon from model
  lev2::image_ptr_t image = nullptr;

  // First try provider (lazy loading)
  auto provider = _model->getIconProvider(path, size);
  if (provider) {
    image = provider->_func();
  }

  // Fall back to direct image
  if (!image) {
    image = _model->getIcon(path, size);
  }

  // If model provides an image, create texture and cache it
  if (image) {
    auto texture = std::make_shared<lev2::Texture>();
    txi->initTextureFromImage(texture.get(), image, true);
    _icon_cache[path] = texture;
    return texture;
  }

  // Fall back to default icons (don't cache - use defaults directly)
  return (type == FileType::Directory) ? _folder_icon_texture : _file_icon_texture;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
