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

#include <libtar.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>
#include <boost/filesystem.hpp>

namespace ork::util {

static logchannel_ptr_t logchan_tar = logger()->configureChannel("TAR", fvec3(0.0f, 1.0f, 1.0f), false);

////////////////////////////////////////////////////////////////////////////////
// Internal implementation using libtar
////////////////////////////////////////////////////////////////////////////////

struct TarArchive::Impl {
    datablock_ptr_t archive_data;           // Raw tar archive data
    tar_entry_map_t entries;                // Cached entry information
    std::string last_error;                 // Last error message
    bool is_valid = false;                  // Archive validity flag
    
    // Internal helpers
    bool loadFromData(datablock_ptr_t data);
    bool createFromEntries(const tar_entry_map_t& entries, const TarCreateOptions& options);
    static std::string formatLibtarError(const std::string& operation);
};

////////////////////////////////////////////////////////////////////////////////
// TarArchive Implementation
////////////////////////////////////////////////////////////////////////////////

TarArchive::TarArchive() : _impl(std::make_unique<Impl>()) {
}

TarArchive::TarArchive(std::unique_ptr<Impl> impl) : _impl(std::move(impl)) {
}

TarArchive::~TarArchive() = default;

////////////////////////////////////////////////////////////////////////////////
// Archive Creation
////////////////////////////////////////////////////////////////////////////////

tararchive_ptr_t TarArchive::createFromDirectory(
    const file::Path& source_directory,
    const TarCreateOptions& options) {
    
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
        }
    } catch (const boost::filesystem::filesystem_error& ex) {
        // Directory traversal failed
        return nullptr;
    }
    
    return createFromFiles(file_paths, options);
}

tararchive_ptr_t TarArchive::createFromFiles(
    const std::vector<file::Path>& file_paths,
    const TarCreateOptions& options) {
    
    tar_entry_map_t entries;
    
    // Load each file into memory
    for (const auto& file_path : file_paths) {
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
        auto entry = std::make_shared<TarEntry>();
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
        
        // Load file data
        File file(file_path, EFM_READ);
        if (!file.IsOpen()) {
            continue;
        }
        
        size_t file_size = 0;
        file.GetLength(file_size);
        
        entry->data = std::make_shared<DataBlock>();
        entry->data->reserve(file_size);
        entry->data->_storage.resize(file_size);
        
        if (file_size > 0) {
            file.Read(const_cast<uint8_t*>(entry->data->data()), file_size);
        }
        
        entry->size = file_size;
        entry->mode = options.default_mode;
        entry->mtime = time(nullptr);
        entry->is_directory = false;
        
        entries[entry->name] = entry;
        
        // Progress callback
        if (options.progress) {
            options.progress(entry->name, file_size, file_size);
        }
    }
    
    return createFromMemory(entries, options);
}

tararchive_ptr_t TarArchive::createFromMemory(
    const tar_entry_map_t& entries,
    const TarCreateOptions& options) {
    
    auto archive = std::make_shared<TarArchive>();
    
    if (!archive->_impl->createFromEntries(entries, options)) {
        return nullptr;
    }
    
    return archive;
}

////////////////////////////////////////////////////////////////////////////////
// Archive Extraction
////////////////////////////////////////////////////////////////////////////////

