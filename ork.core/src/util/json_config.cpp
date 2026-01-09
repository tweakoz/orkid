////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/util/json_config.h>
#include <ork/file/path.h>
#include <ork/kernel/string/string.h>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>

namespace fs = std::filesystem;

namespace ork {

// Static members
std::unordered_map<std::string, json_config_ptr_t> JsonConfig::_config_cache;
std::mutex JsonConfig::_cache_mutex;
std::string JsonConfig::_config_directory;

///////////////////////////////////////////////////////////////////////////////

JsonConfig::JsonConfig() = default;
JsonConfig::~JsonConfig() {
  if (_dirty && !_filepath.empty()) {
    saveNow();
  }
}

///////////////////////////////////////////////////////////////////////////////

void JsonConfig::setConfigDirectory(const std::string& dir) {
  std::lock_guard<std::mutex> lock(_cache_mutex);
  _config_directory = dir;
}

std::string JsonConfig::getConfigDirectory() {
  std::lock_guard<std::mutex> lock(_cache_mutex);
  if (_config_directory.empty()) {
    // Default to ~/.config/orkid/
    const char* home = std::getenv("HOME");
    if (home) {
      _config_directory = std::string(home) + "/.config/orkid";
    } else {
      _config_directory = "/tmp/orkid_config";
    }
  }
  return _config_directory;
}

///////////////////////////////////////////////////////////////////////////////

json_config_ptr_t JsonConfig::instance(const std::string& name) {
  std::string dir = getConfigDirectory();
  std::string filepath = dir + "/" + name + ".json";

  std::lock_guard<std::mutex> lock(_cache_mutex);
  auto it = _config_cache.find(filepath);
  if (it != _config_cache.end()) {
    return it->second;
  }

  auto config = std::make_shared<JsonConfig>();
  config->load(filepath);
  _config_cache[filepath] = config;
  return config;
}

json_config_ptr_t JsonConfig::defaultConfig() {
  return instance("config");
}

///////////////////////////////////////////////////////////////////////////////

bool JsonConfig::load(const std::string& filepath) {
  std::lock_guard<std::mutex> lock(_mutex);
  _filepath = filepath;
  return _loadFromFile();
}

bool JsonConfig::_loadFromFile() {
  // Ensure directory exists
  fs::path path(_filepath);
  fs::path dir = path.parent_path();
  if (!dir.empty() && !fs::exists(dir)) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
      return false;
    }
  }

  // If file doesn't exist, create empty config
  if (!fs::exists(_filepath)) {
    _data.clear();
    _dirty = false;
    _loaded = true;
    return true;
  }

  // Read file
  std::ifstream file(_filepath);
  if (!file.is_open()) {
    return false;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string json = buffer.str();
  file.close();

  if (json.empty()) {
    _data.clear();
    _dirty = false;
    _loaded = true;
    return true;
  }

  bool ok = _fromJson(json);
  if (ok) {
    _dirty = false;
    _loaded = true;
  }
  return ok;
}

bool JsonConfig::save() {
  return saveNow();
}

bool JsonConfig::saveNow() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (!_dirty) {
    return true;
  }
  return _saveToFile();
}

bool JsonConfig::_saveToFile() {
  if (_filepath.empty()) {
    return false;
  }

  std::string json = _toJson();

  // Atomic write: write to temp file, then rename
  std::string temp_path = _filepath + ".tmp";

  std::ofstream file(temp_path);
  if (!file.is_open()) {
    return false;
  }

  file << json;
  file.close();

  if (!file.good()) {
    fs::remove(temp_path);
    return false;
  }

  // Atomic rename
  std::error_code ec;
  fs::rename(temp_path, _filepath, ec);
  if (ec) {
    fs::remove(temp_path);
    return false;
  }

  _dirty = false;
  return true;
}

///////////////////////////////////////////////////////////////////////////////

