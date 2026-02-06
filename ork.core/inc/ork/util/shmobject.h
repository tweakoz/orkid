////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/detail/shared_dir_helpers.hpp>
#include <memory>
#include <string>
#include <atomic>
#include <type_traits>
#include <cstring>
#include <chrono>
#include <unistd.h>
#include <regex>
#include <dirent.h>
#include <vector>

namespace ork {

///////////////////////////////////////////////////////////////////////////////
// Shared memory wrapper with RAII and reference counting
///////////////////////////////////////////////////////////////////////////////

template<typename T>
class ShmObject {
  static_assert(std::is_standard_layout<T>::value,
                "T must have standard layout for shared memory");

private:
  // Atomic competition control - kept separate from T data
  struct CreationControl {
    std::atomic<uint64_t> primary_lock{0};      // First gate
    std::atomic<uint64_t> secondary_lock{0};    // Second gate  
    std::atomic<uint64_t> creator_verified{0};  // Third gate
    std::atomic<uint64_t> init_complete{0};     // Initialization done signal
  };
  
  static constexpr size_t HEADER_SIZE = sizeof(CreationControl);
  static constexpr size_t DATA_OFFSET = HEADER_SIZE;

public:
  // Realize shared memory segment (create if doesn't exist, attach if does)
  static std::shared_ptr<ShmObject> realize(const std::string& name) {
    // Add prefix for easy cleanup - max user name is 247 chars (255 - 8)
    std::string prefixed_name = "ork.shm." + name;
    return std::make_shared<ShmObject>(prefixed_name);
  }

  // Constructor - bulletproof atomic competition
  ShmObject(const std::string& name)
      : _name(name)
      , _size(sizeof(T) + HEADER_SIZE)  // Total size includes header
      , _is_creator(false) {
    
    namespace bip = boost::interprocess;
    
    try {
      // Always use open_or_create
      _shm = std::make_unique<bip::shared_memory_object>(
        bip::open_or_create,
        _name.c_str(),
        bip::read_write
      );
      
      // Set total size (safe to call multiple times)
      _shm->truncate(_size);
      _region = std::make_unique<bip::mapped_region>(*_shm, bip::read_write);
      
      // Get control header
      CreationControl* control = static_cast<CreationControl*>(_region->get_address());
      
      // Generate unique process identifier
      uint64_t process_id = generateProcessId();
      
      // Run atomic competition - exactly one winner guaranteed
      if (runCreatorCompetition(control, process_id)) {
        // I WON! Initialize T in protected area
        _is_creator = true;
        _image = (T*) getDataPtr(control);
        _image->initializeShmImage();
        
        // Signal initialization complete (releases all waiters)
        control->init_complete.store(0xDEADBEEF);
        //printf("ShmObject: Created and initialized '%s' size=%zu\n", _name.c_str(), _size);
        
      } else {
        // Someone else won - wait for initialization to complete
        _is_creator = false;
        waitForInitialization(control);
        _image = static_cast<T*>(getDataPtr(control));
        //printf("ShmObject: Attached to initialized '%s' size=%zu\n", _name.c_str(), _size);
      }
      
    } catch (const bip::interprocess_exception& e) {
      printf("ShmObject error: %s\n", e.what());
      throw;
    }
  }

  ~ShmObject() {
    if (_is_creator && _image) {
      // Call destructor explicitly (no-op for POD types)
      _image->uninitializeShmImage();
      
      // Remove shared memory
      boost::interprocess::shared_memory_object::remove(_name.c_str());
      //printf("ShmObject: Removed '%s'\n", _name.c_str());
    } else {
      //printf("ShmObject: Detached from '%s'\n", _name.c_str());
    }
  }

  // No copy
  ShmObject(const ShmObject&) = delete;
  ShmObject& operator=(const ShmObject&) = delete;

  // Get data pointer
  T* image() { return _image; }
  const T* image() const { return _image; }
  
  // Dereference operators
  T& operator*() { return *_image; }
  const T& operator*() const { return *_image; }
  T* operator->() { return _image; }
  const T* operator->() const { return _image; }

  // Get size
  size_t size() const { return _size; }
  
  // Get name
  const std::string& name() const { return _name; }
  
  // Check if creator
  bool is_creator() const { return _is_creator; }
  
