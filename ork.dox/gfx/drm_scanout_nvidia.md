# DRM scanout on NVIDIA — constraints and multi-card enumeration

**Provenance:** distilled from the multi-card DRM investigation handoff (RESOLVED 2026-07-01);
verified rendering on a 2x RTX 3090 Linux box via DRM/KMS (nvidia-drm `modeset=1`).

## The five NVIDIA DRM scanout constraints

1. **Do not request `VK_KHR_swapchain` in DRM mode** (`vulkan_ctx.cpp`).
   The DRM instance has no `VK_KHR_surface`, and DRM present never uses
   `VkSwapchainKHR` — requesting the device extension fails device creation.

2. **dma-buf exports need `VkMemoryDedicatedAllocateInfo`**
   (`vulkan_swapchain_drm.cpp`). NVIDIA requires dedicated allocation for
   exported scanout memory; RADV merely tolerated its absence, which masked this.

3. **Render GPU must match the display GPU.** Pick the Vulkan physical device
   whose `VK_EXT_physical_device_drm` major/minor matches the DRM card — for
   BOTH the loader context and the DRM context (`vulkan_ctx.cpp`). Scanout
   dma-bufs cannot cross GPUs on a multi-GPU box.

4. **Scanout goes through a LINEAR copy, not direct tiled scanout**
   (`vulkan_swapchain_drm.cpp`). NVIDIA block-linear direct scanout displays
   corrupted output (tiling decode mismatch at the display engine). The engine
   automatically renders tiled → `vkCmdCopyImage` → LINEAR image → page flip;
   verified pixel-faithful via movie-capture vs on-screen comparison.
   `ORKID_DRM_DIRECT_SCANOUT=1` forces the old direct path for A/B testing.

5. **The DRM context must share the loader context's `VkDevice` when the
   physical GPUs match** (`vulkan_ctx.cpp` `initializeDRMContext`), as the
   GLFW/offscreen paths already did. Otherwise textures uploaded by the loader
   thread are foreign handles in the DRM context and silently render as
   placeholder content (green graticule sky, red/grey checker grid). Not
   multi-GPU-specific — DRM mode on any box needs this.

## Multi-card monitor enumeration

DRM enumeration scans **all** `/dev/dri/card*` nodes (not just the first one
that opens), merges connected monitors with globally-unique device letters
('a', 'b', ...) in card order, and records each monitor's owning `card_path` so
`DRMContext` opens the correct card for the selected monitor
(`drm::DRMContext::enumerateAllMonitors()` in
`ork.lev2/src/drm/drm_context.cpp`, `Monitor::card_path` in
`ork.lev2/inc/ork/lev2/drm/drm_types.h`).

Why: the old code grabbed the first openable card and stopped. On a multi-GPU
box the first node can be a motherboard VGA/BMC DAC (e.g. `card1` when `card0`
doesn't exist), hiding every real GPU monitor. It only ever worked on AMD boxes
because the amdgpu happened to be the first openable node.

List devices/modes with `ork.devicelist.monitors.ix.drm.py` — each display
prints its `Card: /dev/dri/cardN` line.

## Environment knobs

- `ORKID_DRM_MODE=<dev><mode>` (e.g. `c0`) — forces DRM mode for any app with
  no argparse plumbing. Applied at `AppInitData` construction
  (`application.cpp`) so Vulkan GPU selection matches the DRM card (constraint
  3). An explicit `drm_mode_id` kwarg takes precedence.
- `ORKID_DRM_DIRECT_SCANOUT=1` — bypass the LINEAR-copy path (constraint 4)
  for A/B; expect corruption on NVIDIA.
- `ORKID_DRM_NOVSYNC=1` — async/tearing page flips (auto-fallback if the
  driver rejects them); unblocks frame rates quantized against vblank.

## Known limitation

The DRM present path is fully serial (record → submit → fence-wait → flip →
vblank-wait); windowed swapchains pipeline 2-3 frames in flight. Improving this
means not fence-waiting before the flip / overlapping the next frame's record.
