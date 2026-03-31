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
#include <ork/lev2/ui/dropdown_menu.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/context.h>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////
// PathBarWidget
///////////////////////////////////////////////////////////////////////////////

PathBarWidget::PathBarWidget(FilesystemView* parent)
    : Widget("path_bar", 0, 0, 0, 0)
    , _fsview(parent) {
}

void PathBarWidget::DoDraw(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int bar_y1 = iy1;
  int bar_y2 = iy1 + _geometry._h;

  auto mtxi = tgt->MTXI();
  mtxi->PushUIMatrix();

  // Draw background
  auto rs = defmtl->_rasterstate;
  rs->setBlendingMacro(lev2::BlendingMacro::OFF);
  rs->setDepthTest(lev2::EDepthTest::OFF);
  fxi->pushRasterState(rs);
  tgt->PushModColor(_fsview->_header_bgcolor);
  defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
  primi->RenderQuadAtZ(defmtl.get(), ix1, ix2, bar_y1, bar_y2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
  tgt->PopModColor();
  fxi->popRasterState();

  // Draw path text
  if (_fsview->_font && _fsview->_model) {
    std::string path = _fsview->_model->getCurrentPath();
    lev2::FontMan::PushFont(_fsview->_font);
    tgt->PushModColor(_fsview->_text_color);
    lev2::FontMan::beginTextBlock(tgt, path.length());
    int text_y = bar_y1 + (_geometry._h - _fsview->_font->description().miAdvanceHeight) / 2;
    lev2::FontMan::DrawText(tgt, ix1 + 8, text_y, path.c_str());
    lev2::FontMan::endTextBlock(tgt);
    tgt->PopModColor();
    lev2::FontMan::PopFont();
  }

  mtxi->PopUIMatrix();
}

HandlerResult PathBarWidget::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;
  // Path bar click handling could be added here (breadcrumb navigation)
  return result;
}

