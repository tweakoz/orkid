////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/lev2_types.h>
#include <ork/lev2/gfx/image.h>
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <ctime>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// FileType enumeration
////////////////////////////////////////////////////////////////////

enum class FileType {
  Unknown,
  File,
  Directory,
  Symlink
};

////////////////////////////////////////////////////////////////////
// Per-item option types for inline micro-renderers
////////////////////////////////////////////////////////////////////

enum class OptionWidgetType {
  Checkbox,   // Toggle boolean
  Dropdown,   // Cycle through choices
  Button,     // Clickable action
  Label       // Read-only text
};

// Combines definition + current value for one option on one item
struct ItemOptionDef {
  std::string name;
  OptionWidgetType type = OptionWidgetType::Label;
  bool bool_val = false;
  std::string string_val;
  std::vector<std::string> choices;  // For Dropdown type
};

////////////////////////////////////////////////////////////////////
// FilesystemEntry: Metadata for a filesystem item
////////////////////////////////////////////////////////////////////

struct FilesystemEntry {
  std::string path;           // Full path
  std::string name;           // Display name (filename only)
  FileType type = FileType::Unknown;
  size_t size = 0;            // File size in bytes
  time_t modified_time = 0;   // Last modification time
  std::string mime_type;      // MIME type (e.g., "image/png")
  std::string extension;      // File extension (lowercase, no dot)
  std::string description;    // Optional description/detail text
  std::string options;        // Optional options summary text
  bool is_hidden = false;     // Hidden file (starts with . on Unix)
  bool is_readable = true;
  bool is_writable = true;
};

using filesystem_entry_ptr_t = std::shared_ptr<FilesystemEntry>;
using filesystem_entry_list_t = std::vector<FilesystemEntry>;

////////////////////////////////////////////////////////////////////
// FilesystemModel: Abstract base class for filesystem models
// - Can be subclassed in C++ or Python
// - Provides data access and manipulation interface
// - Supports filtering and thumbnail generation
////////////////////////////////////////////////////////////////////

struct FilesystemModel;
using filesystem_model_ptr_t = std::shared_ptr<FilesystemModel>;

struct FilesystemModel {
  FilesystemModel() = default;
  virtual ~FilesystemModel() = default;

  //////////////////////////////////////////////////////////////
  // Model identification (for favorites namespacing)
  //////////////////////////////////////////////////////////////

  // Returns a unique identifier for this model type (e.g., "local", "s3", "ftp")
  // Used to namespace favorites and other persistent settings
  virtual std::string modelIdentifier() const = 0;

  //////////////////////////////////////////////////////////////
  // Navigation - override these in subclasses
  //////////////////////////////////////////////////////////////

  // Get the current directory path
  virtual std::string getCurrentPath() const = 0;

  // Set the current directory (navigate)
  virtual bool setCurrentPath(const std::string& path) = 0;

  // Get parent directory path (empty if at root)
  virtual std::string getParentPath() const = 0;

  // Check if path exists
  virtual bool exists(const std::string& path) const = 0;

  // Check if path is a directory
  virtual bool isDirectory(const std::string& path) const = 0;

  //////////////////////////////////////////////////////////////
  // Enumeration - override these in subclasses
  //////////////////////////////////////////////////////////////

  // Get entries in current directory (respects filter)
  virtual filesystem_entry_list_t getEntries() const = 0;

  // Get entry metadata for a specific path
  virtual FilesystemEntry getEntry(const std::string& path) const = 0;

  // Get display name for an entry
  virtual std::string getDisplayName(const std::string& path) const = 0;

  //////////////////////////////////////////////////////////////
  // Filtering
  //////////////////////////////////////////////////////////////

  // Set filename filter pattern
  // Supports:
  //   *.ext        - match extension
  //   name*        - match name prefix
  //   *name        - match name suffix
  //   *name*       - match name contains
  //   name         - exact name match
  //   *.ext1;*.ext2 - multiple patterns (OR)
  void setFilter(const std::string& pattern) {
    _filter_pattern = pattern;
    _parseFilterPattern();
    notifyModelChanged();
  }
  std::string getFilter() const { return _filter_pattern; }

  // Set name filter (glob pattern for filename, separate from extension)
  void setNameFilter(const std::string& pattern) {
    _name_filter = pattern;
    notifyModelChanged();
  }
  std::string getNameFilter() const { return _name_filter; }

  // Show/hide hidden files
  void setShowHidden(bool show) {
    _show_hidden = show;
    notifyModelChanged();
  }
  bool getShowHidden() const { return _show_hidden; }

  // Show/hide directories
  void setShowDirectories(bool show) {
    _show_directories = show;
    notifyModelChanged();
  }
  bool getShowDirectories() const { return _show_directories; }

  //////////////////////////////////////////////////////////////
  // Operations (for read/write models)
  //////////////////////////////////////////////////////////////

  // Check if model is read-only
  virtual bool isReadOnly() const { return true; }

  // Create a new directory
  virtual bool createDirectory(const std::string& name) { return false; }

  // Delete an item
  virtual bool deleteItem(const std::string& path) { return false; }

  // Rename an item - returns new path on success, empty on failure
  virtual std::string renameItem(const std::string& path, const std::string& new_name) { return ""; }

