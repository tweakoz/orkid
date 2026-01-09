////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/ui/favorites.h>
#include <ork/file/path.h>
#include <algorithm>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////
// FavoriteEntry implementation
///////////////////////////////////////////////////////////////////////////////

favorite_entry_ptr_t FavoriteEntry::fromModel(FilesystemModel* model, const std::string& display_name) {
  auto entry = std::make_shared<FavoriteEntry>();
  entry->path = model->getCurrentPath();
  entry->name = display_name;
  entry->name_filter = model->getNameFilter();
  entry->sort_field = model->getSortField();
  entry->sort_order = model->getSortOrder();
  entry->directories_first = model->getDirectoriesFirst();
  entry->show_hidden = model->getShowHidden();
  return entry;
}

void FavoriteEntry::applyToModel(FilesystemModel* model) const {
  model->setCurrentPath(path);
  model->setNameFilter(name_filter);
  model->setSortField(sort_field);
  model->setSortOrder(sort_order);
  model->setDirectoriesFirst(directories_first);
  model->setShowHidden(show_hidden);
}

void FavoriteEntry::toJson(json_config_ptr_t obj) const {
  obj->setString("path", path);
  obj->setString("name", name);
  obj->setString("name_filter", name_filter);
  obj->setInt("sort_field", static_cast<int>(sort_field));
  obj->setInt("sort_order", static_cast<int>(sort_order));
  obj->setBool("directories_first", directories_first);
  obj->setBool("show_hidden", show_hidden);
}

favorite_entry_ptr_t FavoriteEntry::fromJson(json_config_ptr_t obj) {
  auto entry = std::make_shared<FavoriteEntry>();
  entry->path = obj->getString("path", "");
  entry->name = obj->getString("name", "");
  entry->name_filter = obj->getString("name_filter", "");
  entry->sort_field = static_cast<FilesystemModel::SortField>(obj->getInt("sort_field", 0));
  entry->sort_order = static_cast<FilesystemModel::SortOrder>(obj->getInt("sort_order", 0));
  entry->directories_first = obj->getBool("directories_first", true);
  entry->show_hidden = obj->getBool("show_hidden", false);
  return entry;
}

std::string FavoriteEntry::displayName() const {
  if (!name.empty()) {
    return name;
  }
  // Return basename of path
  file::Path p(path);
  auto basename = p.getName();
  return basename.c_str()[0] ? std::string(basename.c_str()) : path;
}

///////////////////////////////////////////////////////////////////////////////
// FavoritesManager static members
///////////////////////////////////////////////////////////////////////////////

favorites_manager_ptr_t FavoritesManager::_instance;
std::mutex FavoritesManager::_instance_mutex;

///////////////////////////////////////////////////////////////////////////////

FavoritesManager::FavoritesManager() {
  _config = JsonConfig::instance("favorites");
}

FavoritesManager::~FavoritesManager() {
  save();
}

///////////////////////////////////////////////////////////////////////////////

favorites_manager_ptr_t FavoritesManager::instance() {
  std::lock_guard<std::mutex> lock(_instance_mutex);
  if (!_instance) {
    _instance = std::make_shared<FavoritesManager>();
  }
  return _instance;
}

///////////////////////////////////////////////////////////////////////////////

std::string FavoritesManager::_favoritesKey(const std::string& model_id) const {
  return "favorites_" + model_id;
}

std::string FavoritesManager::_recentKey(const std::string& model_id) const {
  return "recent_" + model_id;
}

///////////////////////////////////////////////////////////////////////////////
// Internal favorites cache management
///////////////////////////////////////////////////////////////////////////////

void FavoritesManager::_loadFavorites(const std::string& model_id) const {
  // Check if already loaded
  auto it = _favorites_cache.find(model_id);
  if (it != _favorites_cache.end()) {
    return;
  }

  std::vector<favorite_entry_ptr_t> entries;
  std::string key = _favoritesKey(model_id);

  // Get the count of favorites
  auto count_key = key + "_count";
  int count = static_cast<int>(_config->getInt(count_key, 0));

  for (int i = 0; i < count; i++) {
    auto entry_key = key + "_" + std::to_string(i);
    auto entry_obj = _config->getObject(entry_key);
    if (entry_obj && entry_obj->hasKey("path")) {
      auto entry = FavoriteEntry::fromJson(entry_obj);
      if (!entry->path.empty()) {
        entries.push_back(entry);
      }
    }
  }

  _favorites_cache[model_id] = entries;
}

void FavoritesManager::_saveFavorites(const std::string& model_id) {
  auto it = _favorites_cache.find(model_id);
  if (it == _favorites_cache.end()) {
    return;
  }

  const auto& entries = it->second;
  std::string key = _favoritesKey(model_id);

  // Save count
  auto count_key = key + "_count";
  _config->setInt(count_key, static_cast<int64_t>(entries.size()));

  // Save each entry
  for (size_t i = 0; i < entries.size(); i++) {
    auto entry_key = key + "_" + std::to_string(i);
    auto entry_obj = _config->getObject(entry_key);
    entries[i]->toJson(entry_obj);
  }

  _config->save();
}

///////////////////////////////////////////////////////////////////////////////
// Full-state favorites (with filter/sort)
///////////////////////////////////////////////////////////////////////////////

