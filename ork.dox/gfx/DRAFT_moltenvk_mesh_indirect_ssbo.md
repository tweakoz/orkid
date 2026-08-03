# DRAFT (NOT FILED) — vkCmdDrawMeshTasksIndirectEXT costs ~90 ms/call with a storage buffer bound

**Status: DRAFT for owner filing.** Prepared 2026-07-26 from the asset-free reproducer in
`ork.lev2/pyext/tests/llgfx/test_terrain_meshshader_ab.py --reproduce-indirect`. Unlike the
companion MSAA draft, this one survives its cross-platform control by three orders of magnitude.

## Summary

On MoltenVK with the taskless mesh-shader implementation, a single
`vkCmdDrawMeshTasksIndirectEXT` recorded against a pipeline that has a storage buffer bound
costs **~90 ms per call** in a Release build. The cost is:

- **linear in call count** (1 / 2 / 4 calls -> 90.7 / 177.7 / 355.5 ms),
- **independent of resolution** (88.3 -> 92.3 ms/call across a 36x change in pixel count),
- **absent from the direct entry point**: the same pipeline, same SSBO, same meshlets drawn with
  `vkCmdDrawMeshTasksEXT` costs 1.4-1.6 ms,
- **absent on a discrete GPU**: the same reproducer on NVIDIA measures 0.24-1.0 ms/call.

Because the cost does not scale with rasterization work and vanishes when only the entry point
changes, it is a per-call stall in the indirect path rather than GPU rendering work.

## Environment

| | |
|---|---|
| host | Apple M3 Ultra, 80-core GPU, 256 GB, macOS 26.5.1 (25F80), Metal 4 |
| MoltenVK | `MVK_VERSION` 1.4.2, ICD `api_version` 1.4.0, `is_portability_driver` true |
| MoltenVK source | PR #2777 — `dttdrv/MoltenVK` branch `macgaming/mesh-shader` @ `4fc3f6c1f97c` (taskless mesh shaders; CTS 353/0) |
| SPIRV-Cross pin | `af71ba0bbfcc6896aad8d5744784b65da7f955b0` |
| MVK env | `MVK_CONFIG_LOG_LEVEL=0`, `VK_ICD_FILENAMES=<stage>/builds/moltenvk/Package/Latest/MoltenVK/dylib/macOS/MoltenVK_icd.json` |
| control host | NVIDIA RTX 3090, Linux 6.8, Vulkan SDK 1.3.296.0 |
| build | Release (the cost is present in Release; this is not a validation-layer or debug artifact) |

## Minimal repro steps

1. Create one storage buffer holding a procedural heightfield, a camera block, a visible-chunk
   list and a `VkDrawMeshTasksIndirectCommandEXT`.
2. Create one taskless mesh-shader graphics pipeline with that storage buffer bound
   (descriptor set 0), and a fragment shader that writes one color attachment.
3. Per frame, outside the render pass, record one compute dispatch that fills the visible-chunk
   list and writes the indirect command, then a producer->consumer barrier.
4. Begin the render pass and record K x `vkCmdDrawMeshTasksIndirectEXT(cb, ssbo, offset, 1, 0)`.
5. Submit and wait on the fence. Time the whole frame on the host.
6. Repeat with `vkCmdDrawMeshTasksEXT(cb, x, y, 1)` using the same grid the indirect command
   contains — the only difference is the entry point.

The `compaction-only` row below is step 3 with K = 0: the same compute work, the same barrier,
the same submit, no draws. It is the floor the draw legs are measured against.

## Measured — MoltenVK / M3 Ultra (mean wall ms per full frame, 30 frames/cell)

    res         compact-only  mesh_ind x1  mesh_ind x2  mesh_ind x4  mesh_dir x1    ms/ind-call
    640x360           0.8453      90.6508     177.6545     355.5473       1.3706        88.2988
    2560x1280         0.9970      92.8745     182.3240     363.3048       1.6302        90.1434
    3840x2160         1.1657      94.1341     186.1087     371.0212       1.6085        92.2957

Observations:

- **K-linearity**: 90.65 / 177.65 / 355.55 ms for 1 / 2 / 4 calls — a constant ~88 ms marginal
  cost per additional indirect call, with no fixed setup component beyond the ~1 ms floor.
- **Resolution independence**: 88.3 / 90.1 / 92.3 ms per call while pixel count grows 36x
  (230k -> 8.3M). Rasterization work is not what is being paid for.
- **Entry-point isolation**: `mesh_dir x1` draws the same meshlets from the same SSBO over the
  same compacted list via `vkCmdDrawMeshTasksEXT` and costs 1.37-1.63 ms — a ~55-65x difference
  attributable to the indirect entry point alone.

## Measured — NVIDIA 3090 control (same reproducer, same day)

    res         compact-only  mesh_ind x1  mesh_ind x2  mesh_ind x4  mesh_dir x1    ms/ind-call
    640x360           0.4546       1.7814       2.0617       2.4908       1.7799         0.2365
    2560x1280         0.4611       1.3755       2.4495       4.3748       1.3264         0.9997
    3840x2160         0.4736       1.3446       2.3761       4.1546       1.3301         0.9367

On the discrete GPU the marginal indirect call costs 0.24-1.0 ms and *does* scale with
resolution — i.e. it behaves like the drawing work it represents.

## Additional isolation from earlier lane measurements

An independently written probe reproduced 82.7-83.7 ms/call with marginal K-linearity 1.00 and
narrowed the trigger further: with a **trivial pipeline that has no storage buffer bound**, the
indirect draw costs 0.86 ms against 0.74 ms for the direct draw (1.2x, flat in K). The cost
therefore appears specific to indirect mesh draws **against pipelines with bound storage**, and
sits in submit + fence-wait; CPU-side command encoding measured under 0.06 ms.

Mechanism hypothesis (unverified, offered only as a starting point): the indirect prepare /
"compute capture and replay" path re-processes bound storage per call; the encode split around
`MVKCmdDraw.mm` (approximately lines 1534-1700 in the PR branch) is the plausible vehicle, with
the storage-buffer interaction as the actual cost.

## Reproducer invocation

    # asset-free, offscreen, no window, ~80s on M3 Ultra / ~10s on NVIDIA
    ork.python ork.lev2/pyext/tests/llgfx/test_terrain_meshshader_ab.py \
        --reproduce-indirect --reproduce-frames 30

The same file's default mode is a committed regression gate; its `MESHPERF ... ratio_ind=` field
carries the same signal from the full battery (`mesh_ind_ms_mean` 92-96 ms at 2560x1280 on
MoltenVK, 0.2-1.3 ms on NVIDIA).

## Impact on the shipping application

The engine's GPU-driven terrain path (one compaction dispatch publishing the mesh grid, then one
indirect mesh draw) is unusable on this MoltenVK build for real content: a single frame with one
indirect mesh draw costs ~93 ms, so the path stays behind an explicit opt-in on macOS while the
direct-dispatch variant ships instead.
