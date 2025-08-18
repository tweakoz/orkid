# Asset Catalog System - Technical Design Document

---

## Summary

The Asset Catalog provides a unified, thread-safe system for managing downloadable content with sophisticated caching, encryption, and state management. It serves dual purposes with symmetric operations:

1. **Build-time**: Packages, encrypts, and uploads assets to CDN
2. **Runtime**: Downloads, decrypts, unpackage, verifies, caches, and serves assets to applications

The system maintains perfect symmetry:
- **Build**: File → Package → Encrypt → Upload to CDN
- **Runtime**: Download from CDN → Decrypt → Verify → Serve to App

This symmetry ensures that what goes up comes down intact, with end-to-end verification. Built around a flyweight pattern for efficient memory usage and atomic operations for thread safety.

---

## Features

### Core Capabilities

- **Unified Asset Management**
  - Single interface for all asset operations across namespaces
  - Flyweight pattern for AssetRequests with reference counting
  - Atomic state tracking throughout asset lifecycle
  - Thread-safe operations via LockedResource pattern

- **Namespace Organization**
  - Hierarchical namespace structure for asset organization
  - Per-namespace encryption keys and codecs
  - Namespace-specific configuration and policies
  - Pattern-based asset discovery and filtering

- **Robust Download Management**
  - Parallel downloads with DownloadGroup coordination
  - Concurrent asset fetching via AssetFuture/enqueueGet()
  - Progress tracking and cancellation support
  - Chunk-based downloads for large files (>16MB)
  - Configurable concurrent download limits (default: 4)

- **Security and Integrity**
  - Per-namespace encryption with libsodium
  - Hash verification (MD5, XXHash) with corruption detection
  - Password-protected namespace support with caching
  - Content-addressable storage using hash-based filenames

- **Async/Concurrent Operations**
  - AssetFuture-based async API for non-blocking fetches
  - Parallel chunk downloads for large assets
  - Thread-safe state management without shared_from_this
  - Background download queue with opq::concurrentQueue

---

## Architecture

![Asset Catalog Architecture](asset_catalog_architecture.svg)

---

## Design Principles

### Flyweight Pattern for Memory Efficiency
AssetRequests use flyweight pattern where identical asset IDs share the same request object, reducing memory overhead and enabling efficient reference counting.

### Thread-Safe State Management
All state transitions use atomic operations and LockedResource pattern for thread-safe access. The catalog uses direct mutation with proper locking rather than versioned state copies.

### Pimpl Pattern
Public interfaces use private-implementation (pimpl) pattern via CatalogImpl, hiding implementation details and enabling ABI stability.

### Content-Addressable Storage
Assets stored using hash-based filenames ({storage_hash}.enc) enabling deduplication and integrity verification.

---

## Component Details

### AssetCatalog
Central coordinator managing all asset operations, caching, and namespace organization. Thread-safe public interface.

### CatalogImpl
Private implementation containing:
- DownloadManager with parallel download support (max 4 concurrent)
- UploadManager for CDN uploads
- Stats tracking for cache hits/misses and performance metrics
- Direct state mutation via LockedResource

### FetchRequest
Encapsulates all parameters for asset fetching:
```cpp
struct FetchRequest {
  assetid_t asset_id;
  AssetLocation location;
  assetentry_ptr_t asset_info;
  bool decrypt = true;
  bool disable_cache = false;
};
```

### AssetFuture
Async fetch result with wait() and polling support:
- No catalog reference (avoiding shared_from_this)
- Thread-safe completion notification via condition variable
- Cancellation support

### DownloadGroup
Coordinates parallel downloads for chunks:
- Manages multiple concurrent Download objects
- Tracks overall progress across all chunks
- Atomic success/failure tracking

### ChunkAssembler
Reconstructs files from downloaded chunks:
- Verifies chunk hashes (XXHash)
- Assembles in correct order
- Handles decryption after assembly

### PasswordProvider
Interactive authentication for protected namespaces:
- Main thread password prompts (not background)
- Password caching for session
- Integration with namespace API keys

---

## State Management