void FavoritesManager::addFavoriteEntry(const std::string& model_id, favorite_entry_ptr_t entry) {
  _loadFavorites(model_id);
  auto& entries = _favorites_cache[model_id];

  // Check if already exists (by path)
  for (auto& e : entries) {
    if (e->path == entry->path) {
      // Update existing
      *e = *entry;
      _saveFavorites(model_id);
      if (_onFavoritesChanged) {
        _onFavoritesChanged(model_id);
      }
      return;
    }
  }

  // Add new
  entries.push_back(entry);
  _saveFavorites(model_id);
  if (_onFavoritesChanged) {
    _onFavoritesChanged(model_id);
  }
}

void FavoritesManager::removeFavoriteEntry(const std::string& model_id, const std::string& path) {
  _loadFavorites(model_id);
  auto& entries = _favorites_cache[model_id];

  entries.erase(
      std::remove_if(entries.begin(), entries.end(),
                     [&path](const favorite_entry_ptr_t& e) { return e->path == path; }),
      entries.end());

  _saveFavorites(model_id);
  if (_onFavoritesChanged) {
    _onFavoritesChanged(model_id);
  }
}

favorite_entry_ptr_t FavoritesManager::getFavoriteEntry(const std::string& model_id, const std::string& path) const {
  _loadFavorites(model_id);
  auto it = _favorites_cache.find(model_id);
  if (it != _favorites_cache.end()) {
    for (const auto& e : it->second) {
      if (e->path == path) {
        return e;
      }
    }
  }
  return nullptr;
}

void FavoritesManager::updateFavoriteEntry(const std::string& model_id, favorite_entry_ptr_t entry) {
  _loadFavorites(model_id);
  auto& entries = _favorites_cache[model_id];

  for (auto& e : entries) {
    if (e->path == entry->path) {
      *e = *entry;
      _saveFavorites(model_id);
      if (_onFavoritesChanged) {
        _onFavoritesChanged(model_id);
      }
      return;
    }
  }
}

std::vector<favorite_entry_ptr_t> FavoritesManager::getFavoriteEntries(const std::string& model_id) const {
  _loadFavorites(model_id);
  auto it = _favorites_cache.find(model_id);
  if (it != _favorites_cache.end()) {
    return it->second;
  }
  return {};
}

///////////////////////////////////////////////////////////////////////////////
// Simple favorites (path only, backwards compatible)
///////////////////////////////////////////////////////////////////////////////

void FavoritesManager::addFavorite(const std::string& model_id, const std::string& path) {
  auto entry = std::make_shared<FavoriteEntry>();
  entry->path = path;
  addFavoriteEntry(model_id, entry);
}

void FavoritesManager::removeFavorite(const std::string& model_id, const std::string& path) {
  removeFavoriteEntry(model_id, path);
}

bool FavoritesManager::isFavorite(const std::string& model_id, const std::string& path) const {
  return getFavoriteEntry(model_id, path) != nullptr;
}

std::vector<std::string> FavoritesManager::getFavorites(const std::string& model_id) const {
  auto entries = getFavoriteEntries(model_id);
  std::vector<std::string> paths;
  paths.reserve(entries.size());
  for (const auto& e : entries) {
    paths.push_back(e->path);
  }
  return paths;
}

void FavoritesManager::clearFavorites(const std::string& model_id) {
  _favorites_cache[model_id].clear();
  _saveFavorites(model_id);
  if (_onFavoritesChanged) {
    _onFavoritesChanged(model_id);
  }
}

void FavoritesManager::moveFavoriteUp(const std::string& model_id, const std::string& path) {
  _loadFavorites(model_id);
  auto& entries = _favorites_cache[model_id];

  for (size_t i = 1; i < entries.size(); i++) {
    if (entries[i]->path == path) {
      std::swap(entries[i], entries[i - 1]);
      _saveFavorites(model_id);
      if (_onFavoritesChanged) {
        _onFavoritesChanged(model_id);
      }
      break;
    }
  }
}

void FavoritesManager::moveFavoriteDown(const std::string& model_id, const std::string& path) {
  _loadFavorites(model_id);
  auto& entries = _favorites_cache[model_id];

  for (size_t i = 0; i + 1 < entries.size(); i++) {
    if (entries[i]->path == path) {
      std::swap(entries[i], entries[i + 1]);
      _saveFavorites(model_id);
      if (_onFavoritesChanged) {
        _onFavoritesChanged(model_id);
      }
      break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Recent paths
///////////////////////////////////////////////////////////////////////////////

void FavoritesManager::addRecent(const std::string& model_id, const std::string& path) {
  std::string key = _recentKey(model_id);
  auto list = _config->getStringList(key);

  // Remove if already exists (will re-add at front)
  list.erase(std::remove(list.begin(), list.end(), path), list.end());

  // Add to front
  list.insert(list.begin(), path);

  // Trim to max size
  if (list.size() > static_cast<size_t>(_max_recent)) {
    list.resize(_max_recent);
  }

  _config->setStringList(key, list);
  _config->save();

  if (_onRecentChanged) {
    _onRecentChanged(model_id);
  }
}

std::vector<std::string> FavoritesManager::getRecent(const std::string& model_id) const {
  std::string key = _recentKey(model_id);
  return _config->getStringList(key);
}

void FavoritesManager::clearRecent(const std::string& model_id) {
  std::string key = _recentKey(model_id);
  _config->setStringList(key, {});
  _config->save();
  if (_onRecentChanged) {
    _onRecentChanged(model_id);
  }
}

///////////////////////////////////////////////////////////////////////////////

void FavoritesManager::save() {
  if (_config) {
    _config->saveNow();
  }
}

} // namespace ork::ui