std::string JsonConfig::_toJson() const {
  rapidjson::Document doc;
  doc.SetObject();
  auto& allocator = doc.GetAllocator();

  for (const auto& [key, val] : _data) {
    rapidjson::Value jkey(key.c_str(), allocator);

    if (auto as_str = val.tryAs<std::string>()) {
      rapidjson::Value jval(as_str.value().c_str(), allocator);
      doc.AddMember(jkey, jval, allocator);
    } else if (auto as_int = val.tryAs<int64_t>()) {
      doc.AddMember(jkey, as_int.value(), allocator);
    } else if (auto as_double = val.tryAs<double>()) {
      doc.AddMember(jkey, as_double.value(), allocator);
    } else if (auto as_bool = val.tryAs<bool>()) {
      doc.AddMember(jkey, as_bool.value(), allocator);
    } else if (auto as_list = val.tryAs<std::vector<std::string>>()) {
      rapidjson::Value arr(rapidjson::kArrayType);
      for (const auto& item : as_list.value()) {
        rapidjson::Value jitem(item.c_str(), allocator);
        arr.PushBack(jitem, allocator);
      }
      doc.AddMember(jkey, arr, allocator);
    }
    // Nested objects are handled separately (stored in _nested_objects)
  }

  // Serialize nested objects
  for (const auto& [key, nested] : _nested_objects) {
    if (nested) {
      rapidjson::Value jkey(key.c_str(), allocator);
      rapidjson::Document nested_doc;
      nested_doc.Parse(nested->_toJson().c_str());
      if (!nested_doc.HasParseError()) {
        rapidjson::Value nested_val(nested_doc, allocator);
        doc.AddMember(jkey, nested_val, allocator);
      }
    }
  }

  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);

  return buffer.GetString();
}

bool JsonConfig::_fromJson(const std::string& json) {
  rapidjson::Document doc;
  doc.Parse(json.c_str());

  if (doc.HasParseError()) {
    return false;
  }

  if (!doc.IsObject()) {
    return false;
  }

  _data.clear();
  _nested_objects.clear();

  for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
    std::string key = it->name.GetString();
    const auto& val = it->value;

    if (val.IsString()) {
      _data[key].set<std::string>(val.GetString());
    } else if (val.IsInt64()) {
      _data[key].set<int64_t>(val.GetInt64());
    } else if (val.IsDouble()) {
      _data[key].set<double>(val.GetDouble());
    } else if (val.IsBool()) {
      _data[key].set<bool>(val.GetBool());
    } else if (val.IsArray()) {
      std::vector<std::string> list;
      for (auto& item : val.GetArray()) {
        if (item.IsString()) {
          list.push_back(item.GetString());
        }
      }
      _data[key].set<std::vector<std::string>>(list);
    } else if (val.IsObject()) {
      // Nested object
      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      val.Accept(writer);

      auto nested = std::make_shared<JsonConfig>();
      nested->_fromJson(buffer.GetString());
      _nested_objects[key] = nested;
    }
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void JsonConfig::_markDirty() {
  _dirty = true;
  if (_onChanged) {
    _onChanged();
  }
}

///////////////////////////////////////////////////////////////////////////////
// String values
///////////////////////////////////////////////////////////////////////////////

void JsonConfig::setString(const std::string& key, const std::string& value) {
  std::lock_guard<std::mutex> lock(_mutex);
  _data[key].set<std::string>(value);
  _markDirty();
}

std::string JsonConfig::getString(const std::string& key, const std::string& default_value) const {
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _data.find(key);
  if (it != _data.end()) {
    if (auto as_str = it->second.tryAs<std::string>()) {
      return as_str.value();
    }
  }
  return default_value;
}

bool JsonConfig::hasKey(const std::string& key) const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _data.find(key) != _data.end() || _nested_objects.find(key) != _nested_objects.end();
}

