////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/ui/filesystem_model.h>
#include <ork/file/path.h>
#include <algorithm>
#include <filesystem>
#include <cctype>

namespace fs = std::filesystem;

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////
// FilesystemModel base class
///////////////////////////////////////////////////////////////////////////////

void FilesystemModel::notifyModelChanged() {
  if (_onModelChanged) {
    _onModelChanged();
  }
}

void FilesystemModel::notifyDirectoryChanged(const std::string& path) {
  if (_onDirectoryChanged) {
    _onDirectoryChanged(path);
  }
}

bool FilesystemModel::_globMatch(const std::string& pattern, const std::string& text) {
  // Simple glob matching supporting * and ?
  size_t p = 0, t = 0;
  size_t star_p = std::string::npos, star_t = 0;

  while (t < text.length()) {
    if (p < pattern.length() && (pattern[p] == '?' || ::tolower(pattern[p]) == ::tolower(text[t]))) {
      // Character match or single wildcard
      p++;
      t++;
    } else if (p < pattern.length() && pattern[p] == '*') {
      // Star wildcard - remember position for backtracking
      star_p = p++;
      star_t = t;
    } else if (star_p != std::string::npos) {
      // Backtrack to last star
      p = star_p + 1;
      t = ++star_t;
    } else {
      return false;
    }
  }

  // Skip trailing stars
  while (p < pattern.length() && pattern[p] == '*') {
    p++;
  }

  return p == pattern.length();
}

void FilesystemModel::_parseFilterPattern() {
  _filter_extensions.clear();
  _filter_patterns.clear();

  if (_filter_pattern.empty()) {
    return;
  }

  // Parse pattern like "*.png;*.jpg;test*;*_backup*"
  std::string pattern = _filter_pattern;

  // Split by semicolon
  std::vector<std::string> parts;
  size_t pos = 0;
  while ((pos = pattern.find(';')) != std::string::npos) {
    std::string part = pattern.substr(0, pos);
    pattern.erase(0, pos + 1);
    // Trim whitespace
    while (!part.empty() && std::isspace(part.front())) part.erase(0, 1);
    while (!part.empty() && std::isspace(part.back())) part.pop_back();
    if (!part.empty()) {
      parts.push_back(part);
    }
  }
  // Handle last part
  while (!pattern.empty() && std::isspace(pattern.front())) pattern.erase(0, 1);
  while (!pattern.empty() && std::isspace(pattern.back())) pattern.pop_back();
  if (!pattern.empty()) {
    parts.push_back(pattern);
  }

  for (const auto& part : parts) {
    // Check if it's a pure extension filter "*.ext"
    if (part.length() > 2 && part[0] == '*' && part[1] == '.' &&
        part.find('*', 2) == std::string::npos && part.find('?', 2) == std::string::npos) {
      std::string ext = part.substr(2);
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      _filter_extensions.push_back(ext);
    } else {
      // It's a glob pattern
      _filter_patterns.push_back(part);
    }
  }
}

bool FilesystemModel::_passesFilter(const FilesystemEntry& entry) const {
  // Check hidden
  if (entry.is_hidden && !_show_hidden) {
    return false;
  }

  // Directories-only mode: reject non-directories
  if (_directories_only && entry.type != FileType::Directory) {
    return false;
  }

  // Check name filter (applies to both files and directories)
  if (!_name_filter.empty()) {
    if (!_globMatch(_name_filter, entry.name)) {
      return false;
    }
  }

  // Directories pass after name_filter check (extension filters don't apply)
  if (entry.type == FileType::Directory) {
    return _show_directories;
  }

  // If we have filters from setFilter(), apply them (OR logic between patterns)
  bool has_extension_filters = !_filter_extensions.empty();
  bool has_glob_patterns = !_filter_patterns.empty();

  if (has_extension_filters || has_glob_patterns) {
    bool match_found = false;

    // Check extension filters
    if (has_extension_filters) {
      std::string ext = entry.extension;
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
      for (const auto& filter_ext : _filter_extensions) {
        if (ext == filter_ext) {
          match_found = true;
          break;
        }
      }
    }

    // Check glob patterns against full filename
    if (!match_found && has_glob_patterns) {
      for (const auto& pattern : _filter_patterns) {
        if (_globMatch(pattern, entry.name)) {
          match_found = true;
          break;
        }
      }
    }

    if (!match_found) {
      return false;
    }
  }

  return true;
}

