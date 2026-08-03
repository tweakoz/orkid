---
name: orklev2-vulkan
description: Answer questions about orkid's Vulkan backend, VkContext, device selection, swapchain (GLFW/DRM/offscreen), command buffers (primary/secondary), descriptor pools, pipeline caching, memory management, synchronization (semaphores/fences/timeline), dynamic rendering, validation layers, and platform specifics (MoltenVK/Linux DRM). Use when the user asks about Vulkan internals or GPU backend details.
user-invocable: false
---

# Orkid Vulkan Backend Reference

When answering questions about the Vulkan backend in orkid, consult the files below. All under `ork.lev2/src/gfx/vulkan/`.

## Key Files

| Component | File |
|-----------|------|
| VkContext | `headers/vulkan_ctx.h` (lines 664–912) |
| Synchronization | `headers/vk_synchro.h` |
| Memory/Buffers | `headers/vk_memory.h` |
| Pipelines | `headers/vk_pipeline.h` |
| Images/Textures | `headers/vk_image.h` |
| Render Targets | `headers/vk_rtgroup.h` |
| DRM Swapchain | `headers/vk_swapchain_drm.h` |
| Geometry | `headers/vk_geom.h` |
| Misc/Utils | `headers/vk_misc.h` |
| Type Aliases | `headers/vk_protos.h` |
| Context Init | `vulkan_ctx.cpp` |
| Instance/Device | `vulkan_vkimpl.cpp` |
| Command Buffers | `vulkan_ctx_cb.cpp` |
| Sync Impl | `vulkan_ctx_synchro.cpp` |
| Swapchain | `vulkan_swapchain.cpp` |
| DRM Swapchain | `vulkan_swapchain_drm.cpp` |
| Offscreen | `vulkan_offscreen.cpp` |
| FBI | `vulkan_fbi.cpp`, `vulkan_fbi_rtgroup.cpp` |
| TXI | `vulkan_txi.cpp`, `vulkan_txi_from_data.cpp` |
| FXI/Pipelines | `vulkan_fxi.cpp`, `vulkan_fxi_pipelines*.cpp` |
| GBI | `vulkan_gbi.cpp` |
| Compute | `vulkan_compute.cpp` |

## Architecture

```
VkContext : public Context
├── VkSwapChain (GLFW window presentation)
├── VkOffscreen (headless rendering)
├── VkSwapChainDRM (Linux DRM direct rendering)
├── VkGeometryBufferInterface : GBI
├── VkTextureInterface : TXI
├── VkFxInterface : FXI
├── VkFrameBufferInterface : FBI
├── VkComputeInterface : CI
├── VkDrawingInterface : DWI
└── VkMatrixStackInterface : MTXI
```

## VkContext Key Members

| Member | Description |
|--------|-------------|
| `_vkdevice` | Logical device |
| `_vkphysicaldevice` | Physical device |
| `_vkdeviceinfo` | Device capabilities |
| `_vkqfid_graphics/compute/transfer` | Queue family indices |
| `_vkqueue_graphics` | Graphics queue |
| `_vkcmdpool_graphics` | Command pool |
| `_vkDescriptorPool` | Global descriptor pool (262K descriptors) |
| `_sampler_cache` | Sampler object cache |
| `_defaultCommandBuffer` | Current primary CB |
| `_pri_cmdbuf_pool` | 16-entry primary CB pool |

## Output Paths

### VkSwapChain (GLFW)
- KHR swapchain with BGRA8 format
- Prefers MAILBOX, falls back to FIFO (vsync)
- Binary semaphores for acquire/present
- `_buildup()` / `_teardown()` for resize

### VkOffscreen
- No presentation surface
- Synchronous fence-based submission
- Transitions color buffer to `SHADER_READ_ONLY` for readback

### VkSwapChainDRM (Linux)
- Direct rendering via DRM page-flip
- Exportable images with `VK_EXT_IMAGE_DRM_FORMAT_MODIFIER`
- DMA-BUF memory allocation
- Prefers `DRM_FORMAT_MOD_LINEAR`

## Frame Lifecycle