void JsonConfig::removeKey(const std::string& key) {
  std::lock_guard<std::mutex> lock(_mutex);
  _data.erase(key);
  _nested_objects.erase(key);
  _markDirty();
}

///////////////////////////////////////////////////////////////////////////////
// Numeric values
///////////////////////////////////////////////////////////////////////////////

void JsonConfig::setInt(const std::string& key, int64_t value) {
  std::lock_guard<std::mutex> lock(_mutex);
  _data[key].set<int64_t>(value);
  _markDirty();
}

int64_t JsonConfig::getInt(const std::string& key, int64_t default_value) const {
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _data.find(key);
  if (it != _data.end()) {
    if (auto as_int = it->second.tryAs<int64_t>()) {
      return as_int.value();
    }
  }
  return default_value;
}

void JsonConfig::setFloat(const std::string& key, double value) {
  std::lock_guard<std::mutex> lock(_mutex);
  _data[key].set<double>(value);
  _markDirty();
}

double JsonConfig::getFloat(const std::string& key, double default_value) const {
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _data.find(key);
  if (it != _data.end()) {
    if (auto as_double = it->second.tryAs<double>()) {
      return as_double.value();
    }
  }
  return default_value;
}

void JsonConfig::setBool(const std::string& key, bool value) {
  std::lock_guard<std::mutex> lock(_mutex);
  _data[key].set<bool>(value);
  _markDirty();
}

bool JsonConfig::getBool(const std::string& key, bool default_value) const {
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _data.find(key);
  if (it != _data.end()) {
    if (auto as_bool = it->second.tryAs<bool>()) {
      return as_bool.value();
    }
  }
  return default_value;
}

///////////////////////////////////////////////////////////////////////////////
// String list values
///////////////////////////////////////////////////////////////////////////////

void JsonConfig::setStringList(const std::string& key, const std::vector<std::string>& values) {
  std::lock_guard<std::mutex> lock(_mutex);
  _data[key].set<std::vector<std::string>>(values);
  _markDirty();
}

std::vector<std::string> JsonConfig::getStringList(const std::string& key) const {
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _data.find(key);
  if (it != _data.end()) {
    if (auto as_list = it->second.tryAs<std::vector<std::string>>()) {
      return as_list.value();
    }
  }
  return {};
}

void JsonConfig::appendToStringList(const std::string& key, const std::string& value) {
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _data.find(key);
  std::vector<std::string> list;
  if (it != _data.end()) {
    if (auto as_list = it->second.tryAs<std::vector<std::string>>()) {
      list = as_list.value();
    }
  }
  // Avoid duplicates
  for (const auto& item : list) {
    if (item == value) return;
  }
  list.push_back(value);
  _data[key].set<std::vector<std::string>>(list);
  _markDirty();
}

void JsonConfig::removeFromStringList(const std::string& key, const std::string& value) {
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _data.find(key);
  if (it != _data.end()) {
    if (auto as_list = it->second.tryAs<std::vector<std::string>>()) {
      auto list = as_list.value();
      list.erase(std::remove(list.begin(), list.end(), value), list.end());
      _data[key].set<std::vector<std::string>>(list);
      _markDirty();
    }
  }
}

bool JsonConfig::stringListContains(const std::string& key, const std::string& value) const {
  auto list = getStringList(key);
  for (const auto& item : list) {
    if (item == value) return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// Nested objects
///////////////////////////////////////////////////////////////////////////////

json_config_ptr_t JsonConfig::getObject(const std::string& key) {
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _nested_objects.find(key);
  if (it != _nested_objects.end()) {
    return it->second;
  }
  // Create new nested object
  auto obj = std::make_shared<JsonConfig>();
  _nested_objects[key] = obj;
  _markDirty();
  return obj;
}

void JsonConfig::setObject(const std::string& key, json_config_ptr_t obj) {
  std::lock_guard<std::mutex> lock(_mutex);
  _nested_objects[key] = obj;
  _markDirty();
}

} // namespace ork
