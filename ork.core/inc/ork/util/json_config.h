////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/svariant.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <functional>

namespace ork {

////////////////////////////////////////////////////////////////////
// JsonConfig: Persistent JSON configuration with atomic saves
// - Creates config file on demand
// - Atomic writes (temp file + rename) for crash resilience
// - Thread-safe access
////////////////////////////////////////////////////////////////////

struct JsonConfig;
using json_config_ptr_t = std::shared_ptr<JsonConfig>;

struct JsonConfig {
  JsonConfig();
  ~JsonConfig();

  //////////////////////////////////////////////////////////////
  // Factory methods
  //////////////////////////////////////////////////////////////

  // Get or create a named config (configs are cached by path)
  static json_config_ptr_t instance(const std::string& name);

  // Get the default application config
  static json_config_ptr_t defaultConfig();

  // Set the config directory (default: ~/.config/orkid/)
  static void setConfigDirectory(const std::string& dir);
  static std::string getConfigDirectory();

  //////////////////////////////////////////////////////////////
  // Load/Save
  //////////////////////////////////////////////////////////////

  // Load from file (creates default if missing)
  bool load(const std::string& filepath);

  // Save to file (atomic write)
  bool save();

  // Force immediate save (bypasses any debouncing)
  bool saveNow();

  // Get the file path
  std::string getFilePath() const { return _filepath; }

  //////////////////////////////////////////////////////////////
  // String values
  //////////////////////////////////////////////////////////////

  void setString(const std::string& key, const std::string& value);
  std::string getString(const std::string& key, const std::string& default_value = "") const;
  bool hasKey(const std::string& key) const;
  void removeKey(const std::string& key);

  //////////////////////////////////////////////////////////////
  // Numeric values
  //////////////////////////////////////////////////////////////

  void setInt(const std::string& key, int64_t value);
  int64_t getInt(const std::string& key, int64_t default_value = 0) const;

  void setFloat(const std::string& key, double value);
  double getFloat(const std::string& key, double default_value = 0.0) const;

  void setBool(const std::string& key, bool value);
  bool getBool(const std::string& key, bool default_value = false) const;

  //////////////////////////////////////////////////////////////
  // String list values (for favorites, recent files, etc.)
  //////////////////////////////////////////////////////////////

  void setStringList(const std::string& key, const std::vector<std::string>& values);
  std::vector<std::string> getStringList(const std::string& key) const;

  void appendToStringList(const std::string& key, const std::string& value);
  void removeFromStringList(const std::string& key, const std::string& value);
  bool stringListContains(const std::string& key, const std::string& value) const;

  //////////////////////////////////////////////////////////////
  // Nested objects (for model-specific data)
  //////////////////////////////////////////////////////////////

  // Get a nested config object (creates if missing)
  json_config_ptr_t getObject(const std::string& key);

  // Set a nested object
  void setObject(const std::string& key, json_config_ptr_t obj);

  //////////////////////////////////////////////////////////////
  // Callbacks
  //////////////////////////////////////////////////////////////

  std::function<void()> _onChanged;

  //////////////////////////////////////////////////////////////
  // Internal data access (for serialization)
  //////////////////////////////////////////////////////////////

  using value_t = svar256_t;
  using map_t = std::unordered_map<std::string, value_t>;

  const map_t& data() const { return _data; }
  void setData(const map_t& data) { _data = data; _dirty = true; }

private:
  void _markDirty();
  bool _loadFromFile();
  bool _saveToFile();
  std::string _toJson() const;
  bool _fromJson(const std::string& json);

  std::string _filepath;
  map_t _data;
  mutable std::mutex _mutex;
  bool _dirty = false;
  bool _loaded = false;

  // Nested objects cache
  std::unordered_map<std::string, json_config_ptr_t> _nested_objects;

  // Static config cache
  static std::unordered_map<std::string, json_config_ptr_t> _config_cache;
  static std::mutex _cache_mutex;
  static std::string _config_directory;
};

} // namespace ork