filesystem_entry_list_t FilesystemModel::_sortEntries(filesystem_entry_list_t entries) const {
  auto compare = [this](const FilesystemEntry& a, const FilesystemEntry& b) -> bool {
    // Directories first if enabled
    if (_directories_first) {
      if (a.type == FileType::Directory && b.type != FileType::Directory) {
        return true;
      }
      if (a.type != FileType::Directory && b.type == FileType::Directory) {
        return false;
      }
    }

    int cmp = 0;
    switch (_sort_field) {
      case SortField::Name:
        cmp = a.name.compare(b.name);
        break;
      case SortField::Size:
        cmp = (a.size < b.size) ? -1 : ((a.size > b.size) ? 1 : 0);
        break;
      case SortField::Type:
        cmp = a.extension.compare(b.extension);
        if (cmp == 0) {
          cmp = a.name.compare(b.name);
        }
        break;
      case SortField::ModifiedTime:
        cmp = (a.modified_time < b.modified_time) ? -1 : ((a.modified_time > b.modified_time) ? 1 : 0);
        break;
    }

    if (_sort_order == SortOrder::Descending) {
      cmp = -cmp;
    }

    return cmp < 0;
  };

  std::sort(entries.begin(), entries.end(), compare);
  return entries;
}

///////////////////////////////////////////////////////////////////////////////
// LocalFilesystemModel implementation
///////////////////////////////////////////////////////////////////////////////

LocalFilesystemModel::LocalFilesystemModel() {
  // Start at home directory
  _current_path = fs::current_path().string();
}

LocalFilesystemModel::LocalFilesystemModel(const std::string& initial_path) {
  if (!initial_path.empty() && fs::exists(initial_path) && fs::is_directory(initial_path)) {
    _current_path = fs::canonical(initial_path).string();
  } else {
    _current_path = fs::current_path().string();
  }
}

std::string LocalFilesystemModel::getCurrentPath() const {
  return _current_path;
}

bool LocalFilesystemModel::setCurrentPath(const std::string& path) {
  try {
    fs::path p(path);
    if (!fs::exists(p) || !fs::is_directory(p)) {
      return false;
    }

    std::string canonical = fs::canonical(p).string();

    // Check root constraint
    if (!_root_path.empty()) {
      if (canonical.find(_root_path) != 0) {
        return false;  // Outside root
      }
    }

    _current_path = canonical;
    notifyDirectoryChanged(_current_path);
    notifyModelChanged();
    return true;
  } catch (const fs::filesystem_error&) {
    return false;
  }
}

std::string LocalFilesystemModel::getParentPath() const {
  try {
    fs::path p(_current_path);
    fs::path parent = p.parent_path();

    if (parent.empty() || parent == p) {
      return "";  // At root
    }

    std::string parent_str = parent.string();

    // Check root constraint
    if (!_root_path.empty()) {
      if (parent_str.length() < _root_path.length()) {
        return "";  // Would go above root
      }
    }

    return parent_str;
  } catch (const fs::filesystem_error&) {
    return "";
  }
}

bool LocalFilesystemModel::exists(const std::string& path) const {
  try {
    return fs::exists(path);
  } catch (const fs::filesystem_error&) {
    return false;
  }
}

bool LocalFilesystemModel::isDirectory(const std::string& path) const {
  try {
    return fs::is_directory(path);
  } catch (const fs::filesystem_error&) {
    return false;
  }
}

