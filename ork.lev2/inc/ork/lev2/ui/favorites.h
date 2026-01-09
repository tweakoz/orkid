////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/json_config.h>
#include <ork/lev2/ui/filesystem_model.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// FavoriteEntry: A favorite location with associated view state
////////////////////////////////////////////////////////////////////

struct FavoriteEntry;
using favorite_entry_ptr_t = std::shared_ptr<FavoriteEntry>;

struct FavoriteEntry {
  std::string path;                                           // Directory path
  std::string name;                                           // Display name (defaults to path basename)
  std::string name_filter;                                    // Name filter pattern
  FilesystemModel::SortField sort_field = FilesystemModel::SortField::Name;
  FilesystemModel::SortOrder sort_order = FilesystemModel::SortOrder::Ascending;
  bool directories_first = true;
  bool show_hidden = false;

  // Create from current model state
  static favorite_entry_ptr_t fromModel(FilesystemModel* model, const std::string& display_name = "");

  // Apply this favorite's state to a model
  void applyToModel(FilesystemModel* model) const;

  // Serialize to/from JSON object
  void toJson(json_config_ptr_t obj) const;
  static favorite_entry_ptr_t fromJson(json_config_ptr_t obj);

  // Get display name (returns name if set, otherwise path basename)
  std::string displayName() const;
};

////////////////////////////////////////////////////////////////////
// FavoritesManager: Manages favorite paths for filesystem models
// - Stores favorites per model identifier (e.g., "local", "s3")
// - Persists to JSON config
// - Thread-safe
////////////////////////////////////////////////////////////////////

struct FavoritesManager;
using favorites_manager_ptr_t = std::shared_ptr<FavoritesManager>;

struct FavoritesManager {
  FavoritesManager();
  ~FavoritesManager();

  //////////////////////////////////////////////////////////////
  // Singleton access
  //////////////////////////////////////////////////////////////

  static favorites_manager_ptr_t instance();

  //////////////////////////////////////////////////////////////
  // Full-state favorites (with filter/sort)
  //////////////////////////////////////////////////////////////

  // Add a favorite entry for a specific model
  void addFavoriteEntry(const std::string& model_id, favorite_entry_ptr_t entry);

  // Remove a favorite by path
  void removeFavoriteEntry(const std::string& model_id, const std::string& path);

  // Get a favorite entry by path
  favorite_entry_ptr_t getFavoriteEntry(const std::string& model_id, const std::string& path) const;

  // Update an existing favorite entry
  void updateFavoriteEntry(const std::string& model_id, favorite_entry_ptr_t entry);

  // Get all favorite entries for a model
  std::vector<favorite_entry_ptr_t> getFavoriteEntries(const std::string& model_id) const;

  //////////////////////////////////////////////////////////////
  // Simple favorites (path only, backwards compatible)
  //////////////////////////////////////////////////////////////

  // Add a favorite for a specific model
  void addFavorite(const std::string& model_id, const std::string& path);

  // Remove a favorite
  void removeFavorite(const std::string& model_id, const std::string& path);

  // Check if path is a favorite
  bool isFavorite(const std::string& model_id, const std::string& path) const;

  // Get all favorites for a model (paths only)
  std::vector<std::string> getFavorites(const std::string& model_id) const;

  // Clear all favorites for a model
  void clearFavorites(const std::string& model_id);

  // Move a favorite up/down in the list
  void moveFavoriteUp(const std::string& model_id, const std::string& path);
  void moveFavoriteDown(const std::string& model_id, const std::string& path);

  //////////////////////////////////////////////////////////////
  // Recent paths (separate from favorites)
  //////////////////////////////////////////////////////////////

  // Add to recent paths (auto-manages list size)
  void addRecent(const std::string& model_id, const std::string& path);

  // Get recent paths
  std::vector<std::string> getRecent(const std::string& model_id) const;

  // Clear recent paths
  void clearRecent(const std::string& model_id);

  // Set max recent paths to remember (default: 20)
  void setMaxRecent(int max) { _max_recent = max; }
  int getMaxRecent() const { return _max_recent; }

  //////////////////////////////////////////////////////////////
  // Persistence
  //////////////////////////////////////////////////////////////

  // Force save (normally auto-saves on changes)
  void save();

  //////////////////////////////////////////////////////////////
  // Callbacks
  //////////////////////////////////////////////////////////////

  std::function<void(const std::string& model_id)> _onFavoritesChanged;
  std::function<void(const std::string& model_id)> _onRecentChanged;

private:
  std::string _favoritesKey(const std::string& model_id) const;
  std::string _recentKey(const std::string& model_id) const;

  // Cache of loaded favorites per model
  mutable std::map<std::string, std::vector<favorite_entry_ptr_t>> _favorites_cache;
  void _loadFavorites(const std::string& model_id) const;
  void _saveFavorites(const std::string& model_id);

  json_config_ptr_t _config;
  int _max_recent = 20;

  static favorites_manager_ptr_t _instance;
  static std::mutex _instance_mutex;
};

} // namespace ork::ui
