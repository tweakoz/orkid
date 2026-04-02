---
name: orkcore-filesystem
description: Answer questions about orkid's filesystem abstraction, file paths, URI protocols, path expanders, FileEnv, file devices, DataBlock, and file I/O. Use when the user asks about file paths, URI schemes, path expansion, or data loading.
user-invocable: false
---

# Orkid Core Filesystem Reference

When answering questions about file I/O, paths, or virtual filesystem in orkid, consult the files below.

## Key Files

| Component | Header | Implementation |
|-----------|--------|----------------|
| Path | `ork.core/inc/ork/file/path.h` | `ork.core/src/file/Path.cpp` |
| FileEnv (singleton) | `ork.core/inc/ork/file/fileenv.h` | `ork.core/src/file/fileenv.cpp` |
| FileDev (abstract) | `ork.core/inc/ork/file/filedev.h` | `ork.core/src/file/filedev.cpp` |
| FileDevContext | `ork.core/inc/ork/file/filedevcontext.h` | |
| FileDevStd | `ork.core/inc/ork/file/filestd.h` | `ork.core/src/file/filestd.cpp` |
| FileDevRam | `ork.core/inc/ork/file/filedevram.h` | `ork.core/src/file/filedevram.cpp` |
| File (high-level) | `ork.core/inc/ork/file/file.h` | `ork.core/src/file/file.cpp` |
| DataBlock | `ork.core/inc/ork/kernel/datablock.h` | |
| ChunkFile | `ork.core/inc/ork/file/chunkfile.h` | `ork.core/src/file/chunkfile.cpp` |

## Architecture Overview

### Path Abstraction (`ork::file::Path`)
- Unified path supporting native, POSIX, URL, and asset catalog forms
- Path composition: `operator/()`, `operator+()`
- Decomposition: protocol, folder, file, extension
- Asset catalog paths: `namespace|assetid` (detected via `|` character)
- Filesystem queries: `doesPathExist()`, `isFile()`, `isFolder()`

### Path Expansion (thread-safe)
```cpp
file::setPathExpander("assetcache", stage_dir / "assetcache");
// Then: "<assetcache>/foo" expands to "/path/to/stage/assetcache/foo"
// Also: "assetcache://foo" works via URI protocol
```

Expansion order:
1. `~` -> `$HOME`
2. `key://rest` -> registered path + `/rest`
3. `<key>` -> registered path
4. `${ENV_VAR}` -> environment variable value

### Registered Expanders (from application.cpp)
- `<staging>` / `<stage>` -> staging directory
- `<assetcache>` -> `<stage>/assetcache`
- `<temp>` -> temp directory
- `<ork_data>` -> orkid data directory
- URI protocols: `src://`, `data://`, `ork_core://`, `ork_lev2://`

### FileEnv Singleton
- Global registry for URI protocols and file devices
- `createContextForUriBase("proto://", base_path)` — register new URI scheme
- `GetDeviceForUrl(path)` — resolve device for a path

### File Devices
- `FileDevStd` — platform native filesystem
- `FileDevRam` — in-memory filesystem (`RegisterRamFile()`)
- Abstract via `FileDev` interface: `_doOpenFile`, `_doRead`, `_doWrite`, etc.

### DataBlock
- Generic binary container backed by `std::vector<uint8_t>`
- `addData()`, `allocateBlock()`, `data()`, `length()`
- Compression: `compressed(level)` / `decompressed()` (LZ4)
- Hashing: `hash()` (xxHash64)
- Helpers: `datablockFromFileAtPath()`, `File::loadDatablock()`

### High-Level File I/O
```cpp
auto result = File::readAsString(path);      // StringFileReadResult
auto result = File::readAsBinary(path);      // BinaryFileReadResult
File::writeString(path, "content");
File::writeBinary(path, data_ptr, size);
```

## How to Answer

1. For path resolution: read `Path.cpp` `expandPaths()` function
2. For URI protocols: check `fileenv.cpp` and `application.cpp` registered expanders
3. For file reading: prefer `File::readAsString/readAsBinary` or `datablockFromFileAtPath`
4. Always verify path expander names exist by grepping `setPathExpander` calls