filesystem_entry_list_t LocalFilesystemModel::getEntries() const {
  filesystem_entry_list_t entries;

  try {
    for (const auto& dir_entry : fs::directory_iterator(_current_path)) {
      FilesystemEntry entry;
      entry.path = dir_entry.path().string();
      entry.name = dir_entry.path().filename().string();

      // Determine type
      if (dir_entry.is_symlink()) {
        entry.type = FileType::Symlink;
      } else if (dir_entry.is_directory()) {
        entry.type = FileType::Directory;
      } else if (dir_entry.is_regular_file()) {
        entry.type = FileType::File;
      } else {
        entry.type = FileType::Unknown;
      }

      // Get size (only for files)
      if (entry.type == FileType::File) {
        try {
          entry.size = dir_entry.file_size();
        } catch (...) {
          entry.size = 0;
        }
      }

      // Get modification time
      try {
        auto ftime = dir_entry.last_write_time();
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        entry.modified_time = std::chrono::system_clock::to_time_t(sctp);
      } catch (...) {
        entry.modified_time = 0;
      }

      // Get extension
      if (dir_entry.path().has_extension()) {
        entry.extension = dir_entry.path().extension().string();
        if (!entry.extension.empty() && entry.extension[0] == '.') {
          entry.extension = entry.extension.substr(1);
        }
        std::transform(entry.extension.begin(), entry.extension.end(),
                       entry.extension.begin(), ::tolower);
      }

      // Check hidden (Unix: starts with .)
      entry.is_hidden = (!entry.name.empty() && entry.name[0] == '.');

      // Get MIME type
      entry.mime_type = _detectMimeType(entry.path);

      // Check permissions
      try {
        auto perms = dir_entry.status().permissions();
        entry.is_readable = (perms & fs::perms::owner_read) != fs::perms::none;
        entry.is_writable = (perms & fs::perms::owner_write) != fs::perms::none;
      } catch (...) {
        entry.is_readable = true;
        entry.is_writable = false;
      }

      // Apply filter
      if (_passesFilter(entry)) {
        entries.push_back(entry);
      }
    }
  } catch (const fs::filesystem_error&) {
    // Return empty list on error
  }

  return _sortEntries(entries);
}

FilesystemEntry LocalFilesystemModel::getEntry(const std::string& path) const {
  FilesystemEntry entry;
  entry.path = path;

  try {
    fs::path p(path);
    entry.name = p.filename().string();

    if (fs::is_symlink(p)) {
      entry.type = FileType::Symlink;
    } else if (fs::is_directory(p)) {
      entry.type = FileType::Directory;
    } else if (fs::is_regular_file(p)) {
      entry.type = FileType::File;
      entry.size = fs::file_size(p);
    } else {
      entry.type = FileType::Unknown;
    }

    // Get modification time
    auto ftime = fs::last_write_time(p);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    entry.modified_time = std::chrono::system_clock::to_time_t(sctp);

    // Get extension
    if (p.has_extension()) {
      entry.extension = p.extension().string();
      if (!entry.extension.empty() && entry.extension[0] == '.') {
        entry.extension = entry.extension.substr(1);
      }
      std::transform(entry.extension.begin(), entry.extension.end(),
                     entry.extension.begin(), ::tolower);
    }

    entry.is_hidden = (!entry.name.empty() && entry.name[0] == '.');
    entry.mime_type = _detectMimeType(path);

  } catch (const fs::filesystem_error&) {
    entry.type = FileType::Unknown;
  }

  return entry;
}

std::string LocalFilesystemModel::getDisplayName(const std::string& path) const {
  return fs::path(path).filename().string();
}

bool LocalFilesystemModel::createDirectory(const std::string& name) {
  if (_read_only) return false;

  try {
    fs::path new_path = fs::path(_current_path) / name;
    bool result = fs::create_directory(new_path);
    if (result) {
      notifyModelChanged();
    }
    return result;
  } catch (const fs::filesystem_error&) {
    return false;
  }
}

bool LocalFilesystemModel::deleteItem(const std::string& path) {
  if (_read_only) return false;

  try {
    // Safety check: don't delete outside root
    if (!_root_path.empty()) {
      std::string canonical = fs::canonical(path).string();
      if (canonical.find(_root_path) != 0) {
        return false;
      }
    }

    bool result = fs::remove_all(path) > 0;
    if (result) {
      notifyModelChanged();
    }
    return result;
  } catch (const fs::filesystem_error&) {
    return false;
  }
}