### AssetState Enumeration
```cpp
enum class AssetState {
  NOT_AVAILABLE,    // Not in catalog
  QUEUED,          // Download queued
  DOWNLOADING,     // Active download
  ASSEMBLING,      // Chunk assembly
  VERIFYING,       // Hash verification
  CACHED_MEMORY,   // In memory
  CACHED_DISK,     // On disk
  FAILED,          // Operation failed
  CORRUPTED        // Hash mismatch
};
```

### AssetStatus Enumeration
```cpp
enum class AssetStatus {
  OK,                  // Success
  NETWORK,            // Network error
  DOWNLOAD_FAILED,    // Download error
  UPLOAD_FAILED,      // Upload error
  FILE_NOT_FOUND,     // Missing file
  PERMISSION,         // Access denied
  TIMEOUT,            // Operation timeout
  CANCELLED,          // User cancelled
  NOT_FOUND,          // Asset not in catalog
  CHECKSUM,           // Hash verification failed
  DECRYPT_FAILED,     // Decryption error
  DECOMPRESS_FAILED,  // Decompression error
  UNSUPPORTED         // Unknown format
};
```

---

## Chunking System

### Automatic Chunking
Files >16MB automatically split into 16MB chunks during repackage():
- Each chunk encrypted independently
- Chunk hashes tracked in ChunkManifest
- Storage hash = hash of concatenated chunk hashes

### Parallel Chunk Downloads
DownloadGroup manages concurrent chunk fetches:
- Each chunk to unique temp file (avoiding collision)
- Per-chunk hash verification
- Assembly only after all chunks complete

### Chunk Storage
```
cache/enc/chunks/
  {storage_hash}.chunk.0
  {storage_hash}.chunk.1
  ...
  {storage_hash}.chunk.manifest
```

---

## Password Authentication

### Protected Namespaces
Namespaces can require password authentication:
- API key contains password placeholder
- PasswordProvider prompts on main thread
- Passwords cached for session duration

### Authentication Flow
1. Check if API key requires password (PasswordProvider::requiresPasswordAuth)
2. Prompt user on main thread before enqueueing
3. Replace placeholder with actual password
4. Use authenticated key for downloads

---

## Cache Management

### Cache Structure
```
${OBT_STAGE}/assetcache/
  enc/              # Encrypted assets
    {hash}.enc      # Single file assets
    chunks/         # Chunked assets
      {hash}.chunk.0
      {hash}.chunk.manifest
  receipts/         # Upload receipts
  temp/             # Temporary downloads
```

### Cache Control
- `disable_cache` parameter bypasses cache entirely
- Cache verification via hash checking
- Automatic cleanup of corrupted entries

---

## Error Handling

### Mixed Error Model
- Fatal errors use OrkAssert (development)
- Recoverable errors return AssetStatus codes
- Async operations store errors in AssetResult

### Error Propagation
- Synchronous: Immediate AssetResult with status
- Asynchronous: Error stored in future, retrieved via wait()
- Callbacks: Error callbacks for progress tracking

---

## Performance Optimizations

### Parallel Operations
- Max 4 concurrent downloads (configurable)
- Chunk-level parallelism for large files
- Async enqueueing for batch operations

### Memory Management
- Flyweight pattern for request deduplication
- Shared pointers for safe async operations
- Direct buffer operations avoiding copies

### Progress Tracking
- Real-time bandwidth calculation
- Per-chunk and overall progress
- Pending bytes tracking at enqueue time

---

## Security Considerations

### Encryption
- Per-namespace libsodium encryption
- Password-protected namespaces
- Secure key storage in config

### Integrity
- Content hash verification (MD5/XXHash)
- Storage hash for encrypted data
- End-to-end tamper detection

### Access Control
- API key authentication
- Password caching with session scope
- TLS certificate validation (configurable)

---

## Implementation Notes

### Thread Safety
- All public methods thread-safe
- LockedResource for shared state
- Atomic operations for counters

### Resource Management
- RAII for file handles
- Automatic temp file cleanup
- Graceful shutdown handling

### Platform Support
- Cross-platform file operations
- Platform-specific path resolution
- Endian-neutral hash algorithms
