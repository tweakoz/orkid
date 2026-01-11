# Vulkan Texture Interface (TXI) Technical Design Document

## Overview

The Vulkan Texture Interface provides multiple pipelines for uploading and managing textures on the GPU. Each pipeline is optimized for specific use cases with different performance characteristics.

## Upload Methods

### 1. initTextureFromData - CPU → GPU Upload

**File:** `ork.lev2/src/gfx/vulkan/vulkan_txi_from_data.cpp`

**Signature:**
```cpp
void VkTextureInterface::initTextureFromData(Texture* ptex, TextureInitData tid)
```

**Parameters:**
- `ptex`: Target texture object
- `tid.w, tid.h, tid.d`: Dimensions
- `tid._dst_format`: Target format (RGB8, RGBA8, BGRA8, RGB32F, Z24S8, etc.)
- `tid._data`: CPU memory pointer
- `tid._autogenmips`: Auto-generate mip chain
- `tid._allow_async`: Enable async upload (default: true)
- `tid._samplingMode`: Filtering/addressing modes

**What It Does:**
1. Handles format conversion (RGB8→RGBA8, BGR8→BGRA8 on macOS)
2. Auto-generates mip chain if requested
3. Uses staging buffers to transfer CPU→GPU
4. Manages image layout transitions (UNDEFINED → TRANSFER_DST → SHADER_READ_ONLY)
5. Implements double-buffering for async uploads (2 VkImages per texture)

**Performance:**

| Mode | Behavior |
|------|----------|
| Sync | Blocks caller, uses persistent staging buffer, CPU waits for GPU |
| Async | Non-blocking, borrows from staging buffer pool, completion callback |

**Use Cases:**
- Loading textures from disk at startup
- Dynamic texture updates (procedural, video frames)
- Cube textures with mip generation

---

### 2. initTextureFromGpuExternalSurface - GPU Direct Import

**File:** `ork.lev2/src/gfx/vulkan/vulkan_txi_from_external.mm`

**Signature:**
```cpp
void VkTextureInterface::initTextureFromGpuExternalSurface(Texture* ptex)
```

**What It Does:**
1. Imports IOSurface directly as VkImage via `VK_EXT_metal_objects` (macOS)
2. Zero-copy integration - MoltenVK creates Metal texture from IOSurface
3. 1:1 mapping: each IoSurfaceTexImpl owns one VkImage (created once, reused)
4. Async image layout transition to SHADER_READ_ONLY_OPTIMAL

**Performance:**

| Aspect | Characteristic |
|--------|----------------|
| Memory | Zero-copy, no staging buffer |
| Sync | GPU-GPU implicit, no CPU stall |
| Overhead | Minimal bookkeeping allocation only |

**Supported Formats:**
- NV12 (4:2:0 8-bit, 2 planes)
- P010 (4:2:0 10-bit, 2 planes)
- BGRA8 (32-bit)

**Use Cases:**
- Hardware video decode (VideoToolbox)
- Cross-framework GPU texture sharing
- Low-latency video playback
- Camera frame integration

---

### 3. _initTextureFromRtBuffer - Render Target Integration

**File:** `ork.lev2/src/gfx/vulkan/vulkan_txi_from_rtg.cpp`

**Signature:**
```cpp
void VkTextureInterface::_initTextureFromRtBuffer(RtBuffer* rtbuffer)
```

**What It Does:**
1. Wraps RtBuffer's existing VkImage as a Texture for sampling
2. Handles both color and depth buffer formats
3. Clears image with proper initial values
4. Suspends/resumes active render pass for barrier execution

**Performance:**

| Aspect | Characteristic |
|--------|----------------|
| Memory | Uses RtBuffer's allocation, no duplication |
| Sync | Synchronous, immediate barrier execution |
| Layout | UNDEFINED → TRANSFER_DST → attachment-optimal |

**Use Cases:**
- Post-processing passes (sample previous render)
- Ping-pong rendering
- Deferred shading G-buffer sampling
- Shadow map sampling

---

### 4. initTextureArray2DFromData - Batch Array Upload