std::string LocalFilesystemModel::renameItem(const std::string& path, const std::string& new_name) {
  if (_read_only) return "";

  try {
    fs::path old_path(path);
    fs::path new_path = old_path.parent_path() / new_name;

    if (fs::exists(new_path)) {
      return "";  // Name collision
    }

    fs::rename(old_path, new_path);
    notifyModelChanged();
    return new_path.string();
  } catch (const fs::filesystem_error&) {
    return "";
  }
}

lev2::image_provider_ptr_t LocalFilesystemModel::getThumbnailProvider(const std::string& path, int size) {
  if (!hasThumbnail(path)) {
    return nullptr;
  }

  auto provider = std::make_shared<lev2::ImageProvider>();
  provider->_func = [path, size]() -> lev2::image_ptr_t {
    auto image = std::make_shared<lev2::Image>();
    if (image->readFromFile(ork::file::Path(path))) {
      // Resize if needed
      if ((int)image->_width > size || (int)image->_height > size) {
        float scale = std::min((float)size / image->_width, (float)size / image->_height);
        int new_w = (int)(image->_width * scale);
        int new_h = (int)(image->_height * scale);
        auto resized = std::make_shared<lev2::Image>();
        resized->resizedOf(*image, new_w, new_h);
        return resized;
      }
      return image;
    }
    return nullptr;
  };

  return provider;
}

bool LocalFilesystemModel::hasThumbnail(const std::string& path) const {
  try {
    if (!fs::is_regular_file(path)) {
      return false;
    }

    fs::path p(path);
    if (!p.has_extension()) {
      return false;
    }

    std::string ext = p.extension().string();
    if (!ext.empty() && ext[0] == '.') {
      ext = ext.substr(1);
    }
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return _isImageFile(ext);
  } catch (...) {
    return false;
  }
}

std::string LocalFilesystemModel::_detectMimeType(const std::string& path) {
  fs::path p(path);
  if (!p.has_extension()) {
    return "application/octet-stream";
  }

  std::string ext = p.extension().string();
  if (!ext.empty() && ext[0] == '.') {
    ext = ext.substr(1);
  }
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  // Common MIME types
  static const std::unordered_map<std::string, std::string> mime_types = {
      // Images
      {"png", "image/png"},
      {"jpg", "image/jpeg"},
      {"jpeg", "image/jpeg"},
      {"gif", "image/gif"},
      {"bmp", "image/bmp"},
      {"tga", "image/x-tga"},
      {"tiff", "image/tiff"},
      {"tif", "image/tiff"},
      {"webp", "image/webp"},
      {"exr", "image/x-exr"},
      {"hdr", "image/vnd.radiance"},
      {"dds", "image/vnd-ms.dds"},

      // 3D
      {"obj", "model/obj"},
      {"fbx", "model/fbx"},
      {"gltf", "model/gltf+json"},
      {"glb", "model/gltf-binary"},

      // Audio
      {"wav", "audio/wav"},
      {"mp3", "audio/mpeg"},
      {"ogg", "audio/ogg"},
      {"flac", "audio/flac"},

      // Video
      {"mp4", "video/mp4"},
      {"mov", "video/quicktime"},
      {"avi", "video/x-msvideo"},
      {"webm", "video/webm"},

      // Text
      {"txt", "text/plain"},
      {"json", "application/json"},
      {"xml", "application/xml"},
      {"yaml", "application/x-yaml"},
      {"yml", "application/x-yaml"},
      {"py", "text/x-python"},
      {"cpp", "text/x-c++src"},
      {"h", "text/x-chdr"},
      {"hpp", "text/x-c++hdr"},

      // Archives
      {"zip", "application/zip"},
      {"tar", "application/x-tar"},
      {"gz", "application/gzip"},
  };

  auto it = mime_types.find(ext);
  if (it != mime_types.end()) {
    return it->second;
  }

  return "application/octet-stream";
}

bool LocalFilesystemModel::_isImageFile(const std::string& extension) {
  static const std::unordered_set<std::string> image_extensions = {
      "png", "jpg", "jpeg", "gif", "bmp", "tga", "tiff", "tif", "webp", "exr", "hdr", "dds"};
  return image_extensions.count(extension) > 0;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