  // Clear (zero) the memory - only the T data, not the header
  void clear() {
    if (_image) {
      std::memset(_image, 0, sizeof(T));
    }
  }

private:
  // Atomic competition - exactly one winner guaranteed
  bool runCreatorCompetition(CreationControl* control, uint64_t process_id) {
    uint64_t expected = 0;
    
    // Gate 1: Primary lock
    if (!control->primary_lock.compare_exchange_strong(expected, process_id)) {
      return false;
    }
    
    // Gate 2: Secondary verification  
    expected = 0;
    if (!control->secondary_lock.compare_exchange_strong(expected, process_id)) {
      return false;
    }
    
    // Gate 3: Final creator verification
    expected = 0;
    if (!control->creator_verified.compare_exchange_strong(expected, process_id)) {
      return false;
    }
    
    // All gates passed - guaranteed winner!
    return true;
  }
  
  // Wait for winner to complete initialization
  void waitForInitialization(CreationControl* control) {
    // Spin-wait with backoff to avoid CPU thrashing
    int backoff = 1;
    while (control->init_complete.load() != 0xDEADBEEF) {
      usleep(backoff);
      if (backoff < 1000) backoff *= 2;  // Exponential backoff up to 1ms
    }
  }
  
  // Get pointer to T data (after header)
  void* getDataPtr(CreationControl* control) {
    return reinterpret_cast<char*>(control) + HEADER_SIZE;
  }
  
  // Generate unique process identifier
  uint64_t generateProcessId() {
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    return (uint64_t(getpid()) << 32) | (timestamp & 0xFFFFFFFF);
  }

private:
  std::string _name;
  size_t _size;
  bool _is_creator;
  std::unique_ptr<boost::interprocess::shared_memory_object> _shm;
  std::unique_ptr<boost::interprocess::mapped_region> _region;
  T* _image = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// Reference counted shared memory with inter-process coordination
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct ShmObjectHeader {
  std::atomic<int32_t> ref_count{0};
  boost::interprocess::interprocess_mutex mutex;
  boost::interprocess::interprocess_condition condition;
  T data;
};

template<typename T>
class RefCountedShmObject {
  static_assert(std::is_standard_layout<T>::value,
                "T must have standard layout for shared memory");

public:
  using Header = ShmObjectHeader<T>;
  
  // Create new shared memory segment
  static std::shared_ptr<RefCountedShmObject> create(const std::string& name) {
    std::string prefixed_name = "ork.shm." + name;
    auto ptr = std::make_shared<RefCountedShmObject>(prefixed_name, true);
    ptr->add_ref();
    return ptr;
  }

  // Attach to existing shared memory segment
  static std::shared_ptr<RefCountedShmObject> attach(const std::string& name) {
    std::string prefixed_name = "ork.shm." + name;
    auto ptr = std::make_shared<RefCountedShmObject>(prefixed_name, false);
    ptr->add_ref();
    return ptr;
  }

  RefCountedShmObject(const std::string& name, bool create_new)
      : _name(name)
      , _is_creator(create_new) {
    
    namespace bip = boost::interprocess;
    
    try {
      if (_is_creator) {
        // Remove old instance if exists
        bip::shared_memory_object::remove(_name.c_str());
        
        // Create new shared memory
        _shm = std::make_unique<bip::shared_memory_object>(
          bip::create_only,
          _name.c_str(),
          bip::read_write
        );
        
        // Set size
        _shm->truncate(sizeof(Header));
        
        // Map the memory
        _region = std::make_unique<bip::mapped_region>(*_shm, bip::read_write);
        
        // Construct header in-place
        _header = new (_region->get_address()) Header();
        
        printf("RefCountedShmObject: Created '%s'\n", _name.c_str());
      } else {
        // Attach to existing
        _shm = std::make_unique<bip::shared_memory_object>(
          bip::open_only,
          _name.c_str(),
          bip::read_write
        );
        
        // Map the memory
        _region = std::make_unique<bip::mapped_region>(*_shm, bip::read_write);
        
        // Get pointer to existing header
        _header = static_cast<Header*>(_region->get_address());
        
        printf("RefCountedShmObject: Attached to '%s'\n", _name.c_str());
      }
    } catch (const bip::interprocess_exception& e) {
      printf("RefCountedShmObject error: %s\n", e.what());
      throw;
    }
  }

  ~RefCountedShmObject() {
    release_ref();
  }

  // No copy
  RefCountedShmObject(const RefCountedShmObject&) = delete;
  RefCountedShmObject& operator=(const RefCountedShmObject&) = delete;

  // Get data
  T* image() { return &_header->data; }
  const T* image() const { return &_header->data; }
  
  // Lock for exclusive access
  boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock() {
    return boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>(_header->mutex);
  }
  