bool TarArchive::extractToDirectory(
    const file::Path& output_directory,
    const TarExtractOptions& options) const {
    
    if (!_impl->is_valid || !_impl->archive_data) {
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
    for (const auto& [name, entry] : _impl->entries) {
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
    
    if (!_impl->is_valid) {
        return result;
    }
    
    // Apply filters and copy entries
    for (const auto& [name, entry] : _impl->entries) {
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
    logchan_tar->log("extractFile: looking for '%s'", filename.c_str());
    if (!_impl->is_valid) {
        logchan_tar->log("extractFile: archive not valid");
        return nullptr;
    }
    
    logchan_tar->log("extractFile: archive has %zu entries", _impl->entries.size());
    auto it = _impl->entries.find(filename);
    if (it != _impl->entries.end()) {
        logchan_tar->log("extractFile: found entry, data=%p, size=%zu", it->second->data.get(), it->second->data ? it->second->data->length() : 0);
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
    
    for (const auto& [name, entry] : _impl->entries) {
        result.push_back(name);
    }
    
    std::sort(result.begin(), result.end());
    return result;
}

tarentry_ptr_t TarArchive::getEntryInfo(const std::string& filename) const {
    auto it = _impl->entries.find(filename);
    if (it != _impl->entries.end()) {
        return it->second;
    }
    return nullptr;
}

bool TarArchive::hasFile(const std::string& filename) const {
    return _impl->entries.find(filename) != _impl->entries.end();
}

size_t TarArchive::getArchiveSize() const {
    return _impl->archive_data ? _impl->archive_data->length() : 0;
}

size_t TarArchive::getTotalSize() const {
    size_t total = 0;
    for (const auto& [name, entry] : _impl->entries) {
        total += entry->size;
    }
    return total;
}

////////////////////////////////////////////////////////////////////////////////
// Serialization
////////////////////////////////////////////////////////////////////////////////

bool TarArchive::saveToFile(const file::Path& tar_file) const {
    logchan_tar->log("saveToFile: starting, path='%s'", tar_file.c_str());
    
    if (!_impl->is_valid) {
        logchan_tar->log("saveToFile: archive not valid");
        return false;
    }
    
    if (!_impl->archive_data) {
        logchan_tar->log("saveToFile: no archive data");
        return false;
    }
    
    logchan_tar->log("saveToFile: archive_data size=%zu", _impl->archive_data->length());
    
    File file(tar_file, EFM_WRITE);
    if (!file.IsOpen()) {
        logchan_tar->log("saveToFile: failed to open file for writing");
        return false;
    }
    
    logchan_tar->log("saveToFile: file opened successfully");
    
    EFileErrCode write_result = file.Write(_impl->archive_data->data(), _impl->archive_data->length());
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
            bool success = (write_result == 0) && (file_size == _impl->archive_data->length());
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
    return _impl->archive_data;
}

tararchive_ptr_t TarArchive::loadFromMemory(datablock_ptr_t data) {
    if (!data || data->length() == 0) {
        return nullptr;
    }
    
    auto impl = std::make_unique<Impl>();
    if (!impl->loadFromData(data)) {
        return nullptr;
    }
    
    return std::shared_ptr<TarArchive>(new TarArchive(std::move(impl)));
}

////////////////////////////////////////////////////////////////////////////////
// Error Handling
////////////////////////////////////////////////////////////////////////////////

std::string TarArchive::getLastError() const {
    return _impl->last_error;
}

bool TarArchive::isValid() const {
    return _impl->is_valid;
}

////////////////////////////////////////////////////////////////////////////////
// Impl Helper Methods
////////////////////////////////////////////////////////////////////////////////

bool TarArchive::Impl::loadFromData(datablock_ptr_t data) {
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
    
    // Create a temporary file from memory for libtar
    char temp_template[] = "/tmp/orktar_XXXXXX";
    int temp_fd = mkstemp(temp_template);
    logchan_tar->log("loadFromData: created temp file %s, fd=%d", temp_template, temp_fd);
    if (temp_fd == -1) {
        last_error = formatLibtarError("Failed to create temporary file");
        logchan_tar->log("loadFromData: failed to create temp file");
        return false;
    }
    
    // Write data to temp file
    ssize_t written = write(temp_fd, data->data(), data->length());
    logchan_tar->log("loadFromData: wrote %zd bytes (expected %zu)", written, data->length());
    if (written != static_cast<ssize_t>(data->length())) {
        close(temp_fd);
        unlink(temp_template);
        last_error = formatLibtarError("Failed to write data to temporary file");
        logchan_tar->log("loadFromData: write failed");
        return false;
    }
    
    // Rewind to beginning
    lseek(temp_fd, 0, SEEK_SET);
    
    // Open tar archive
    TAR* tar_handle = nullptr;
    logchan_tar->log("loadFromData: opening tar archive with libtar");
    if (tar_fdopen(&tar_handle, temp_fd, temp_template, nullptr, O_RDONLY, 0644, TAR_GNU) != 0) {
        close(temp_fd);
        unlink(temp_template);
        last_error = formatLibtarError("Failed to open tar archive");
        logchan_tar->log("loadFromData: tar_fdopen failed");
        return false;
    }
    
    // Read entries
    logchan_tar->log("loadFromData: reading tar entries");
    int entry_count = 0;
    while (th_read(tar_handle) == 0) {
        std::string entry_name = th_get_pathname(tar_handle);
        logchan_tar->log("loadFromData: found entry '%s'", entry_name.c_str());
        
        auto entry = std::make_shared<TarEntry>();
        entry->name = entry_name;
        entry->size = th_get_size(tar_handle);
        entry->mtime = th_get_mtime(tar_handle);
        entry->mode = th_get_mode(tar_handle);
        entry->is_directory = TH_ISDIR(tar_handle);
        
        logchan_tar->log("loadFromData: entry size=%zu, is_dir=%d", entry->size, entry->is_directory);
        
        // Read file data if not a directory
        if (!entry->is_directory && entry->size > 0) {
            logchan_tar->log("loadFromData: reading %zu bytes of data for '%s'", entry->size, entry_name.c_str());
            entry->data = std::make_shared<DataBlock>();
            entry->data->reserve(entry->size);
            entry->data->_storage.resize(entry->size);
            
            // Read file data directly from tar stream
            size_t bytes_to_read = entry->size;
            size_t bytes_read = 0;
            char* data_ptr = reinterpret_cast<char*>(const_cast<uint8_t*>(entry->data->data()));
            
            while (bytes_read < bytes_to_read) {
                ssize_t chunk_size = read(tar_handle->fd, data_ptr + bytes_read, bytes_to_read - bytes_read);
                logchan_tar->log("loadFromData: read chunk %zd bytes (total %zu/%zu)", chunk_size, bytes_read + chunk_size, bytes_to_read);
                if (chunk_size <= 0) {
                    tar_close(tar_handle);
                    unlink(temp_template);
                    last_error = formatLibtarError("Failed to read file data: " + entry_name);
                    logchan_tar->log("loadFromData: read failed");
                    return false;
                }
                bytes_read += chunk_size;
            }
            
            // Debug: print first few bytes of read data
            std::string preview;
            for (size_t i = 0; i < std::min(entry->size, (size_t)15); i++) {
                preview += data_ptr[i];
            }
            logchan_tar->log("loadFromData: first 15 bytes: '%s'", preview.c_str());
            
            // Skip padding to next 512-byte boundary
            size_t padding = (512 - (entry->size % 512)) % 512;
            if (padding > 0) {
                logchan_tar->log("loadFromData: skipping %zu padding bytes", padding);
                lseek(tar_handle->fd, padding, SEEK_CUR);
            }
        }
        
        entries[entry_name] = entry;
        entry_count++;
    }
    
    logchan_tar->log("loadFromData: loaded %d entries total", entry_count);
    
    // Clean up
    tar_close(tar_handle);
    unlink(temp_template);
    
    is_valid = true;
    logchan_tar->log("loadFromData: success");
    return true;
}

bool TarArchive::Impl::createFromEntries(const tar_entry_map_t& entries_input, const TarCreateOptions& options) {
    entries = entries_input;
    last_error.clear();
    is_valid = false;
    
    logchan_tar->log("createFromEntries: starting with %zu entries", entries.size());
    
    if (entries.empty()) {
        last_error = "No entries provided";
        logchan_tar->log("createFromEntries: no entries provided");
        return false;
    }
    
    // Create a temporary file for tar creation
    char temp_template[] = "/tmp/orktar_create_XXXXXX";
    int temp_fd = mkstemp(temp_template);
    if (temp_fd == -1) {
        last_error = formatLibtarError("Failed to create temporary file");
        return false;
    }
    
    // Open tar archive for writing
    TAR* tar_handle = nullptr;
    if (tar_fdopen(&tar_handle, temp_fd, temp_template, nullptr, O_WRONLY | O_CREAT | O_TRUNC, 0644, TAR_GNU) != 0) {
        close(temp_fd);
        unlink(temp_template);
        last_error = formatLibtarError("Failed to open tar archive for writing");
        return false;
    }
    
    // Add each entry to the archive
    logchan_tar->log("createFromEntries: adding entries to archive");
    for (const auto& [name, entry] : entries) {
        logchan_tar->log("createFromEntries: processing entry '%s', size=%zu", name.c_str(), entry->size);
        
        // Set header information
        th_set_type(tar_handle, entry->is_directory ? DIRTYPE : REGTYPE);
        th_set_path(tar_handle, const_cast<char*>(entry->name.c_str()));
        th_set_mode(tar_handle, entry->mode);
        th_set_size(tar_handle, entry->size);
        th_set_mtime(tar_handle, entry->mtime);
        
        // Write header
        if (th_write(tar_handle) != 0) {
            tar_close(tar_handle);
            unlink(temp_template);
            last_error = formatLibtarError("Failed to write header for: " + entry->name);
            return false;
        }
        
        // Write file data if not a directory and has data
        if (!entry->is_directory && entry->data && entry->data->length() > 0) {
            size_t remaining = entry->data->length();
            const uint8_t* data_ptr = entry->data->data();
            
            while (remaining > 0) {
                ssize_t written = write(tar_handle->fd, data_ptr, remaining);
                if (written <= 0) {
                    tar_close(tar_handle);
                    unlink(temp_template);
                    last_error = formatLibtarError("Failed to write data for: " + entry->name);
                    return false;
                }
                data_ptr += written;
                remaining -= written;
            }
            
            // Pad to 512-byte boundary
            size_t padding = (512 - (entry->data->length() % 512)) % 512;
            if (padding > 0) {
                char zero_buffer[512] = {0};
                if (write(tar_handle->fd, zero_buffer, padding) != static_cast<ssize_t>(padding)) {
                    tar_close(tar_handle);
                    unlink(temp_template);
                    last_error = formatLibtarError("Failed to write padding for: " + entry->name);
                    return false;
                }
            }
        }
        
        // Progress callback
        if (options.progress) {
            options.progress(entry->name, entry->size, entry->size);
        }
    }
    
    // Close archive (writes end-of-archive blocks)
    if (tar_close(tar_handle) != 0) {
        unlink(temp_template);
        last_error = formatLibtarError("Failed to close tar archive");
        return false;
    }
    
    // Read the created archive back into memory
    logchan_tar->log("createFromEntries: reading created archive back");
    File archive_file(temp_template, EFM_READ);
    if (!archive_file.IsOpen()) {
        unlink(temp_template);
        last_error = "Failed to read created archive";
        logchan_tar->log("createFromEntries: failed to open created archive");
        return false;
    }
    
    size_t archive_size = 0;
    archive_file.GetLength(archive_size);
    logchan_tar->log("createFromEntries: created archive size: %zu bytes", archive_size);
    
    archive_data = std::make_shared<DataBlock>();
    archive_data->reserve(archive_size);
    archive_data->_storage.resize(archive_size);
    
    if (archive_size > 0) {
        archive_file.Read(const_cast<uint8_t*>(archive_data->data()), archive_size);
    }
    
    // Clean up temporary file
    unlink(temp_template);
    
    is_valid = true;
    logchan_tar->log("createFromEntries: success");
    return true;
}

std::string TarArchive::Impl::formatLibtarError(const std::string& operation) {
    return FormatString("Tar operation '%s' failed", operation.c_str());
}

////////////////////////////////////////////////////////////////////////////////
// Utility Functions
////////////////////////////////////////////////////////////////////////////////

tararchive_ptr_t createTarFromSingleFile(
    const file::Path& file_path,
    const std::string& archive_name,
    const TarCreateOptions& options) {
    
    std::vector<file::Path> files = { file_path };
    return TarArchive::createFromFiles(files, options);
}

datablock_ptr_t extractSingleFileFromTar(
    const file::Path& tar_file,
    const std::string& filename) {
    
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