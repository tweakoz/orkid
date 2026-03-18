# Uncaught Exceptions Audit

Generated: 2026-03-12

## Critical — Will `std::terminate()` (no catch in call chain)

### `ork.lev2/src/gfx/vulkan/vulkan_swapchain_drm.cpp`
No catch blocks anywhere in file. All in methods called from `_buildup()`.

| Line | Function | Message |
|------|----------|---------|
| 164 | `_createExportableImages()` | `"Required Vulkan function not available"` |
| 173 | `_createExportableImages()` | `"No DRM modifiers available"` |
| 211 | `_createExportableImages()` | `"No suitable DRM modifier found"` |
| 228 | `_createExportableImages()` | `"Required Vulkan function not available"` |
| 276 | `_createExportableImages()` | `"Failed to create Vulkan image"` |
| 321 | `_createExportableImages()` | `"No suitable memory type found"` |
| 327 | `_createExportableImages()` | `"Failed to allocate Vulkan memory"` |
| 386 | `_exportImagesToDRM()` | `"Required Vulkan function not available"` |
| 399 | `_exportImagesToDRM()` | `"Failed to export DMABUF"` |
| 411 | `_exportImagesToDRM()` | `"Failed to import DMABUF to DRM"` |
| 420 | `_exportImagesToDRM()` | `"Failed to create DRM framebuffer"` |
| 480 | `_createRenderPass()` | `"Failed to create DRM render pass"` |
| 514 | `_createImageViews()` | `"Failed to create DRM image view"` |
| 556 | `_createFramebuffers()` | `"Failed to create DRM framebuffer"` |
| 732 | `waitPresentFrame()` | `"SetCrtc failed"` |
| 752 | `waitPresentFrame()` | `"Page flip failed"` |

**Recommendation:** Replace all with `OrkAssert` for consistency with the rest of the vulkan codebase.

### `ork.lev2/src/gfx/scenegraph/scenegraph.cpp`

| Line | Function | Message |
|------|----------|---------|
| 396 | `configureCompositor()` | `"unknown compositor preset type"` |

**Recommendation:** Replace with `OrkAssert`.

### `ork.lev2/src/drm/drm_context.cpp`
8 throws in `loadConfig()` during device enumeration.

| Line | Message |
|------|---------|
| 338 | `"Failed to open any DRM device (/dev/dri/card*)"` |
| 360 | `error_msg` (dynamic) |
| 370 | `error_msg` (dynamic) |
| 379 | `error_msg` (dynamic) |
| 390 | `error_msg` (dynamic) |
| 412 | `"Failed to get DRM resources"` |
| 420 | `"Failed to get connector"` |
| 444 | `"Failed to find CRTC"` |

**Note:** `drm_context.cpp` has some local catch blocks — review whether these throws escape them before replacing.

---

## Probably Fine — Propagate to Python boundary

These are in I/O/asset/utility paths eventually called from Python bindings, which do have `catch (const std::exception&)` handlers.

| File | Count | Context |
|------|-------|---------|
| `ork.core/src/util/crypt.cpp` | 8 | Crypto encode/decode failures |
| `ork.core/src/kernel/datablock.cpp` | 2 | LZ4 compress/decompress failures |
| `ork.core/src/asset/catalog/entry.cpp` | 2 | Asset upload validation |
| `ork.core/src/asset/catalog/chunk_manifest.cpp` | 1 | JSON parse error |
| `ork.core/src/python/pycodec_pybind11.cpp` | 4 | Unregistered codec type |

---

## Known Catch Sites (for reference)

| File | Scope |
|------|-------|
| `ork.core/src/application/application.cpp:493` | Only wraps `onAppShutdown()` |
| `ork.core/src/kernel/thread.cpp:42` | Thread entry point |
| `pyext_*.cpp` (many) | Python/C++ boundary |
| `ork.core/src/asset/catalog/*.cpp` | Asset I/O operations |
| `ork.core/src/util/upload_https.cpp` | Network I/O |
