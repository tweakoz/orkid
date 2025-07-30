////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/kernel/datablock.h>
#include <ork/file/path.h>
#include <memory>
#include <map>
#include <functional>

namespace ork::util {

////////////////////////////////////////////////////////////////////////////////
// Tar file handling utilities using libtar
// 
// Provides high-level interface for creating and extracting tar archives
// with support for compression, encryption, and progress callbacks.
//
// Key features:
// - Create tar archives from files or memory
// - Extract tar archives to files or memory
// - Support for in-memory operations (no temp files)
// - Progress tracking for large archives
// - Error handling with detailed messages
////////////////////////////////////////////////////////////////////////////////

class TarArchive;
using tararchive_ptr_t = std::shared_ptr<TarArchive>;

////////////////////////////////////////////////////////////////////////////////
// Progress callback types
////////////////////////////////////////////////////////////////////////////////

using tar_progress_callback_t = std::function<void(const std::string& filename, size_t bytes_processed, size_t total_bytes)>;
using tar_completion_callback_t = std::function<void(bool success, const std::string& error_message)>;

////////////////////////////////////////////////////////////////////////////////
// Tar archive entry information
////////////////////////////////////////////////////////////////////////////////

struct TarEntry {
    std::string name;           // File path within archive
    size_t size = 0;           // File size in bytes
    time_t mtime = 0;          // Modification time
    mode_t mode = 0644;        // File permissions
    bool is_directory = false; // True if this is a directory entry
    
    // File content (when extracting to memory)
    datablock_ptr_t data;
};

using tarentry_ptr_t = std::shared_ptr<TarEntry>;
using tar_entry_map_t = std::map<std::string, tarentry_ptr_t>;

////////////////////////////////////////////////////////////////////////////////
// Tar archive creation options
////////////////////////////////////////////////////////////////////////////////

struct TarCreateOptions {
    bool compress = false;              // Enable gzip compression
    tar_progress_callback_t progress;   // Progress callback (optional)
    std::string base_path = "";         // Base path to strip from file paths
    mode_t default_mode = 0644;         // Default file permissions
    
    // Filters
    std::function<bool(const std::string& path)> include_filter; // Return true to include file
    std::function<bool(const std::string& path)> exclude_filter; // Return true to exclude file
};

////////////////////////////////////////////////////////////////////////////////
// Tar archive extraction options
////////////////////////////////////////////////////////////////////////////////

struct TarExtractOptions {
    file::Path output_directory;        // Where to extract files (empty = memory only)
    bool extract_to_memory = false;     // Extract to memory instead of files
    bool overwrite_existing = true;     // Overwrite existing files
    tar_progress_callback_t progress;   // Progress callback (optional)
    
    // Filters
    std::function<bool(const std::string& path)> include_filter; // Return true to extract file
    std::function<bool(const std::string& path)> exclude_filter; // Return true to skip file
};

////////////////////////////////////////////////////////////////////////////////
// Main TarArchive class
////////////////////////////////////////////////////////////////////////////////

class TarArchive {
public:
    TarArchive();
    ~TarArchive();
    
    ////////////////////////////////////////////////////////////////////////////////
    // Archive Creation
    ////////////////////////////////////////////////////////////////////////////////
    
    // Create tar archive from directory
    static tararchive_ptr_t createFromDirectory(
        const file::Path& source_directory,
        const TarCreateOptions& options = {}
    );
    
    // Create tar archive from file list
    static tararchive_ptr_t createFromFiles(
        const std::vector<file::Path>& file_paths,
        const TarCreateOptions& options = {}
    );
    
    // Create tar archive from memory buffers
    static tararchive_ptr_t createFromMemory(
        const tar_entry_map_t& entries,
        const TarCreateOptions& options = {}
    );
    
    ////////////////////////////////////////////////////////////////////////////////
    // Archive Extraction
    ////////////////////////////////////////////////////////////////////////////////
    
    // Extract to directory
    bool extractToDirectory(
        const file::Path& output_directory,
        const TarExtractOptions& options = {}
    ) const;
    
    // Extract to memory
    tar_entry_map_t extractToMemory(
        const TarExtractOptions& options = {}
    ) const;
    
    // Extract single file to memory
    datablock_ptr_t extractFile(const std::string& filename) const;
    
    ////////////////////////////////////////////////////////////////////////////////
    // Archive Information
    ////////////////////////////////////////////////////////////////////////////////
    
    // List all entries in archive
    std::vector<std::string> listEntries() const;
    
    // Get entry information
    tarentry_ptr_t getEntryInfo(const std::string& filename) const;
    
    // Check if archive contains file
    bool hasFile(const std::string& filename) const;
    
    // Get archive size
    size_t getArchiveSize() const;
    
    // Get total uncompressed size
    size_t getTotalSize() const;
    
    ////////////////////////////////////////////////////////////////////////////////
    // Serialization
    ////////////////////////////////////////////////////////////////////////////////
    
    // Save archive to file
    bool saveToFile(const file::Path& tar_file) const;
    
    // Load archive from file
    static tararchive_ptr_t loadFromFile(const file::Path& tar_file);
    
    // Get archive data as memory buffer
    datablock_ptr_t getArchiveData() const;
    
    // Load archive from memory buffer
    static tararchive_ptr_t loadFromMemory(datablock_ptr_t data);
    
    ////////////////////////////////////////////////////////////////////////////////
    // Error Handling
    ////////////////////////////////////////////////////////////////////////////////
    
    // Get last error message
    std::string getLastError() const;
    
    // Check if archive is valid
    bool isValid() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
    
    // Internal constructor for loaded archives
    explicit TarArchive(std::unique_ptr<Impl> impl);
};

////////////////////////////////////////////////////////////////////////////////
// Utility Functions
////////////////////////////////////////////////////////////////////////////////

// Helper to create archive from single file
tararchive_ptr_t createTarFromSingleFile(
    const file::Path& file_path,
    const std::string& archive_name = "",
    const TarCreateOptions& options = {}
);

// Helper to extract single file from tar
datablock_ptr_t extractSingleFileFromTar(
    const file::Path& tar_file,
    const std::string& filename
);

// Helper to list tar contents without loading
std::vector<std::string> listTarContents(const file::Path& tar_file);

// Helper to get tar file info
size_t getTarFileSize(const file::Path& tar_file);

} // namespace ork::util