  // Copy an item
  virtual bool copyItem(const std::string& src_path, const std::string& dest_path) { return false; }

  // Move an item
  virtual bool moveItem(const std::string& src_path, const std::string& dest_path) { return false; }

  //////////////////////////////////////////////////////////////
  // Thumbnails
  //////////////////////////////////////////////////////////////

  // Get icon for a path (returns nullptr to use view's default)
  // Override to provide custom icons per file type or per item
  virtual lev2::image_ptr_t getIcon(const std::string& path, int size) {
    return nullptr;
  }

  // Get icon provider for lazy loading (returns nullptr to use getIcon or view's default)
  virtual lev2::image_provider_ptr_t getIconProvider(const std::string& path, int size) {
    return nullptr;
  }

  // Get thumbnail provider for a path (returns nullptr if not available)
  virtual lev2::image_provider_ptr_t getThumbnailProvider(const std::string& path, int size) {
    return nullptr;
  }

  // Check if thumbnails are supported for this path
  virtual bool hasThumbnail(const std::string& path) const { return false; }

  //////////////////////////////////////////////////////////////
  // Per-item options (inline micro-renderers)
  //////////////////////////////////////////////////////////////

  // Get option definitions + values for a specific item (empty = no options)
  virtual std::vector<ItemOptionDef> getItemOptions(const std::string& path) const { return {}; }

  // Set an option value by name (returns true if changed)
  virtual bool setItemOption(const std::string& path, const std::string& option_name, const ItemOptionDef& value) { return false; }

  //////////////////////////////////////////////////////////////
  // Sorting
  //////////////////////////////////////////////////////////////

  enum class SortField {
    Name,
    Size,
    Type,
    ModifiedTime
  };

  enum class SortOrder {
    Ascending,
    Descending
  };

  void setSortField(SortField field) { _sort_field = field; notifyModelChanged(); }
  SortField getSortField() const { return _sort_field; }

  void setSortOrder(SortOrder order) { _sort_order = order; notifyModelChanged(); }
  SortOrder getSortOrder() const { return _sort_order; }

  // Whether to sort directories before files
  void setDirectoriesFirst(bool first) { _directories_first = first; notifyModelChanged(); }
  bool getDirectoriesFirst() const { return _directories_first; }

  //////////////////////////////////////////////////////////////
  // Change notifications
  //////////////////////////////////////////////////////////////

  void notifyModelChanged();
  void notifyDirectoryChanged(const std::string& path);

  //////////////////////////////////////////////////////////////
  // Callbacks for observers (FilesystemView subscribes)
  //////////////////////////////////////////////////////////////

  std::function<void()> _onModelChanged;
  std::function<void(const std::string& path)> _onDirectoryChanged;

protected:
  // Check if entry passes current filter
  bool _passesFilter(const FilesystemEntry& entry) const;
  void _parseFilterPattern();
  filesystem_entry_list_t _sortEntries(filesystem_entry_list_t entries) const;

  // Glob-style pattern matching (supports * and ?)
  static bool _globMatch(const std::string& pattern, const std::string& text);

  std::string _filter_pattern;
  std::string _name_filter;                     // Name glob pattern
  std::vector<std::string> _filter_extensions;  // Parsed extension filters
  std::vector<std::string> _filter_patterns;    // Parsed glob patterns
  bool _show_hidden = false;
  bool _show_directories = true;
  SortField _sort_field = SortField::Name;
  SortOrder _sort_order = SortOrder::Ascending;
  bool _directories_first = true;
};

////////////////////////////////////////////////////////////////////
// LocalFilesystemModel: Concrete model for real filesystem
////////////////////////////////////////////////////////////////////

struct LocalFilesystemModel : public FilesystemModel {
  LocalFilesystemModel();
  LocalFilesystemModel(const std::string& initial_path);
  ~LocalFilesystemModel() override = default;

  // Model identification
  std::string modelIdentifier() const override { return "local"; }

  // FilesystemModel interface
  std::string getCurrentPath() const override;
  bool setCurrentPath(const std::string& path) override;
  std::string getParentPath() const override;
  bool exists(const std::string& path) const override;
  bool isDirectory(const std::string& path) const override;

  filesystem_entry_list_t getEntries() const override;
  FilesystemEntry getEntry(const std::string& path) const override;
  std::string getDisplayName(const std::string& path) const override;

  bool isReadOnly() const override { return _read_only; }
  void setReadOnly(bool ro) { _read_only = ro; }

  bool createDirectory(const std::string& name) override;
  bool deleteItem(const std::string& path) override;
  std::string renameItem(const std::string& path, const std::string& new_name) override;

  // Thumbnail support for images
  lev2::image_provider_ptr_t getThumbnailProvider(const std::string& path, int size) override;
  bool hasThumbnail(const std::string& path) const override;

  // Root path constraint (optional - limits navigation)
  void setRootPath(const std::string& root) { _root_path = root; }
  std::string getRootPath() const { return _root_path; }

private:
  std::string _current_path;
  std::string _root_path;  // Optional constraint
  bool _read_only = false;

  // MIME type detection
  static std::string _detectMimeType(const std::string& path);
  static bool _isImageFile(const std::string& extension);
};

using local_filesystem_model_ptr_t = std::shared_ptr<LocalFilesystemModel>;

} // namespace ork::ui