Widget* PathBarWidget::doRouteUiEvent(event_constptr_t ev) {
  if (IsEventInside(ev)) return this;
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// HeaderWidget
///////////////////////////////////////////////////////////////////////////////

HeaderWidget::HeaderWidget(FilesystemView* parent)
    : Widget("header", 0, 0, 0, 0)
    , _fsview(parent) {
}

void HeaderWidget::DoDraw(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _geometry._w;
  int header_y1 = iy1;
  int header_y2 = iy1 + _geometry._h;

  auto mtxi = tgt->MTXI();
  mtxi->PushUIMatrix();

  // Draw background
  auto rs = defmtl->_rasterstate;
  rs->setBlendingMacro(lev2::BlendingMacro::OFF);
  rs->setDepthTest(lev2::EDepthTest::OFF);
  fxi->pushRasterState(rs);
  tgt->PushModColor(_fsview->_header_bgcolor * 0.9f);
  defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
  primi->RenderQuadAtZ(defmtl.get(), ix1, ix2, header_y1, header_y2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
  tgt->PopModColor();
  fxi->popRasterState();

  // Draw column headers with sort indicators
  if (_fsview->_font) {
    lev2::FontMan::PushFont(_fsview->_font);

    auto sort_field = _fsview->_model ? _fsview->_model->getSortField() : FilesystemModel::SortField::Name;
    auto sort_order = _fsview->_model ? _fsview->_model->getSortOrder() : FilesystemModel::SortOrder::Ascending;
    const char* arrow_up = " ^";
    const char* arrow_down = " v";

    const int col_padding = 8;
    int x = ix1 + _fsview->_icon_column_width;
    int text_y = header_y1 + (_geometry._h - _fsview->_font->description().miAdvanceHeight) / 2;

    lev2::FontMan::beginTextBlock(tgt, 80);

    // Name column
    bool is_name_sorted = (sort_field == FilesystemModel::SortField::Name);
    tgt->PushModColor(is_name_sorted ? _fsview->_text_color : _fsview->_text_color * 0.7f);
    if (is_name_sorted) {
      std::string label = std::string("Name") + (sort_order == FilesystemModel::SortOrder::Ascending ? arrow_up : arrow_down);
      lev2::FontMan::DrawText(tgt, x, text_y, label.c_str());
    } else {
      lev2::FontMan::DrawText(tgt, x, text_y, "Name");
    }
    tgt->PopModColor();
    x += _fsview->_name_column_width;

    // Description column
    if (_fsview->_show_description_column) {
      tgt->PushModColor(_fsview->_text_color * 0.7f);
      lev2::FontMan::DrawText(tgt, x + col_padding, text_y, "Description");
      tgt->PopModColor();
      x += _fsview->_description_column_width;
    }

    // Options column
    if (_fsview->_show_options_column) {
      tgt->PushModColor(_fsview->_text_color * 0.7f);
      lev2::FontMan::DrawText(tgt, x + col_padding, text_y, "Options");
      tgt->PopModColor();
      x += _fsview->_options_column_width;
    }

    // Size column
    if (_fsview->_show_size_column) {
      bool is_size_sorted = (sort_field == FilesystemModel::SortField::Size);
      tgt->PushModColor(is_size_sorted ? _fsview->_text_color : _fsview->_text_color * 0.7f);
      if (is_size_sorted) {
        std::string label = std::string("Size") + (sort_order == FilesystemModel::SortOrder::Ascending ? arrow_up : arrow_down);
        lev2::FontMan::DrawText(tgt, x + col_padding, text_y, label.c_str());
      } else {
        lev2::FontMan::DrawText(tgt, x + col_padding, text_y, "Size");
      }
      tgt->PopModColor();
      x += _fsview->_size_column_width;
    }

    // Type column
    if (_fsview->_show_type_column) {
      bool is_type_sorted = (sort_field == FilesystemModel::SortField::Type);
      tgt->PushModColor(is_type_sorted ? _fsview->_text_color : _fsview->_text_color * 0.7f);
      if (is_type_sorted) {
        std::string label = std::string("Type") + (sort_order == FilesystemModel::SortOrder::Ascending ? arrow_up : arrow_down);
        lev2::FontMan::DrawText(tgt, x + col_padding, text_y, label.c_str());
      } else {
        lev2::FontMan::DrawText(tgt, x + col_padding, text_y, "Type");
      }
      tgt->PopModColor();
      x += _fsview->_type_column_width;
    }

    // Date column
    if (_fsview->_show_date_column) {
      bool is_date_sorted = (sort_field == FilesystemModel::SortField::ModifiedTime);
      tgt->PushModColor(is_date_sorted ? _fsview->_text_color : _fsview->_text_color * 0.7f);
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
    rs = defmtl->_rasterstate;
    rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    rs->setDepthTest(lev2::EDepthTest::OFF);
    fxi->pushRasterState(rs);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);

    fvec4 sep_color = fvec4(0.4f, 0.4f, 0.45f, 1.0f);
    tgt->PushModColor(sep_color);

    int sep_x = ix1 + _fsview->_icon_column_width + _fsview->_name_column_width;
    primi->RenderQuadAtZ(defmtl.get(), sep_x - 1, sep_x + 1, header_y1 + 2, header_y2 - 2,
                         0.0f, 0.0f, 1.0f, 0.0f, 1.0f);

    if (_fsview->_show_description_column) {
      sep_x += _fsview->_description_column_width;
      primi->RenderQuadAtZ(defmtl.get(), sep_x - 1, sep_x + 1, header_y1 + 2, header_y2 - 2,
                           0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    }

    if (_fsview->_show_options_column) {
      sep_x += _fsview->_options_column_width;
      primi->RenderQuadAtZ(defmtl.get(), sep_x - 1, sep_x + 1, header_y1 + 2, header_y2 - 2,
                           0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    }

    if (_fsview->_show_size_column) {
      sep_x += _fsview->_size_column_width;
      primi->RenderQuadAtZ(defmtl.get(), sep_x - 1, sep_x + 1, header_y1 + 2, header_y2 - 2,
                           0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    }

    if (_fsview->_show_type_column) {
      sep_x += _fsview->_type_column_width;
      primi->RenderQuadAtZ(defmtl.get(), sep_x - 1, sep_x + 1, header_y1 + 2, header_y2 - 2,
                           0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    }

    tgt->PopModColor();
    fxi->popRasterState();
  }

  mtxi->PopUIMatrix();
}

HandlerResult HeaderWidget::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  int localX = 0;
  int localY = 0;
  RootToLocal(ev->miX, ev->miY, localX, localY);

  switch (ev->_eventcode) {
    case EventCode::PUSH: {
      // Check column separator for resize
      int sep_col = _fsview->_getColumnSeparatorAt(localX);
      if (sep_col >= 0) {
        _fsview->_resize_column = sep_col;
        _fsview->_resize_start_x = localX;
        int* width_ptr = _fsview->_getColumnWidthPtr(sep_col);
        _fsview->_resize_start_width = width_ptr ? *width_ptr : 0;
        result.setHandled(this);
        break;
      }
      // Sort by column
      _fsview->_handleHeaderClick(localX);
      result.setHandled(this);
      break;
    }
    case EventCode::DRAG: {
      if (_fsview->_resize_column >= 0) {
        int delta = localX - _fsview->_resize_start_x;
        int new_width = std::max(40, _fsview->_resize_start_width + delta);
        int* width_ptr = _fsview->_getColumnWidthPtr(_fsview->_resize_column);
        if (width_ptr) {
          *width_ptr = new_width;
        }
        result.setHandled(this);
      }
      break;
    }
    case EventCode::RELEASE: {
      if (_fsview->_resize_column >= 0) {
        _fsview->_resize_column = -1;
        result.setHandled(this);
      }
      break;
    }
    default:
      break;
  }

  return result;
}

Widget* HeaderWidget::doRouteUiEvent(event_constptr_t ev) {
  if (IsEventInside(ev)) return this;
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// ContentWidget
///////////////////////////////////////////////////////////////////////////////

ContentWidget::ContentWidget(FilesystemView* parent)
    : Widget("content", 0, 0, 0, 0)
    , _fsview(parent) {
}

void ContentWidget::DoDraw(drawevent_constptr_t drwev) {
  _drawColoredBox(drwev, _fsview->_bgcolor);
  if (_fsview->_view_mode == FilesystemViewMode::List) {
    _fsview->_drawContentListMode(drwev);
  } else {
    _fsview->_drawContentIconMode(drwev);
  }
}

HandlerResult ContentWidget::DoOnUiEvent(event_constptr_t ev) {
  HandlerResult result;

  int localX = 0;
  int localY = 0;
  RootToLocal(ev->miX, ev->miY, localX, localY);

  // Handle keyboard input when editing
  if (_fsview->isEditing()) {
    switch (ev->_eventcode) {
      case EventCode::KEY_DOWN:
      case EventCode::KEY_REPEAT: {
        int key = ev->miKeyCode;
        switch (key) {
          case 256: // ESC
            _fsview->cancelEditing();
            break;
          case 257: // Enter
            _fsview->commitEditing();
            result._widget_finished = true;
            break;
          case 259: // Backspace
            if (_fsview->_cursor_pos > 0) {
              _fsview->_edit_value.erase(_fsview->_cursor_pos - 1, 1);
              _fsview->_cursor_pos--;
            }
            break;
          case 261: // Delete
            if (_fsview->_cursor_pos < (int)_fsview->_edit_value.length()) {
              _fsview->_edit_value.erase(_fsview->_cursor_pos, 1);
            }
            break;
          case 263: // Left
            if (_fsview->_cursor_pos > 0) _fsview->_cursor_pos--;
            break;
          case 262: // Right
            if (_fsview->_cursor_pos < (int)_fsview->_edit_value.length()) _fsview->_cursor_pos++;
            break;
          default:
            if (key >= 32 && key <= 126) {
              char ch = ev->mbSHIFT ? char(key) : std::tolower(key);
              _fsview->_edit_value.insert(_fsview->_cursor_pos, 1, ch);
              _fsview->_cursor_pos++;
            }
            break;
        }
        result.setHandled(this);
        return result;
      }
      case EventCode::PUSH: {
        std::string clicked_path = _fsview->_getItemPathAt(localX, localY);
        if (clicked_path != _fsview->_editing_path) {
          _fsview->commitEditing();
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
      // Right-click: context menu
      if (ev->IsButton2DownF()) {
        std::string clicked_path = _fsview->_getItemPathAt(localX, localY);
        if (!clicked_path.empty() && _fsview->_onContextMenu) {
          int root_x, root_y;
          LocalToRoot(localX, localY, root_x, root_y);
          _fsview->_onContextMenu(clicked_path, root_x, root_y);
          result.setHandled(this);
          break;
        }
      }

      // Item selection
      std::string clicked_path = _fsview->_getItemPathAt(localX, localY);
      if (!clicked_path.empty()) {
        bool is_already_selected = _fsview->isSelected(clicked_path);

        if (ev->mbSHIFT && _fsview->_allow_multiselect) {
          if (is_already_selected) {
            _fsview->removeFromSelection(clicked_path);
          } else {
            _fsview->addToSelection(clicked_path);
          }
        } else if (is_already_selected) {
          _fsview->removeFromSelection(clicked_path);
        } else {
          _fsview->setSelectedPath(clicked_path);
        }
        result.setHandled(this);
      }
      break;
    }

    case EventCode::DOUBLECLICK: {
      if (ev->mbSHIFT) {
        std::string clicked_path = _fsview->_getItemPathAt(localX, localY);
        if (!clicked_path.empty()) {
          _fsview->startEditing(clicked_path);
          result.setHandled(this);
        }
      } else {
        std::string clicked_path = _fsview->_getItemPathAt(localX, localY);
        if (!clicked_path.empty()) {
          _fsview->activateItem(clicked_path);
          result.setHandled(this);
        }
      }
      break;
    }

    case EventCode::KEY_DOWN: {
      int key = ev->miKeyCode;
      std::string selected = _fsview->getSelectedPath();

      if (key == 259 && !ev->mbSHIFT) {
        _fsview->navigateUp();
        result.setHandled(this);
      } else if (key == 257 && !selected.empty()) {
        _fsview->activateItem(selected);
        result.setHandled(this);
      } else if (key == 261 && ev->mbSHIFT && !_fsview->_selected_paths.empty()) {
        if (_fsview->_model && !_fsview->_model->isReadOnly()) {
          std::vector<std::string> to_delete(_fsview->_selected_paths.begin(), _fsview->_selected_paths.end());
          _fsview->clearSelection();
          for (const auto& path : to_delete) {
            _fsview->_model->deleteItem(path);
            if (_fsview->_onDelete) {
              _fsview->_onDelete(path);
            }
          }
          _fsview->_needs_rebuild = true;
        }
        result.setHandled(this);
      } else if (key == 291 && _fsview->_selected_paths.size() == 1) {
        _fsview->startEditing(*_fsview->_selected_paths.begin());
        result.setHandled(this);
      } else if (key == 265 || key == 264) { // Up/Down
        if (!_fsview->_visible_items.empty()) {
          int current_index = -1;
          if (!selected.empty()) {
            for (size_t i = 0; i < _fsview->_visible_items.size(); i++) {
              if (_fsview->_visible_items[i].entry.path == selected) {
                current_index = i;
                break;
              }
            }
          }
          int new_index = current_index;
          if (key == 265) {
            new_index = (current_index <= 0) ? _fsview->_visible_items.size() - 1 : current_index - 1;
          } else {
            new_index = (current_index < 0 || current_index >= (int)_fsview->_visible_items.size() - 1) ? 0 : current_index + 1;
          }
          _fsview->setSelectedPath(_fsview->_visible_items[new_index].entry.path);
          result.setHandled(this);
        }
      } else if (key == 'A' && ev->mbCTRL) {
        _fsview->selectAll();
        result.setHandled(this);
      } else if (key == 256) {
        _fsview->clearSelection();
        result.setHandled(this);
      }
      break;
    }

    case EventCode::MOVE: {
      std::string hovered_path = _fsview->_getItemPathAt(localX, localY);
      if (_fsview->_hovered_path != hovered_path) {
        _fsview->_hovered_path = hovered_path;
        if (_fsview->_onHover) {
          _fsview->_onHover(hovered_path);
        }
      }
      break;
    }

    case EventCode::MOUSEWHEEL: {
      _fsview->_scroll_offset -= ev->miMWY * 3;
      _fsview->_clampScrollOffset();
      result.setHandled(this);
      break;
    }

    default:
      break;
  }

  return result;
}

Widget* ContentWidget::doRouteUiEvent(event_constptr_t ev) {
  if (IsEventInside(ev)) return this;
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// FilesystemView
///////////////////////////////////////////////////////////////////////////////

FilesystemView::FilesystemView(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
  setFontSize(16);
  _model = std::make_shared<LocalFilesystemModel>();
  _subscribeToModel();

  _inner_vpack = std::make_shared<VerticalPack>(name + "_vpack");
  _inner_vpack->_margin = 1;
  _inner_vpack->_draw_background = false;
  addChild(_inner_vpack, false);

  _path_bar_widget = std::make_shared<PathBarWidget>(this);
  _path_bar_widget->_fixed_height = 28;
  _inner_vpack->addChild(_path_bar_widget, false);

  _header_widget = std::make_shared<HeaderWidget>(this);
  _header_widget->_fixed_height = 24;
  _inner_vpack->addChild(_header_widget, false);

  _bars_vpack = std::make_shared<VerticalPack>(name + "_bars");
  _bars_vpack->_margin = 1;
  _bars_vpack->_draw_background = false;
  _inner_vpack->addChild(_bars_vpack, false);

  _content_widget = std::make_shared<ContentWidget>(this);
  _inner_vpack->_fill_widget = _content_widget;
  _inner_vpack->addChild(_content_widget, false);
}

FilesystemView::~FilesystemView() {
}

void FilesystemView::setFontSize(int size) {
  _font = lev2::FontMan::fontForId(FormatString("i%d", size));
  _small_font = lev2::FontMan::fontForId(FormatString("i%d", std::max(8, size - 2)));
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
      clearIconCache();
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
  clearIconCache();
}

void FilesystemView::setViewMode(FilesystemViewMode mode) {
  if (_view_mode != mode) {
    _view_mode = mode;
    _needs_rebuild = true;
    _scroll_offset = 0;
    // Header only visible in list mode
    _header_widget->_enable = (mode == FilesystemViewMode::List) && _draw_header;
    // Force relayout of inner vpack via SetRect (public API triggers DoLayout)
    _inner_vpack->SetRect(0, 0, _geometry._w, _geometry._h);
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
  if (_selected_paths.empty()) return "";
  return *_selected_paths.begin();
}

void FilesystemView::addToSelection(const std::string& path) {
  if (!path.empty() && _allow_multiselect) {
    _selected_paths.insert(path);
    if (_onSelect) _onSelect(path);
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
  if (_model) return _model->setCurrentPath(path);
  return false;
}

bool FilesystemView::navigateUp() {
  if (_model) {
    std::string parent = _model->getParentPath();
    if (!parent.empty()) return _model->setCurrentPath(parent);
  }
  return false;
}

void FilesystemView::activateItem(const std::string& path) {
  if (!_model) return;
  if (_model->isDirectory(path)) {
    navigateTo(path);
  } else {
    if (_onActivate) _onActivate(path);
  }
}

void FilesystemView::refresh() {
  _needs_rebuild = true;
  _thumbnail_cache.clear();
  // Force full relayout of VPack hierarchy.
  // Dirty both vpacks so SetRect triggers DoLayout even at same size.
  _inner_vpack->_geometry._w = 0;
  _bars_vpack->_geometry._w = 0;
  _inner_vpack->SetRect(0, 0, _geometry._w, _geometry._h);
}

toolbar_ptr_t FilesystemView::addToolbar(const std::string& name, int height) {
  auto toolbar = std::make_shared<Toolbar>(name);
  toolbar->_fixed_height = height;
  _bars_vpack->addChild(toolbar, false);
  // Re-layout inner vpack so it accounts for the new bar height
  _inner_vpack->_geometry._w = 0;
  _inner_vpack->SetRect(0, 0, _geometry._w, _geometry._h);
  return toolbar;
}

void FilesystemView::removeToolbar(toolbar_ptr_t toolbar) {
  _bars_vpack->removeChild(toolbar, false);
  _inner_vpack->_geometry._w = 0;
  _inner_vpack->SetRect(0, 0, _geometry._w, _geometry._h);
}

void FilesystemView::setFolderIcon(lev2::image_ptr_t img) {
  _folder_icon_image = img;
  _folder_icon_texture = nullptr;
}

void FilesystemView::setFileIcon(lev2::image_ptr_t img) {
  _file_icon_image = img;
  _file_icon_texture = nullptr;
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
  favorites->removeFavorite(_model->modelIdentifier(), _model->getCurrentPath());
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
// Inline editing
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
        _selected_paths.erase(old_path);
        _selected_paths.insert(new_path);
        if (_onRename) _onRename(old_path, new_name);
      }
    }
  }
  cancelEditing();
  _needs_rebuild = true;
}

///////////////////////////////////////////////////////////////////////////////
// Layout & Drawing
///////////////////////////////////////////////////////////////////////////////

void FilesystemView::_doOnResized() {
  int content_width = _geometry._w - _icon_column_width;
  if (_last_content_width > 0 && content_width > 0 && content_width != _last_content_width) {
    float scale = (float)content_width / (float)_last_content_width;
    _name_column_width = std::max(60, (int)(_name_column_width * scale));
    _description_column_width = std::max(80, (int)(_description_column_width * scale));
    _options_column_width = std::max(60, (int)(_options_column_width * scale));
    _size_column_width = std::max(50, (int)(_size_column_width * scale));
    _type_column_width = std::max(50, (int)(_type_column_width * scale));
    _date_column_width = std::max(80, (int)(_date_column_width * scale));
  }
  _last_content_width = content_width;
  DoLayout();
}

void FilesystemView::DoLayout() {
  // Sync enable state
  _path_bar_widget->_enable = _draw_path_bar;
  _header_widget->_enable = _draw_header && (_view_mode == FilesystemViewMode::List);

  _inner_vpack->SetRect(0, 0, _geometry._w, _geometry._h);
  _rebuildVisibleItems();
}

void FilesystemView::_doGpuInit(lev2::Context* ctx) {
  _loadDefaultIcons(ctx);
}

void FilesystemView::DoDraw(drawevent_constptr_t drwev) {
  if (_needs_rebuild) {
    _rebuildVisibleItems();
  }

  auto fbi = drwev->GetTarget()->FBI();
  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  fbi->pushScissor(lev2::ViewportRect(ix1, iy1, _geometry._w, _geometry._h));

  if (_draw_background) {
    _drawColoredBox(drwev, _bgcolor);
  }

  drawChildren(drwev);

  fbi->popScissor();
}

Widget* FilesystemView::doRouteUiEvent(event_constptr_t ev) {
  return Group::doRouteUiEvent(ev);
}

HandlerResult FilesystemView::DoOnUiEvent(event_constptr_t ev) {
  // Group-level events not handled by children
  HandlerResult result;
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// Rebuild & scroll
///////////////////////////////////////////////////////////////////////////////

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
    int available_width = _geometry._w - _icon_spacing * 2;
    int cell_width = _icon_size + _icon_spacing;
    int cols = std::max(1, available_width / cell_width);
    int rows = (_visible_items.size() + cols - 1) / cols;
    content_height = rows * (_icon_size + _icon_label_height + _icon_spacing);
  }

  int visible_height = _content_widget ? _content_widget->height() : _geometry._h;
  int max_scroll = std::max(0, content_height - visible_height);
  _scroll_offset = std::clamp(_scroll_offset, 0, max_scroll);
}

int FilesystemView::_getItemIndexAt(int local_x, int local_y) const {
  // local_x/local_y are in ContentWidget local coords
  int content_y = local_y + _scroll_offset;
  if (content_y < 0) return -1;

  if (_view_mode == FilesystemViewMode::List) {
    int index = content_y / _item_height;
    if (index >= 0 && index < (int)_visible_items.size()) return index;
  } else {
    int available_width = _geometry._w - _icon_spacing * 2;
    int cell_width = _icon_size + _icon_spacing;
    int cell_height = _icon_size + _icon_label_height + _icon_spacing;
    int cols = std::max(1, available_width / cell_width);
    int rows = ((int)_visible_items.size() + cols - 1) / cols;
    int items_in_first_row = std::min((int)_visible_items.size(), cols);
    int grid_width = items_in_first_row * cell_width - _icon_spacing;
    int grid_height = rows * cell_height;
    int x_offset = _icon_center_h ? std::max(0, (_geometry._w - grid_width) / 2) : _icon_spacing;
    int y_offset = _icon_center_v ? std::max(0, ((_content_widget ? _content_widget->height() : _geometry._h) - grid_height) / 2) : 0;
    int col = (local_x - x_offset) / cell_width;
    int row = (content_y - y_offset) / cell_height;
    if (col >= 0 && col < cols && row >= 0) {
      int index = row * cols + col;
      if (index >= 0 && index < (int)_visible_items.size()) return index;
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

///////////////////////////////////////////////////////////////////////////////
// Column helpers (shared by HeaderWidget and ContentWidget)
///////////////////////////////////////////////////////////////////////////////

FilesystemModel::SortField FilesystemView::_getSortFieldAtX(int local_x) const {
  int x = _icon_column_width;

  if (local_x < x + _name_column_width) return FilesystemModel::SortField::Name;
  x += _name_column_width;

  if (_show_description_column) {
    if (local_x < x + _description_column_width) return FilesystemModel::SortField::Name;
    x += _description_column_width;
  }

  if (_show_options_column) {
    if (local_x < x + _options_column_width) return FilesystemModel::SortField::Name;
    x += _options_column_width;
  }

  if (_show_size_column) {
    if (local_x < x + _size_column_width) return FilesystemModel::SortField::Size;
    x += _size_column_width;
  }

  if (_show_type_column) {
    if (local_x < x + _type_column_width) return FilesystemModel::SortField::Type;
    x += _type_column_width;
  }

  if (_show_date_column) return FilesystemModel::SortField::ModifiedTime;

  return FilesystemModel::SortField::Name;
}

void FilesystemView::_handleHeaderClick(int local_x) {
  if (!_model) return;

  auto clicked_field = _getSortFieldAtX(local_x);
  auto current_field = _model->getSortField();
  auto current_order = _model->getSortOrder();

  if (clicked_field == current_field) {
    auto new_order = (current_order == FilesystemModel::SortOrder::Ascending)
                       ? FilesystemModel::SortOrder::Descending
                       : FilesystemModel::SortOrder::Ascending;
    _model->setSortOrder(new_order);
  } else {
    _model->setSortField(clicked_field);
    _model->setSortOrder(FilesystemModel::SortOrder::Ascending);
  }

  _needs_rebuild = true;
}

int FilesystemView::_getColumnSeparatorAt(int local_x) const {
  int x = _icon_column_width;
  int col_idx = 0;

  int sep_x = x + _name_column_width;
  if (local_x >= sep_x - _resize_grip_width && local_x <= sep_x + _resize_grip_width) return col_idx;
  x = sep_x;
  col_idx++;

  if (_show_description_column) {
    sep_x = x + _description_column_width;
    if (local_x >= sep_x - _resize_grip_width && local_x <= sep_x + _resize_grip_width) return col_idx;
    x = sep_x;
    col_idx++;
  }

  if (_show_options_column) {
    sep_x = x + _options_column_width;
    if (local_x >= sep_x - _resize_grip_width && local_x <= sep_x + _resize_grip_width) return col_idx;
    x = sep_x;
    col_idx++;
  }

  if (_show_size_column) {
    sep_x = x + _size_column_width;
    if (local_x >= sep_x - _resize_grip_width && local_x <= sep_x + _resize_grip_width) return col_idx;
    x = sep_x;
    col_idx++;
  }

  if (_show_type_column) {
    sep_x = x + _type_column_width;
    if (local_x >= sep_x - _resize_grip_width && local_x <= sep_x + _resize_grip_width) return col_idx;
    x = sep_x;
    col_idx++;
  }

  return -1;
}

int* FilesystemView::_getColumnWidthPtr(int column_index) {
  int idx = 0;
  if (column_index == idx) return &_name_column_width;
  idx++;
  if (_show_description_column) {
    if (column_index == idx) return &_description_column_width;
    idx++;
  }
  if (_show_options_column) {
    if (column_index == idx) return &_options_column_width;
    idx++;
  }
  if (_show_size_column) {
    if (column_index == idx) return &_size_column_width;
    idx++;
  }
  if (_show_type_column) {
    if (column_index == idx) return &_type_column_width;
    idx++;
  }
  if (column_index == idx) return &_date_column_width;
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// Content drawing (called from ContentWidget::DoDraw)
///////////////////////////////////////////////////////////////////////////////

void FilesystemView::_drawContentListMode(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();
  auto fbi = tgt->FBI();

  int ix1, iy1;
  _content_widget->LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _content_widget->width();
  int iy2 = iy1 + _content_widget->height();

  fbi->pushScissor(lev2::ViewportRect(ix1, iy1, _content_widget->width(), _content_widget->height()));

  mtxi->PushUIMatrix();
  {
    // Draw items
    int content_y = -_scroll_offset;
    for (size_t i = 0; i < _visible_items.size(); i++) {
      int item_y = iy1 + content_y + i * _item_height;

      if (item_y + _item_height < iy1) continue;
      if (item_y > iy2) break;

      bool selected = isSelected(_visible_items[i].entry.path);
      bool hovered = (_visible_items[i].entry.path == _hovered_path);

      _drawListItem(drwev, _visible_items[i], item_y, selected, hovered, i);
    }

    // Draw column separator lines
    {
      auto rs = defmtl->_rasterstate;
      rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
      rs->setDepthTest(lev2::EDepthTest::OFF);
      fxi->pushRasterState(rs);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);

      fvec4 sep_color = fvec4(0.25f, 0.25f, 0.28f, 1.0f);
      tgt->PushModColor(sep_color);

      int sep_x = ix1 + _icon_column_width + _name_column_width;
      primi->RenderQuadAtZ(defmtl.get(), sep_x, sep_x + 1, iy1, iy2,
                           0.0f, 0.0f, 1.0f, 0.0f, 1.0f);

      if (_show_description_column) {
        sep_x += _description_column_width;
        primi->RenderQuadAtZ(defmtl.get(), sep_x, sep_x + 1, iy1, iy2,
                             0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      }

      if (_show_options_column) {
        sep_x += _options_column_width;
        primi->RenderQuadAtZ(defmtl.get(), sep_x, sep_x + 1, iy1, iy2,
                             0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      }

      if (_show_size_column) {
        sep_x += _size_column_width;
        primi->RenderQuadAtZ(defmtl.get(), sep_x, sep_x + 1, iy1, iy2,
                             0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      }

      if (_show_type_column) {
        sep_x += _type_column_width;
        primi->RenderQuadAtZ(defmtl.get(), sep_x, sep_x + 1, iy1, iy2,
                             0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      }

      tgt->PopModColor();
      fxi->popRasterState();
    }
  }
  mtxi->PopUIMatrix();

  fbi->popScissor();
}

void FilesystemView::_drawContentIconMode(drawevent_constptr_t drwev) {
  auto tgt = drwev->GetTarget();
  auto mtxi = tgt->MTXI();
  auto fbi = tgt->FBI();

  int ix1, iy1;
  _content_widget->LocalToRoot(0, 0, ix1, iy1);
  int iy2 = iy1 + _content_widget->height();

  fbi->pushScissor(lev2::ViewportRect(ix1, iy1, _content_widget->width(), _content_widget->height()));

  mtxi->PushUIMatrix();
  {
    int available_width = _geometry._w - _icon_spacing * 2;
    int cell_width = _icon_size + _icon_spacing;
    int cell_height = _icon_size + _icon_label_height + _icon_spacing;
    int cols = std::max(1, available_width / cell_width);
    int rows = ((int)_visible_items.size() + cols - 1) / cols;

    // Centering offsets
    int items_in_first_row = std::min((int)_visible_items.size(), cols);
    int grid_width = items_in_first_row * cell_width - _icon_spacing;
    int grid_height = rows * cell_height;
    int x_offset = _icon_center_h ? std::max(0, (_geometry._w - grid_width) / 2) : _icon_spacing;
    int y_offset = _icon_center_v ? std::max(0, (_content_widget->height() - grid_height) / 2) : 0;

    for (size_t i = 0; i < _visible_items.size(); i++) {
      int col = i % cols;
      int row = i / cols;

      int item_x = ix1 + x_offset + col * cell_width;
      int item_y = iy1 + y_offset - _scroll_offset + row * cell_height;

      if (item_y + cell_height < iy1) continue;
      if (item_y > iy2) break;

      bool selected = isSelected(_visible_items[i].entry.path);
      bool hovered = (_visible_items[i].entry.path == _hovered_path);

      _drawIconItem(drwev, _visible_items[i], item_x, item_y, selected, hovered);
    }
  }
  mtxi->PopUIMatrix();

  fbi->popScissor();
}

///////////////////////////////////////////////////////////////////////////////
// Item drawing (unchanged logic, now uses ContentWidget's absolute coords)
///////////////////////////////////////////////////////////////////////////////

void FilesystemView::_drawListItem(drawevent_constptr_t drwev, const VisibleItem& item,
                                    int y_pos, bool selected, bool hovered, int row_index) {
  auto tgt = drwev->GetTarget();
  auto fxi = tgt->FXI();
  auto primi = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int ix1, iy1;
  _content_widget->LocalToRoot(0, 0, ix1, iy1);
  int ix2 = ix1 + _content_widget->width();

  // Draw alternating row background
  if (!selected && !hovered && (row_index % 2 == 1)) {
    fvec4 alt_bg_color = _bgcolor * 1.15f;
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
    auto icon_texture = _getIconForPath(tgt, item.entry.path, item.entry.type, icon_size);

    if (icon_texture) {
      if (!_tex_material) {
        _tex_material = std::make_shared<lev2::GfxMaterialUITextured>(tgt, "uitextured");
      }
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
      bool is_directory = (item.entry.type == FileType::Directory);
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
    if (_show_description_column) char_count += item.entry.description.length() + 8;
    if (_show_options_column) char_count += item.entry.options.length() + 8;
    if (_show_size_column) char_count += 16;
    if (_show_type_column) char_count += item.entry.extension.length() + 8;
    if (_show_date_column) char_count += 24;

    lev2::FontMan::beginTextBlock(tgt, char_count);

    const int col_padding = 8;

    // Name
    std::string name_str = _truncateToWidth(item.entry.name, _name_column_width);
    lev2::FontMan::DrawText(tgt, x, text_y, name_str.c_str());
    x += _name_column_width;

    // Description
    if (_show_description_column) {
      if (!item.entry.description.empty()) {
        std::string desc_str = _truncateToWidth(item.entry.description, _description_column_width - col_padding);
        lev2::FontMan::DrawText(tgt, x + col_padding, text_y, desc_str.c_str());
      }
      x += _description_column_width;
    }

    // Options text
    if (_show_options_column) {
      if (!item.entry.options.empty()) {
        std::string opts_str = _truncateToWidth(item.entry.options, _options_column_width - col_padding);
        lev2::FontMan::DrawText(tgt, x + col_padding, text_y, opts_str.c_str());
      }
      x += _options_column_width;
    }

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
      if (!_tex_material) {
        _tex_material = std::make_shared<lev2::GfxMaterialUITextured>(tgt, "uitextured");
      }
      auto rs = _tex_material->_rasterstate;
      rs->setBlendingMacro(lev2::BlendingMacro::ALPHA);
      rs->setDepthTest(lev2::EDepthTest::OFF);
      rs->_priority = 1;
      fxi->pushRasterState(rs);
      tgt->PushModColor(fvec4(1, 1, 1, 1));
      _tex_material->SetTexture(lev2::ETEXDEST_DIFFUSE, icon_texture.get());

      // Respect texture aspect ratio: tall textures extend below the icon cell
      int icon_w = _icon_size - 8;
      int icon_h = icon_w;
      int tex_w = icon_texture->_width;
      int tex_h = icon_texture->_height;
      if (tex_w > 0 && tex_h > tex_w) {
        icon_h = icon_w * tex_h / tex_w;
      }

      primi->RenderQuadAtZ(_tex_material.get(),
                           x_pos + 4, x_pos + 4 + icon_w,
                           y_pos + 4, y_pos + 4 + icon_h,
                           0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
      tgt->PopModColor();
      fxi->popRasterState();
    } else {
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
  if (_small_font && _icon_label_enable) {
    fvec4 text_col = (item.entry.type == FileType::Directory) ? _directory_color : _text_color;
    lev2::FontMan::PushFont(_small_font);
    tgt->PushModColor(text_col);

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

///////////////////////////////////////////////////////////////////////////////
// Formatting helpers
///////////////////////////////////////////////////////////////////////////////

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
  int max_chars = (max_width - 8) / char_width;
  if (max_chars <= 0) return "";
  if ((int)text.length() <= max_chars) return text;
  if (max_chars <= 3) return text.substr(0, max_chars);
  return text.substr(0, max_chars - 3) + "...";
}

///////////////////////////////////////////////////////////////////////////////
// Icon/thumbnail management
///////////////////////////////////////////////////////////////////////////////

lev2::texture_ptr_t FilesystemView::_getDefaultIcon(FileType type, const std::string& extension) {
  return nullptr;
}

void FilesystemView::_loadDefaultIcons(lev2::Context* ctx) {
  if (_icons_loaded) return;
  _icons_loaded = true;
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
  auto image = provider->_func();
  if (!image) {
    item.thumbnail_failed = true;
  }
}

lev2::texture_ptr_t FilesystemView::_getIconForPath(lev2::Context* ctx, const std::string& path, FileType type, int size) {
  auto txi = ctx->TXI();

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

  auto it = _icon_cache.find(path);
  if (it != _icon_cache.end()) {
    // Check if this path needs an in-place update (e.g. hover icon change)
    auto pending_it = _icon_update_pending.find(path);
    if (pending_it != _icon_update_pending.end()) {
      _icon_update_pending.erase(pending_it);
      auto& frames = it->second;
      if (!frames.empty()) {
        lev2::image_ptr_t image = nullptr;
        auto provider = _model->getIconProvider(path, size);
        if (provider) image = provider->_func();
        if (!image) image = _model->getIcon(path, size);
        if (image) {
          txi->initTextureFromImage(frames[0].get(), image, true);
          return frames[0];
        }
      }
    }
    auto& frames = it->second;
    if (frames.size() == 1) return frames[0];
    if (frames.size() > 1) {
      float t = _uicontext ? _uicontext->_uitimer.SecsSinceStart() : 0.0f;
      int frame = int(t * _icon_anim_fps) % int(frames.size());
      return frames[frame];
    }
  }

  auto sequence = _model->getIconSequence(path, size);
  if (!sequence.empty()) {
    lev2::texture_list_t frames;
    for (auto& img : sequence) {
      auto texture = std::make_shared<lev2::Texture>();
      txi->initTextureFromImage(texture.get(), img, true);
      frames.push_back(texture);
    }
    _icon_cache[path] = frames;
    return frames[0];
  }

  lev2::image_ptr_t image = nullptr;
  auto provider = _model->getIconProvider(path, size);
  if (provider) image = provider->_func();
  if (!image) image = _model->getIcon(path, size);

  if (image) {
    auto texture = std::make_shared<lev2::Texture>();
    txi->initTextureFromImage(texture.get(), image, true);
    _icon_cache[path] = lev2::texture_list_t{texture};
    return texture;
  }

  return (type == FileType::Directory) ? _folder_icon_texture : _file_icon_texture;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
