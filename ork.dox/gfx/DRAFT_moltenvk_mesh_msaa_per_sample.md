# DRAFT (NOT FILED) — mesh pipelines in multisampled render passes: per-sample cost

**Status: DRAFT, and as measured it does NOT support filing.** Prepared 2026-07-26 from the
asset-free reproducer in `ork.lev2/pyext/tests/llgfx/test_terrain_meshshader_ab.py --reproduce`.
The cross-platform control (same reproducer, NVIDIA 3090) shows the mesh pipeline's per-sample
cost multiplier is **not** an outlier on MoltenVK. Read the "Why this is not fileable as it
stands" section before sending anything upstream.

## Environment

| | |
|---|---|
| host | Apple M3 Ultra, 80-core GPU, 256 GB, macOS 26.5.1 (25F80), Metal 4 |
| MoltenVK | `MVK_VERSION` 1.4.2, ICD `api_version` 1.4.0, `is_portability_driver` true |
| MoltenVK source | PR #2777 — `dttdrv/MoltenVK` branch `macgaming/mesh-shader` @ `4fc3f6c1f97c` (taskless mesh shaders; CTS 353/0) |
| SPIRV-Cross pin | `af71ba0bbfcc6896aad8d5744784b65da7f955b0` |
| MVK env | `MVK_CONFIG_LOG_LEVEL=0`, `VK_ICD_FILENAMES=<stage>/builds/moltenvk/Package/Latest/MoltenVK/dylib/macOS/MoltenVK_icd.json` |
| control host | NVIDIA RTX 3090, Linux 6.8, Vulkan SDK 1.3.296.0 |
| build | Release |

## What is measured

One procedural heightfield (1024x1024, generated in-process — no assets), 64 chunks of 144
meshlets, uploaded once into one SSBO. Two graphics pipelines consume that identical SSBO with
identical shader text and the identical camera:

- `vtx` — SSBO-pull vertex pipeline, non-indexed `vkCmdDrawIndirect`, 98304 verts/chunk
  (every interior corner is decoded ~6x).
- `mesh` — taskless `VK_EXT_mesh_shader` pipeline, direct `vkCmdDrawMeshTasksEXT`, no compute;
  each workgroup emits its meshlet's 144 unique corners once plus 242 local-index triangles.

Both are rendered offscreen into an RtGroup whose only varying property is the sample count
(1x / 2x / 4x), color `RGBA16F`, depth `D32_SFLOAT`. Timing is host wall-clock around one full
frame; the offscreen submit path does `vkQueueSubmit` + fence wait per frame, so each sample is
a complete GPU round trip. 30-40 frames per cell after warmup.

## Measured — MoltenVK / M3 Ultra

    res         msaa      vtx_ms    mesh_ms    ratio  vs 1x: vtx / mesh
    640x360     1x        1.7303     1.0509   0.6074   1.00x /  1.00x
    640x360     2x        1.8096     1.1897   0.6575   1.05x /  1.13x
    640x360     4x        2.0835     1.3982   0.6711   1.20x /  1.33x

    2560x1280   1x        2.0760     1.1583   0.5579   1.00x /  1.00x
    2560x1280   2x        2.1673     1.3561   0.6257   1.04x /  1.17x
    2560x1280   4x        2.2323     1.4963   0.6703   1.08x /  1.29x

    3840x2160   1x        2.1054     1.2008   0.5704   1.00x /  1.00x
    3840x2160   2x        2.3091     1.6438   0.7119   1.10x /  1.37x
    3840x2160   4x        2.3413     2.2099   0.9439   1.11x /  1.84x

At 4K the mesh pipeline pays **1.84x** for 4x MSAA while the vertex pipeline pays **1.11x** —
which read alone looks like a mesh-specific per-sample cost.

## Measured — NVIDIA 3090 control, same reproducer, same day

    res         msaa      vtx_ms    mesh_ms    ratio  vs 1x: vtx / mesh
    640x360     1x        1.8471     1.0838   0.5867   1.00x /  1.00x
    640x360     4x        2.0685     0.9880   0.4776   1.12x /  0.91x

    2560x1280   1x        1.8448     1.0069   0.5458   1.00x /  1.00x
    2560x1280   4x        2.1957     1.2980   0.5912   1.19x /  1.29x

    3840x2160   1x        1.8502     1.0083   0.5449   1.00x /  1.00x
    3840x2160   2x        2.4412     1.7287   0.7081   1.32x /  1.71x
    3840x2160   4x        2.6162     1.9397   0.7414   1.41x /  1.92x

## Why this is not fileable as it stands

At 3840x2160 / 4x, over identical geometry:

| | MoltenVK M3 Ultra | NVIDIA 3090 |
|---|---|---|
| mesh 1x -> 4x multiplier | 1.84x | **1.92x** |
| mesh 1x -> 4x absolute | +0.95 ms | +0.99 ms |
| vtx 1x -> 4x multiplier | 1.11x | 1.41x |
| vtx 1x -> 4x absolute | +0.27 ms | +0.87 ms |

The mesh pipeline's per-sample cost is **the same on both platforms**, in multiplier and in
absolute milliseconds. The apparent MoltenVK asymmetry comes from the denominator: the
vertex-pull baseline is barely affected by MSAA on Apple silicon (TBDR, on-chip MSAA resolve)
and substantially affected on the discrete GPU. A report claiming "mesh pipelines lose the
per-sample fast path on MoltenVK" would be contradicted by the control table in its own body.

## What would make it fileable

1. A workload where the MoltenVK mesh column climbs and the NVIDIA mesh column does not — the
   reproducer supports `--surcharge-vantage`, `--surcharge-res`, `--surcharge-format` and
   `--msaa-samples`, so payload size, coverage and format can be swept without new code.
2. Or an absolute per-sample cost far above what the same geometry costs on a comparable
   TBDR path (e.g. the same content drawn through a vertex pipeline into the same
   multisampled target), which this table does not show.
3. Or a Metal-level capture showing per-sample fragment invocation for the mesh pipeline where
   the vertex pipeline gets per-pixel invocation — that would name a mechanism rather than a
   ratio.

## Reproducer invocation

    # both platforms, asset-free, offscreen, ~10-80s
    ork.python ork.lev2/pyext/tests/llgfx/test_terrain_meshshader_ab.py \
        --reproduce --reproduce-frames 30

    # the committed regression gate (correctness + MESHPERF + the MSAA surcharge leg)
    ork.python ork.lev2/pyext/tests/llgfx/test_terrain_meshshader_ab.py

The gate emits `MESHMSAA_SURCHARGE ... surcharge=<ratio-of-ratios> baseline=.. fence=.. result=..`
and is fenced per platform against a recorded mean +/-15% (NVIDIA 1.293 -> 1.487, darwin 1.148 ->
1.320, both re-measured at the shipped meshlet 8) — it detects a regression in this behaviour, it
does not assert the behaviour is a driver bug. The darwin arm is no longer a known-bad ceiling:
after the meshlet 11 -> 8 reshape darwin measures BELOW NVIDIA on this metric.
