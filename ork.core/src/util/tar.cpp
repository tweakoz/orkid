////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/tar.h>
#include <ork/kernel/string/deco.inl>
#include <ork/file/file.h>
#include <ork/kernel/timer.h>
#include <ork/util/logger.h>

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <climits>
#include <algorithm>
#include <boost/filesystem.hpp>

namespace ork::util {

static logchannel_ptr_t logchan_tar = logger()->configureChannel("TAR", fvec3(0.0f, 1.0f, 1.0f), false);

////////////////////////////////////////////////////////////////////////////////
// Internal implementation using libarchive
////////////////////////////////////////////////////////////////////////////////

struct TarArchive_Impl {
  datablock_ptr_t archive_data; // Raw tar archive data
  tar_entry_map_t entries;      // Cached entry information
  std::string last_error;       // Last error message
  bool is_valid = false;        // Archive validity flag

  // Internal helpers
  bool loadFromData(datablock_ptr_t data);
  bool createFromEntries(const tar_entry_map_t& entries, const TarCreateOptions& options);
  static std::string formatArchiveError(struct archive* a, const std::string& operation);
};

////////////////////////////////////////////////////////////////////////////////
// TarArchive Implementation
////////////////////////////////////////////////////////////////////////////////

TarArchive::TarArchive(){
  _impl.makeShared<TarArchive_Impl>();
}

TarArchive::~TarArchive() = default;

////////////////////////////////////////////////////////////////////////////////
// Archive Creation
////////////////////////////////////////////////////////////////////////////////

tararchive_ptr_t TarArchive::createFromDirectory(const file::Path& source_directory, const TarCreateOptions& options) {

  if (!source_directory.doesPathExist()) {
    return nullptr;
  }

  // Collect all files in directory using boost::filesystem
  std::vector<file::Path> file_paths;

  try {
    boost::filesystem::path boost_path(source_directory.c_str());

    if (boost::filesystem::is_directory(boost_path)) {
      boost::filesystem::recursive_directory_iterator iter(boost_path);
      boost::filesystem::recursive_directory_iterator end;

      for (; iter != end; ++iter) {
        if (boost::filesystem::is_regular_file(iter->status())) {
          file::Path file_path(iter->path().string().c_str());
          file_paths.push_back(file_path);
        }
      }
      if (options._deterministic) {
        std::sort(file_paths.begin(), file_paths.end(), [](const file::Path& a, const file::Path& b) {
          return std::string(a.c_str()) < std::string(b.c_str());
        });
      }
    }
  } catch (const boost::filesystem::filesystem_error& ex) {
    // Directory traversal failed
    return nullptr;
  }

  return createFromFiles(file_paths, options);
}

static time_t roundDownToNHourBlock(time_t timestamp, size_t hrquant ) {
  time_t n_hrs = hrquant * 60 * 60; // N hours in seconds
  return (timestamp / n_hrs) * n_hrs;
}

tararchive_ptr_t TarArchive::createFromFiles(const std::vector<file::Path>& file_paths, const TarCreateOptions& options) {

  tar_entry_map_t entries;

  auto sorted_file_paths = file_paths;
  if (options._deterministic) {
    std::sort(sorted_file_paths.begin(), sorted_file_paths.end(), [](const file::Path& a, const file::Path& b) {
      return std::string(a.c_str()) < std::string(b.c_str());
    });
  }

  // Load each file into memory
  for (const auto& file_path : sorted_file_paths) {
    if (!file_path.doesPathExist()) {
      continue;
    }

    // Apply filters
    std::string path_str = file_path.c_str();
    if (options.include_filter && !options.include_filter(path_str)) {
      continue;
    }
    if (options.exclude_filter && options.exclude_filter(path_str)) {
      continue;
    }

    // Create entry
    auto entry  = std::make_shared<TarEntry>();
    entry->name = path_str;

    // Strip base path if provided
    if (!options.base_path.empty()) {
      if (entry->name.find(options.base_path) == 0) {
        entry->name = entry->name.substr(options.base_path.length());
        // Remove leading slash
        if (!entry->name.empty() && entry->name[0] == '/') {
          entry->name = entry->name.substr(1);
        }
      }
    }

    auto datablock = datablockFromFileAtPath(file_path);

    if (datablock==nullptr) {
      continue;
    }

    size_t file_size = datablock->length();

    if(0)printf("[TARX] Adding file: %s (as %s) file_size<%zu> hash<0x%llx>\n", file_path.c_str(), entry->name.c_str(), file_size, datablock->hash());

    entry->data = datablock;
    entry->size = file_size;
    entry->mode = options.default_mode;
    if (options._deterministic) {
      entry->mtime = roundDownToNHourBlock(time(nullptr),4);
    } else {
      entry->mtime = time(nullptr);
    }
    entry->is_directory = false;

    entries[entry->name] = entry;

    // Progress callback
    if (options.progress) {
      options.progress(entry->name, file_size, file_size);
    }
  }

  return createFromMemory(entries, options);
}

tararchive_ptr_t TarArchive::createFromMemory(const tar_entry_map_t& entries, const TarCreateOptions& options) {

  auto archive = std::make_shared<TarArchive>();
  auto impl = archive->_impl.getShared<TarArchive_Impl>();
  if (!impl->createFromEntries(entries, options)) {
    return nullptr;
  }

  return archive;
}

////////////////////////////////////////////////////////////////////////////////
// Archive Extraction
////////////////////////////////////////////////////////////////////////////////

bool TarArchive::extractToDirectory(const file::Path& output_directory, const TarExtractOptions& options) const {
  auto impl = _impl.getShared<TarArchive_Impl>();
  if (!impl->is_valid || !impl->archive_data) {
    return false;
  }

  // Ensure output directory exists
  if (!output_directory.doesPathExist()) {
    try {
      boost::filesystem::create_directories(output_directory.c_str());
    } catch (const boost::filesystem::filesystem_error&) {
      return false;
    }
  }

  // Extract each entry
  for (const auto& [name, entry] : impl->entries) {
    // Apply filters
    if (options.include_filter && !options.include_filter(name)) {
      continue;
    }
    if (options.exclude_filter && options.exclude_filter(name)) {
      continue;
    }

    auto output_path = output_directory / name;

    // Check if file exists and overwrite policy
    if (output_path.doesPathExist() && !options.overwrite_existing) {
      continue;
    }

    // Create directory structure if needed
    auto parent_dir = output_path;
    parent_dir.setFile(""); // Get directory part
    if (!parent_dir.doesPathExist()) {
      try {
        boost::filesystem::create_directories(parent_dir.c_str());
      } catch (const boost::filesystem::filesystem_error&) {
        // Continue - might still work if directory exists now
      }
    }

    // Write file
    if (entry->data && !entry->is_directory) {
      File output_file(output_path, EFM_WRITE);
      if (output_file.IsOpen() && entry->data->length() > 0) {
        output_file.Write(entry->data->data(), entry->data->length());
      }
    }

    // Progress callback
    if (options.progress) {
      options.progress(name, entry->size, entry->size);
    }
  }

  return true;
}

tar_entry_map_t TarArchive::extractToMemory(const TarExtractOptions& options) const {
  tar_entry_map_t result;
  auto impl = _impl.getShared<TarArchive_Impl>();

  if (!impl->is_valid) {
    return result;
  }

  // Apply filters and copy entries
  for (const auto& [name, entry] : impl->entries) {
    if (options.include_filter && !options.include_filter(name)) {
      continue;
    }
    if (options.exclude_filter && options.exclude_filter(name)) {
      continue;
    }

    result[name] = entry;

    // Progress callback
    if (options.progress) {
      options.progress(name, entry->size, entry->size);
    }
  }

  return result;
}

datablock_ptr_t TarArchive::extractFile(const std::string& filename) const {
  auto impl = _impl.getShared<TarArchive_Impl>();
  logchan_tar->log("extractFile: looking for '%s'", filename.c_str());
  if (!impl->is_valid) {
    logchan_tar->log("extractFile: archive not valid");
    return nullptr;
  }

  logchan_tar->log("extractFile: archive has %zu entries", impl->entries.size());
  auto it = impl->entries.find(filename);
  if (it != impl->entries.end()) {
    logchan_tar->log(
        "extractFile: found entry, data=%p, size=%zu", it->second->data.get(), it->second->data ? it->second->data->length() : 0);
    return it->second->data;
  }

  logchan_tar->log("extractFile: entry not found");
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Archive Information
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> TarArchive::listEntries() const {
  std::vector<std::string> result;
  auto impl = _impl.getShared<TarArchive_Impl>();
  for (const auto& [name, entry] : impl->entries) {
    result.push_back(name);
  }

  std::sort(result.begin(), result.end());
  return result;
}

tarentry_ptr_t TarArchive::getEntryInfo(const std::string& filename) const {
  auto impl = _impl.getShared<TarArchive_Impl>();
  auto it = impl->entries.find(filename);
  if (it != impl->entries.end()) {
    return it->second;
  }
  return nullptr;
}

bool TarArchive::hasFile(const std::string& filename) const {
  auto impl = _impl.getShared<TarArchive_Impl>();
  return impl->entries.find(filename) != impl->entries.end();
}

size_t TarArchive::getArchiveSize() const {
  auto impl = _impl.getShared<TarArchive_Impl>();
  return impl->archive_data ? impl->archive_data->length() : 0;
}

size_t TarArchive::getTotalSize() const {
  size_t total = 0;
  auto impl = _impl.getShared<TarArchive_Impl>();
  for (const auto& [name, entry] : impl->entries) {
    total += entry->size;
  }
  return total;
}

////////////////////////////////////////////////////////////////////////////////
// Serialization
////////////////////////////////////////////////////////////////////////////////

bool TarArchive::saveToFile(const file::Path& tar_file) const {
  logchan_tar->log("saveToFile: starting, path='%s'", tar_file.c_str());
  auto impl = _impl.getShared<TarArchive_Impl>();

  if (!impl->is_valid) {
    logchan_tar->log("saveToFile: archive not valid");
    return false;
  }

  if (!impl->archive_data) {
    logchan_tar->log("saveToFile: no archive data");
    return false;
  }

  logchan_tar->log("saveToFile: archive_data size=%zu", impl->archive_data->length());

  File file(tar_file, EFM_WRITE);
  if (!file.IsOpen()) {
    logchan_tar->log("saveToFile: failed to open file for writing");
    return false;
  }

  logchan_tar->log("saveToFile: file opened successfully");

  EFileErrCode write_result = file.Write(impl->archive_data->data(), impl->archive_data->length());
  logchan_tar->log("saveToFile: Write() returned error code: %d", (int)write_result);

  // Force close and check file exists
  file.Close();
  logchan_tar->log("saveToFile: file closed, checking if file exists: %s", tar_file.doesPathExist() ? "YES" : "NO");

  if (tar_file.doesPathExist()) {
    // Check file size
    File check_file(tar_file, EFM_READ);
    if (check_file.IsOpen()) {
      size_t file_size = 0;
      check_file.GetLength(file_size);
      logchan_tar->log("saveToFile: actual file size on disk: %zu bytes", file_size);

      // Success if write returned no error and file size matches
      bool success = (write_result == 0) && (file_size == impl->archive_data->length());
      logchan_tar->log("saveToFile: %s", success ? "SUCCESS" : "FAILED");
      return success;
    }
  }

  logchan_tar->log("saveToFile: FAILED - could not verify file");
  return false;
}

tararchive_ptr_t TarArchive::loadFromFile(const file::Path& tar_file) {
  if (!tar_file.doesPathExist()) {
    return nullptr;
  }

  File file(tar_file, EFM_READ);
  if (!file.IsOpen()) {
    return nullptr;
  }

  size_t file_size = 0;
  file.GetLength(file_size);

  auto data = std::make_shared<DataBlock>();
  data->reserve(file_size);
  data->_storage.resize(file_size);

  if (file_size > 0) {
    file.Read(const_cast<uint8_t*>(data->data()), file_size);
  }

  return loadFromMemory(data);
}

datablock_ptr_t TarArchive::getArchiveData() const {
  auto impl = _impl.getShared<TarArchive_Impl>();
  return impl->archive_data;
}

tararchive_ptr_t TarArchive::loadFromMemory(datablock_ptr_t data) {
  if (!data || data->length() == 0) {
    return nullptr;
  }
  auto archive = std::make_shared<TarArchive>();
  auto impl = archive->_impl.getShared<TarArchive_Impl>();
  if (!impl->loadFromData(data)) {
    return nullptr;
  }
  return archive;
}

////////////////////////////////////////////////////////////////////////////////
// Error Handling
////////////////////////////////////////////////////////////////////////////////

std::string TarArchive::getLastError() const {
  auto impl = _impl.getShared<TarArchive_Impl>();
  return impl->last_error;
}

bool TarArchive::isValid() const {
  auto impl = _impl.getShared<TarArchive_Impl>();
  return impl->is_valid;
}

////////////////////////////////////////////////////////////////////////////////
// Impl Helper Methods
////////////////////////////////////////////////////////////////////////////////

bool TarArchive_Impl::loadFromData(datablock_ptr_t data) {
  archive_data = data;
  entries.clear();
  last_error.clear();
  is_valid = false;

  logchan_tar->log("loadFromData: starting with %zu bytes", data ? data->length() : 0);

  if (!data || data->length() == 0) {
    last_error = "No data provided";
    logchan_tar->log("loadFromData: no data provided");
    return false;
  }

  // Create archive reader
  struct archive* a = archive_read_new();
  archive_read_support_format_tar(a);
  archive_read_support_format_gnutar(a);

  // Open from memory
  int r = archive_read_open_memory(a, data->data(), data->length());
  if (r != ARCHIVE_OK) {
    last_error = formatArchiveError(a, "Failed to open archive from memory");
    logchan_tar->log("loadFromData: archive_read_open_memory failed: %s", archive_error_string(a));
    archive_read_free(a);
    return false;
  }

  logchan_tar->log("loadFromData: reading tar entries");

  // Read entries
  struct archive_entry* ae;
  int entry_count = 0;
  while (archive_read_next_header(a, &ae) == ARCHIVE_OK) {
    std::string entry_name = archive_entry_pathname(ae);
    logchan_tar->log("loadFromData: found entry '%s'", entry_name.c_str());

    auto entry          = std::make_shared<TarEntry>();
    entry->name         = entry_name;
    entry->size         = archive_entry_size(ae);
    entry->mtime        = archive_entry_mtime(ae);
    entry->mode         = archive_entry_mode(ae);
    entry->is_directory = archive_entry_filetype(ae) == AE_IFDIR;

    logchan_tar->log("loadFromData: entry size=%zu, is_dir=%d", entry->size, entry->is_directory);

    // Read file data if not a directory
    if (!entry->is_directory && entry->size > 0) {
      logchan_tar->log("loadFromData: reading %zu bytes of data for '%s'", entry->size, entry_name.c_str());
      entry->data = std::make_shared<DataBlock>();
      entry->data->reserve(entry->size);
      entry->data->_storage.resize(entry->size);

      // Read file data
      la_ssize_t bytes_read = archive_read_data(a,
                                                 const_cast<uint8_t*>(entry->data->data()),
                                                 entry->size);
      if (bytes_read < 0) {
        last_error = formatArchiveError(a, "Failed to read file data: " + entry_name);
        logchan_tar->log("loadFromData: archive_read_data failed: %s", archive_error_string(a));
        archive_read_free(a);
        return false;
      }
      if (static_cast<size_t>(bytes_read) != entry->size) {
        logchan_tar->log("loadFromData: warning - read %zd bytes, expected %zu", bytes_read, entry->size);
      }

      logchan_tar->log("loadFromData: read %zd bytes for '%s'", bytes_read, entry_name.c_str());
    }

    entries[entry_name] = entry;
    entry_count++;
  }

  logchan_tar->log("loadFromData: loaded %d entries total", entry_count);

  // Clean up
  archive_read_free(a);

  is_valid = true;
  logchan_tar->log("loadFromData: success");
  return true;
}

bool TarArchive_Impl::createFromEntries(const tar_entry_map_t& entries_input, const TarCreateOptions& options) {
  entries = entries_input;
  last_error.clear();
  is_valid = false;

  logchan_tar->log("createFromEntries: starting with %zu entries", entries.size());

  if (entries.empty()) {
    last_error = "No entries provided";
    logchan_tar->log("createFromEntries: no entries provided");
    return false;
  }

  // Create archive writer to memory
  struct archive* a = archive_write_new();
  archive_write_set_format_pax(a);  // PAX format supports large files (>8GB)

  // We'll write to a growing buffer
  std::vector<uint8_t> buffer;
  buffer.reserve(1024 * 1024);  // Start with 1MB

  // Use callback-based writing to memory
  archive_write_open(a, &buffer,
    // open callback
    [](struct archive*, void*) -> int { return ARCHIVE_OK; },
    // write callback
    [](struct archive*, void* client_data, const void* buff, size_t length) -> la_ssize_t {
      auto* vec = static_cast<std::vector<uint8_t>*>(client_data);
      const uint8_t* src = static_cast<const uint8_t*>(buff);
      vec->insert(vec->end(), src, src + length);
      return static_cast<la_ssize_t>(length);
    },
    // close callback
    [](struct archive*, void*) -> int { return ARCHIVE_OK; }
  );

  // Add each entry to the archive
  logchan_tar->log("createFromEntries: adding entries to archive");

  // Create a sorted vector of entries for deterministic processing
  std::vector<std::pair<std::string, tarentry_ptr_t>> sorted_entries;
  for (const auto& entry_pair : entries) {
    sorted_entries.push_back(entry_pair);
  }

  if (options._deterministic) {
    std::sort(sorted_entries.begin(), sorted_entries.end(), [](const auto& a, const auto& b) {
      return a.first < b.first; // Sort by entry name
    });
  }

  for (const auto& [name, entry] : sorted_entries) {
    logchan_tar->log("createFromEntries: processing entry '%s', size=%zu", name.c_str(), entry->size);

    struct archive_entry* ae = archive_entry_new();

    // Set entry metadata
    archive_entry_set_pathname(ae, entry->name.c_str());
    archive_entry_set_size(ae, entry->size);
    archive_entry_set_mtime(ae, entry->mtime, 0);
    archive_entry_set_perm(ae, entry->mode & 0777);

    if (entry->is_directory) {
      archive_entry_set_filetype(ae, AE_IFDIR);
    } else {
      archive_entry_set_filetype(ae, AE_IFREG);
    }

    // Set deterministic ownership if requested
    if (options._deterministic) {
      archive_entry_set_uid(ae, options._fixed_uid);
      archive_entry_set_gid(ae, options._fixed_gid);
      archive_entry_set_uname(ae, "root");
      archive_entry_set_gname(ae, "root");
    }

    // Write header
    int r = archive_write_header(a, ae);
    if (r != ARCHIVE_OK) {
      last_error = formatArchiveError(a, "Failed to write header for: " + entry->name);
      logchan_tar->log("createFromEntries: archive_write_header failed: %s", archive_error_string(a));
      archive_entry_free(ae);
      archive_write_free(a);
      return false;
    }

    // Write file data if not a directory and has data
    if (!entry->is_directory && entry->data && entry->data->length() > 0) {
      // Write in chunks to handle large files (>2GB)
      const size_t chunk_size = 128 * 1024 * 1024; // 128MB chunks
      const uint8_t* data_ptr = entry->data->data();
      size_t remaining = entry->data->length();
      size_t total_written = 0;

      while (remaining > 0) {
        size_t to_write = std::min(remaining, chunk_size);
        la_ssize_t written = archive_write_data(a, data_ptr + total_written, to_write);
        if (written < 0 || static_cast<size_t>(written) != to_write) {
          last_error = formatArchiveError(a, "Failed to write data for: " + entry->name);
          logchan_tar->log("createFromEntries: archive_write_data failed: %s", archive_error_string(a));
          archive_entry_free(ae);
          archive_write_free(a);
          return false;
        }
        total_written += written;
        remaining -= written;
      }
    }

    archive_entry_free(ae);

    // Progress callback
    if (options.progress) {
      options.progress(entry->name, entry->size, entry->size);
    }
  }

  // Close archive
  archive_write_close(a);
  archive_write_free(a);

  // Copy buffer to archive_data
  logchan_tar->log("createFromEntries: created archive size: %zu bytes", buffer.size());
  archive_data = std::make_shared<DataBlock>();
  archive_data->reserve(buffer.size());
  archive_data->addData(buffer.data(), buffer.size());

  is_valid = true;
  logchan_tar->log("createFromEntries: success");
  return true;
}

std::string TarArchive_Impl::formatArchiveError(struct archive* a, const std::string& operation) {
  const char* err = archive_error_string(a);
  return FormatString("Tar operation '%s' failed: %s", operation.c_str(), err ? err : "unknown error");
}

////////////////////////////////////////////////////////////////////////////////
// Utility Functions
////////////////////////////////////////////////////////////////////////////////

tararchive_ptr_t
createTarFromSingleFile(const file::Path& file_path, const std::string& archive_name, const TarCreateOptions& options) {

  std::vector<file::Path> files = {file_path};
  return TarArchive::createFromFiles(files, options);
}

datablock_ptr_t extractSingleFileFromTar(const file::Path& tar_file, const std::string& filename) {

  auto archive = TarArchive::loadFromFile(tar_file);
  if (!archive) {
    return nullptr;
  }

  return archive->extractFile(filename);
}

std::vector<std::string> listTarContents(const file::Path& tar_file) {
  auto archive = TarArchive::loadFromFile(tar_file);
  if (!archive) {
    return {};
  }

  return archive->listEntries();
}

size_t getTarFileSize(const file::Path& tar_file) {
  auto archive = TarArchive::loadFromFile(tar_file);
  if (!archive) {
    return 0;
  }

  return archive->getArchiveSize();
}

} // namespace ork::util