**File:** `ork.lev2/src/gfx/vulkan/vulkan_txi_from_array.cpp`

**Signature:**
```cpp
void VkTextureInterface::initTextureArray2DFromData(TextureArray* array, TextureArrayInitData tid)
```

**Parameters:**
- `array`: Target texture array
- `tid._slices`: Vector of TextureArrayInitSubItem (mip chains per slice)

**What It Does:**
1. Uploads multiple 2D textures as array layers in single batch
2. Validates format consistency across slices
3. Copies all slices to single staging buffer with multiple copy regions
4. Single vkCmdCopyBufferToImage with batched regions

**Performance:**

| Aspect | Characteristic |
|--------|----------------|
| Batch | All slices in one command submission |
| Staging | Single contiguous buffer, per-slice offsets |
| Sync | Single completion semaphore for entire array |
| Scaling | O(N) slices, staging size = sum of slice data |

**Use Cases:**
- Texture atlases (terrain, materials)
- Shadow map arrays (one layer per light)
- Volumetric textures (layered 2D slices)

---

### 5. initTextureArray2DAsync - Deferred Array Init

**File:** `ork.lev2/src/gfx/vulkan/vulkan_txi_from_array.cpp`

**Signature:**
```cpp
void VkTextureInterface::initTextureArray2DAsync(TextureArray* texture_array)
```

**What It Does:**
1. Allocates GPU resources without uploading data
2. Returns immediately, array ready for incremental slice updates
3. Records on secondary command buffer for deferred execution
4. Slices populated later via `_updateTextureArraySlice`

**Performance:**

| Aspect | Characteristic |
|--------|----------------|
| Init Cost | Minimal - create, clear, transition |
| Update | Per-slice staging + command buffer |
| Execution | Deferred, doesn't block caller |

**Use Cases:**
- Streaming texture arrays (progressive loading)
- Dynamic atlasing (runtime add/remove)
- Out-of-core texture management
- Incremental asset loading

---

## Comparison Matrix

| Method | Source | Sync | Async | Memory Model | Primary Use |
|--------|--------|------|-------|--------------|-------------|
| initTextureFromData | CPU | Optional | Default | Staging + double-buffer | General loading |
| initTextureFromGpuExternalSurface | GPU | N/A | Implicit | Zero-copy | Video decode |
| _initTextureFromRtBuffer | RtBuffer | Yes | No | Existing alloc | Post-process |
| initTextureArray2DFromData | CPU | No | Yes | Batch staging | Atlases |
| initTextureArray2DAsync | Empty | No | Yes | Deferred | Streaming |

---

## Implementation Details

### Staging Buffer Strategy

- **initTextureFromData (async):** Per-mip buffers from pool, returned on completion
- **initTextureFromData (sync):** Persistent staging buffer, reused across calls
- **initTextureArray2DFromData:** Single batched staging buffer
- **initTextureFromGpuExternalSurface:** No staging (zero-copy import)

### Image Layout Transitions

All methods manage Vulkan image layout barriers:
1. `VK_IMAGE_LAYOUT_UNDEFINED` - Initial state
2. `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` - During upload
3. `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` - Ready for sampling

### Double-Buffering

`initTextureFromData` with async mode uses two VkImage objects per texture:
- Enables seamless updates without tearing
- Ping-pong between images across frames
- Completion callback swaps active image

### Format Conversion

macOS-specific conversions during staging copy:
- RGB8 → RGBA8 (add alpha channel)
- BGR8 → BGRA8
- RGB32F → RGBA32F

---

## Performance Guidelines

1. **Startup Loading:** Use `initTextureFromData` with async for parallel uploads
2. **Video Playback:** Use `initTextureFromGpuExternalSurface` for zero-copy decode
3. **Dynamic Updates:** Use async mode to avoid frame stalls
4. **Large Atlases:** Use `initTextureArray2DFromData` for batched efficiency
5. **Streaming:** Use `initTextureArray2DAsync` + incremental slice updates

### Memory Overhead

- Format conversion: Up to 4/3x original (RGB→RGBA)
- Mip generation: +33% (base + mips)
- Double-buffering: 2x for async textures