1. **`_doPreBeginFrame()`** — allocate primary CB, execute pending one-shot secondary CBs
2. **`_doBeginFrame()`** — poll completion semaphores, clean up TXI resources
3. **Rendering** — record commands into primary/secondary CBs
4. **`_doEndFrame()`** — end profiler, submit primary CB, deallocate to pool

`MAX_FRAMES_IN_FLIGHT = 2` — double-buffered frames in flight with frame fences.

## Command Buffers

### Primary CBs
- Pool of 16, one per frame
- Reset on allocation, submitted at end-of-frame

### Secondary CBs
- Created via `_beginRecordCommandBuffer(name, rtg)`
- Inherit dynamic rendering state from RTG
- Executed via `vkCmdExecuteCommands()` in primary CB
- Support pre-enqueue and cleanup callbacks

## Descriptor Management

Global pool: 262K descriptors per type (COMBINED_IMAGE_SAMPLER, SAMPLER, SAMPLED_IMAGE, UNIFORM_BUFFER, UNIFORM_BUFFER_DYNAMIC, STORAGE_BUFFER).

`VulkanDescriptorSetCache` — per-program hash-based cache. Creates on-demand from global pool.

## Pipeline Management

`VkPipelineObject`:
- Keyed by hash of: vertex interface + geometry + raster state + program
- `_fetchPipeline()` / `_createPipeline()` — cache lookup/creation
- Dynamic UBO offsets updated per-draw
- Push constants for per-stage uniforms

## Memory Management

- `VulkanBuffer` — wraps `VkBuffer` + `VkDeviceMemory`
- `VulkanMemoryForBuffer` / `VulkanMemoryForImage` — allocation wrappers
- `StagingBufferPool` — locked object pool per size for texture uploads
- `SyncTransferResources` — out-of-frame staging for asset loading

## Synchronization

| Type | Class | Usage |
|------|-------|-------|
| Binary Semaphore | `VulkanBinarySemaphore` | Swapchain acquire/present |
| Timeline Semaphore | `VulkanTimelineSemaphore` | Completion tracking, one-shot commands |
| Completion Semaphore | `VulkanCompletionSemaphore` | Polled callbacks |
| Fence | `VulkanFenceObject` | Frame completion, offscreen sync |

## Dynamic Rendering

Uses `VK_KHR_DYNAMIC_RENDERING` (Vulkan 1.3+) — no traditional render passes. Secondary CBs inherit via `VkCommandBufferInheritanceRenderingInfo`.

## Platform-Specific

### macOS (MoltenVK)
- `VK_KHR_portability_subset`
- `VK_EXT_metal_objects` — IOSurface for GPU-direct video
- Disables Metal argument buffers

### Linux
- `VK_KHR_EXTERNAL_MEMORY_FD` + `VK_EXT_EXTERNAL_MEMORY_DMA_BUF` — DMA-BUF import
- `VK_EXT_IMAGE_DRM_FORMAT_MODIFIER` — DRM direct rendering
- `VK_KHR_SAMPLER_YCBCR_CONVERSION` — video decode

## Debug/Validation

- Enabled via `ORKID_VULKAN_VALIDATE=1` env var
- Uses `VK_LAYER_KHRONOS_validation`
- Debug callback traps on errors
- Object naming via `vkSetDebugUtilsObjectNameEXT`
- Debug markers: `debugPushGroup()` / `debugPopGroup()` / `debugMarker()`

## How to Answer

1. For VkContext: check `vulkan_ctx.h:664–912`
2. For swapchain: check `vulkan_swapchain.cpp` and `vulkan_ctx.h:303–344`
3. For pipelines: check `vk_pipeline.h` and `vulkan_fxi_pipelines*.cpp`
4. For sync: check `vk_synchro.h` and `vulkan_ctx_synchro.cpp`
5. For memory: check `vk_memory.h`
6. For DRM: check `vk_swapchain_drm.h` and `vulkan_swapchain_drm.cpp`
7. For device init: check `vulkan_vkimpl.cpp` and `vulkan_ctx.cpp`
8. For abstract Context mapping: consult **orklev2-gfxcontext** skill
