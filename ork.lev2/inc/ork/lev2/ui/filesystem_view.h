////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/filesystem_model.h>
#include <ork/lev2/ui/favorites.h>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// FilesystemView: Widget for displaying filesystem contents
// - Supports list and icon view modes
// - Model-based data access
// - Selection, navigation, thumbnails
////////////////////////////////////////////////////////////////////

enum class FilesystemViewMode {
  List,   // Detailed list with columns
  Icon    // Grid of icons/thumbnails
};

struct FilesystemView;
using filesystem_view_ptr_t = std::shared_ptr<FilesystemView>;

struct FilesystemView : public Widget {
  FilesystemView(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~FilesystemView();

  //////////////////////////////////////////////////////////////
  // Model
  //////////////////////////////////////////////////////////////

  void setModel(filesystem_model_ptr_t model);
  filesystem_model_ptr_t getModel() const { return _model; }

  //////////////////////////////////////////////////////////////
  // View mode
  //////////////////////////////////////////////////////////////

  void setViewMode(FilesystemViewMode mode);
  FilesystemViewMode getViewMode() const { return _view_mode; }

  //////////////////////////////////////////////////////////////
  // Selection
  //////////////////////////////////////////////////////////////

  void setSelectedPath(const std::string& path);
  std::string getSelectedPath() const;
  void addToSelection(const std::string& path);
  void removeFromSelection(const std::string& path);
  void clearSelection();
  void selectAll();
  const std::unordered_set<std::string>& getSelectedPaths() const { return _selected_paths; }
  bool isSelected(const std::string& path) const { return _selected_paths.count(path) > 0; }

  // Multi-select support
  void setAllowMultiSelect(bool allow) { _allow_multiselect = allow; }
  bool getAllowMultiSelect() const { return _allow_multiselect; }

  //////////////////////////////////////////////////////////////
  // Navigation
  //////////////////////////////////////////////////////////////

  // Navigate to a path (directory)
  bool navigateTo(const std::string& path);

  // Go up one directory
  bool navigateUp();

  // Activate an item (open file or enter directory)
  void activateItem(const std::string& path);

  // Refresh current view
  void refresh();

  //////////////////////////////////////////////////////////////
  // Favorites management
  //////////////////////////////////////////////////////////////

  // Add current location + view state as a favorite
  void addCurrentAsFavorite(const std::string& display_name = "");

  // Remove current path from favorites
  void removeCurrentFromFavorites();

  // Check if current path is a favorite
  bool isCurrentFavorite() const;

  // Apply a favorite entry (navigate and restore view state)
  void applyFavorite(favorite_entry_ptr_t entry);

  // Get current state as a FavoriteEntry (without adding to favorites)
  favorite_entry_ptr_t getCurrentAsFavoriteEntry(const std::string& display_name = "") const;

  //////////////////////////////////////////////////////////////
  // Inline editing (rename)
  //////////////////////////////////////////////////////////////

  void startEditing(const std::string& path);
  void cancelEditing();
  void commitEditing();
  bool isEditing() const { return !_editing_path.empty(); }

  //////////////////////////////////////////////////////////////
  // Callbacks
  //////////////////////////////////////////////////////////////

  std::function<void(const std::string& path)> _onSelect;
  std::function<void(const std::string& path)> _onActivate;  // Double-click / Enter
  std::function<void(const std::string& path)> _onDirectoryChanged;
  std::function<void(const std::string& path)> _onDelete;
  std::function<void(const std::string& old_path, const std::string& new_name)> _onRename;

  //////////////////////////////////////////////////////////////
  // Appearance - List mode
  //////////////////////////////////////////////////////////////

  int _item_height = 20;          // Row height in list mode
  int _icon_column_width = 24;    // Width of icon column
  int _name_column_width = 200;   // Width of name column
  int _size_column_width = 80;    // Width of size column
  int _type_column_width = 100;   // Width of type column
  int _date_column_width = 120;   // Width of date column
  int _last_content_width = 0;    // For proportional column resizing
  bool _show_size_column = true;
  bool _show_type_column = true;
  bool _show_date_column = true;

  //////////////////////////////////////////////////////////////
  // Appearance - Icon mode
  //////////////////////////////////////////////////////////////

  int _icon_size = 64;            // Icon/thumbnail size
  int _icon_spacing = 8;          // Space between icons
  int _icon_label_height = 32;    // Height for label under icon

  //////////////////////////////////////////////////////////////
  // Appearance - Common
  //////////////////////////////////////////////////////////////

  fvec4 _bgcolor = fvec4(0.1f, 0.1f, 0.1f, 1.0f);
  fvec4 _text_color = fvec4(1.0f, 1.0f, 1.0f, 1.0f);
  fvec4 _selected_color = fvec4(0.3f, 0.5f, 0.8f, 1.0f);
  fvec4 _hover_color = fvec4(0.2f, 0.3f, 0.4f, 1.0f);
  fvec4 _directory_color = fvec4(0.8f, 0.9f, 1.0f, 1.0f);  // Tint for directories
  fvec4 _header_bgcolor = fvec4(0.15f, 0.15f, 0.18f, 1.0f);
  bool _draw_background = true;
  bool _draw_header = true;       // Column headers in list mode
  bool _draw_path_bar = true;     // Current path breadcrumb
  lev2::font_ptr_t _font;
  lev2::font_ptr_t _small_font;   // For icon labels

  // Default icons (as images - converted to textures lazily)
  lev2::image_ptr_t _folder_icon_image;
  lev2::image_ptr_t _file_icon_image;
  lev2::texture_ptr_t _folder_icon_texture;  // Cached texture from image
  lev2::texture_ptr_t _file_icon_texture;    // Cached texture from image

  void setFolderIcon(lev2::image_ptr_t img);
  void setFileIcon(lev2::image_ptr_t img);
  lev2::image_ptr_t getFolderIcon() const { return _folder_icon_image; }
  lev2::image_ptr_t getFileIcon() const { return _file_icon_image; }

  // Icon cache: path -> texture (populated from model's getIcon/getIconProvider)
  std::unordered_map<std::string, lev2::texture_ptr_t> _icon_cache;
  void _updateIconCache(lev2::Context* ctx, const std::string& path, int size);
  lev2::texture_ptr_t _getIconForPath(lev2::Context* ctx, const std::string& path, FileType type, int size);
  void clearIconCache() { _icon_cache.clear(); _folder_icon_texture = nullptr; _file_icon_texture = nullptr; }

protected:
  // Override from Widget
  void DoDraw(drawevent_constptr_t drwev) override;
  void DoLayout() override;
  void _doOnResized() override;
  void _doGpuInit(lev2::Context* ctx) override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

private:
  // Internal item representation
  struct VisibleItem {
    FilesystemEntry entry;
    int index;                    // Index in entries list
    lev2::texture_ptr_t thumbnail;
    bool thumbnail_requested = false;
    bool thumbnail_failed = false;
  };

  void _rebuildVisibleItems();
  void _clampScrollOffset();
  int _getItemIndexAt(int local_x, int local_y) const;
  std::string _getItemPathAt(int local_x, int local_y) const;
  bool _isInHeaderArea(int local_y) const;
  FilesystemModel::SortField _getSortFieldAtX(int local_x) const;
  void _handleHeaderClick(int local_x);
  void _subscribeToModel();
  void _requestThumbnail(VisibleItem& item);

  // Drawing helpers
  void _drawListMode(drawevent_constptr_t drwev);
  void _drawIconMode(drawevent_constptr_t drwev);
  void _drawPathBar(drawevent_constptr_t drwev, int& y_offset);
  void _drawHeader(drawevent_constptr_t drwev, int& y_offset);
  void _drawListItem(drawevent_constptr_t drwev, const VisibleItem& item, int y_pos, bool selected, bool hovered, int row_index);
  void _drawIconItem(drawevent_constptr_t drwev, const VisibleItem& item, int x_pos, int y_pos, bool selected, bool hovered);

  // Formatting helpers
  static std::string _formatSize(size_t bytes);
  static std::string _formatDate(time_t time);
  std::string _truncateToWidth(const std::string& text, int max_width) const;

  // Icon/thumbnail management
  lev2::texture_ptr_t _getDefaultIcon(FileType type, const std::string& extension);
  void _loadDefaultIcons(lev2::Context* ctx);

  filesystem_model_ptr_t _model;
  FilesystemViewMode _view_mode = FilesystemViewMode::List;
  std::vector<VisibleItem> _visible_items;
  std::unordered_set<std::string> _selected_paths;
  std::string _hovered_path;
  bool _needs_rebuild = true;
  int _scroll_offset = 0;
  bool _allow_multiselect = true;

  // Inline editing state
  std::string _editing_path;
  std::string _edit_value;
  std::string _original_value;
  int _cursor_pos = 0;

  // Header click tracking (for sorting)
  int _header_height = 24;
  int _path_bar_height = 28;

  // Column resize state
  int _resize_column = -1;        // Which column is being resized (-1 = none)
  int _resize_start_x = 0;        // Mouse X when resize started
  int _resize_start_width = 0;    // Column width when resize started
  static constexpr int _resize_grip_width = 6;  // Pixels on each side of separator

  int _getColumnSeparatorAt(int local_x, int local_y) const;  // Returns column index or -1
  int* _getColumnWidthPtr(int column_index);  // Get pointer to column width variable

  // Default icons
  lev2::texture_ptr_t _icon_file;
  lev2::texture_ptr_t _icon_directory;
  lev2::texture_ptr_t _icon_image;
  lev2::texture_ptr_t _icon_unknown;
  bool _icons_loaded = false;

  // Thumbnail cache
  std::unordered_map<std::string, lev2::texture_ptr_t> _thumbnail_cache;

  // Textured material for icon rendering
  lev2::uitexmaterial_ptr_t _tex_material;
};

} // namespace ork::ui