  // Wait on condition
  void wait(boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>& lock) {
    _header->condition.wait(lock);
  }
  
  // Notify one waiter
  void notify_one() {
    _header->condition.notify_one();
  }
  
  // Notify all waiters
  void notify_all() {
    _header->condition.notify_all();
  }
  
  // Get current reference count
  int32_t ref_count() const {
    return _header->ref_count.load();
  }

private:
  void add_ref() {
    int32_t count = _header->ref_count.fetch_add(1) + 1;
    printf("RefCountedShmObject: '%s' ref_count=%d\n", _name.c_str(), count);
  }
  
  void release_ref() {
    int32_t count = _header->ref_count.fetch_sub(1) - 1;
    printf("RefCountedShmObject: '%s' ref_count=%d\n", _name.c_str(), count);
    
    if (count == 0 && _is_creator) {
      // Last reference and we're the creator, clean up
      _header->~Header();
      boost::interprocess::shared_memory_object::remove(_name.c_str());
      printf("RefCountedShmObject: Removed '%s' (last reference)\n", _name.c_str());
    }
  }

  std::string _name;
  bool _is_creator;
  std::unique_ptr<boost::interprocess::shared_memory_object> _shm;
  std::unique_ptr<boost::interprocess::mapped_region> _region;
  Header* _header = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// Utility functions for shared memory management
///////////////////////////////////////////////////////////////////////////////

// List all shared memory segments matching a pattern
inline std::vector<std::string> listShmObjects(const std::string& pattern = "ork\\.shm\\..*") {
  std::vector<std::string> found_segments;
  
  // Get the shared memory directory
  std::string shm_dir;
  
#ifdef __APPLE__
  // macOS uses /var/folders/.../<uid>/C/ for shared memory
  boost::interprocess::ipcdetail::get_shared_dir(shm_dir);
#else
  // Linux typically uses /dev/shm/
  shm_dir = "/dev/shm/";
#endif
  
  std::regex pattern_regex(pattern);
  
  // Open directory
  DIR* dir = opendir(shm_dir.c_str());
  if (dir) {
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
      std::string name = entry->d_name;
      
      // Check if it matches our pattern
      if (std::regex_match(name, pattern_regex)) {
        found_segments.push_back(name);
      }
    }
    closedir(dir);
  }
  
  return found_segments;
}

// Remove a shared memory segment by name
inline bool removeShmObject(const std::string& name) {
  try {
    return boost::interprocess::shared_memory_object::remove(name.c_str());
  } catch (...) {
    return false;
  }
}

// Clean up all Orkid shared memory segments
inline int cleanupOrkidShmObjects(bool verbose = false) {
  int removed_count = 0;
  int failed_count = 0;
  
  // Find all segments matching ork.shm.* pattern
  auto orkid_segments = listShmObjects("ork\\.shm\\..*");
  
  // Also find legacy segments (without prefix) for backward compatibility
  std::vector<std::string> legacy_patterns = {
    "test_multiproc.*",
    "test_basic",
    "test_stress", 
    "test_death",
    "stress_test_shm",
    "montecarlo_pi"
  };
  
  for (const auto& pattern : legacy_patterns) {
    auto legacy_segs = listShmObjects(pattern);
    orkid_segments.insert(orkid_segments.end(), legacy_segs.begin(), legacy_segs.end());
  }
  
  if (verbose) {
    if (orkid_segments.empty()) {
      fprintf(stderr, "No Orkid shared memory segments found.\n");
    } else {
      fprintf(stderr, "Found %zu segment(s):\n", orkid_segments.size());
      for (const auto& seg : orkid_segments) {
        fprintf(stderr, "  • %s\n", seg.c_str());
      }
      fprintf(stderr, "\n");
    }
  }
  
  // Clean up each segment
  for (const auto& name : orkid_segments) {
    bool removed = removeShmObject(name);
    if (removed) {
      if (verbose) fprintf(stderr, "  ✓ Removed: %s\n", name.c_str());
      removed_count++;
    } else {
      if (verbose) fprintf(stderr, "  ✗ Failed to remove: %s\n", name.c_str());
      failed_count++;
    }
  }
  
  if (verbose && removed_count > 0) {
    fprintf(stderr, "\nSuccessfully removed %d segment(s)\n", removed_count);
  }
  if (verbose && failed_count > 0) {
    fprintf(stderr, "Failed to remove %d segment(s)\n", failed_count);
  }
  
  return removed_count;
}

} // namespace ork