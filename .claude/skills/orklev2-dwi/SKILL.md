---
name: orklev2-dwi
description: Answer questions about orkid's DrawingInterface (DWI), 2D/3D quad rendering, line drawing, fullscreen quads, tiled quads, screen-space rendering helpers, and Python DWI bindings. Use when the user asks about 2D drawing, quad rendering, or screen-space primitives.
user-invocable: false
---

# Orkid DrawingInterface (DWI) Reference

When answering questions about 2D drawing, screen-space quads, or line rendering in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| DWI Interface | `inc/ork/lev2/gfx/dwi.h` (lines 9–56) |
| Vulkan DWI | `src/gfx/vulkan/headers/vulkan_ctx.h` (lines 139–142) |
| Python Bindings | `pyext/src/pyext_gfx.cpp` (lines 298–322) |

## DWI Interface (dwi.h:9–56)

The DrawingInterface provides methods for rendering 2D/3D quads and lines, primarily used for post-processing, UI overlays, and debug visualization.

### Line Drawing
- `line2DEML(v0, v1, vertex_color, depth)` — 2D line with fvec2 endpoints and fvec4 color

### 2D Quad Drawing (Screen-Space)

| Method | Description |
|--------|-------------|
| `quad2DEML(quadrect, uvA, uvB, depth)` | Basic screen quad with dual UV sets |
| `quad2DEML2(quadrect, uvA, uvB, depth)` | Variant 2 |
| `quad2DEMLCCL(quadrect, uvA, uvB, depth)` | CCL winding variant |
| `quad2D(quadrect, uvA, uvB, depth)` | Auto-winding (uses RenderingConventions) |
| `fullscreenQuad(uvA, uvB, depth)` | Fills entire viewport |

Parameters:
- `quadrect` — `fvec4(x, y, w, h)` in screen space
- `uvA`, `uvB` — `fvec4(u0, v0, u1, v1)` UV rectangles for two texture channels
- `depth` — Z depth value (default 0.0)

### 3D Quad Drawing
- `quad3DEML(V0, V1, V2, V3, Uv0, Uv1, Uv2, Uv3, color)` — 4 fvec3 vertices with per-vertex UV and uint32 color

### Advanced 2D Quad (Per-Vertex UVs)
- `quad2DEML(v0..v3, uvA0..uvA3, uvB0..uvB3, color, depth)` — 4 fvec2 vertices, 4+4 fvec2 UVs per channel, fvec4 color

### Tiled Quad Drawing
- `quad2DEMLTiled(quadrect, uvA, uvB, numtiles)` — subdivided quad (legacy)
- `quadTiled2D(quadrect, uvA, uvB, numtiles)` — auto-winding tiled quad

### Notes
- **EML** suffix = "Eulerian-like Materials" — direct draw, bypasses material pass management
- **Auto-winding** methods (`quad2D`, `fullscreenQuad`, `quadTiled2D`) adjust vertex order based on `Context::renderingConventions()` to handle Y-flip differences between backends
- All quad methods use the `SVtxV12C4T16` vertex format internally (position + color + 2 UV sets)

## Typical Usage

```cpp
// Post-processing fullscreen pass
auto* DWI = ctx->DWI();
auto* FXI = ctx->FXI();

FXI->BeginBlock(technique, rcid);
DWI->fullscreenQuad();  // Renders a screen-filling quad
FXI->EndBlock();

// Debug overlay quad
DWI->quad2D(
    fvec4(10, 10, 200, 200),     // screen rect
    fvec4(0, 0, 1, 1),           // UV channel A
    fvec4(0, 0, 1, 1),           // UV channel B
    0.0f                          // depth
);
```

## Python API

```python
dwi = ctx.DWI

# Fullscreen quad (most common for post-processing)
dwi.fullscreenQuad()
dwi.fullscreenQuad(fvec4(0,0,1,1), fvec4(0,0,1,1), 0.0)

# Screen-space quad
dwi.quad2D(fvec4(x, y, w, h))
dwi.quad2D(fvec4(x, y, w, h), fvec4(u0,v0,u1,v1), fvec4(u0,v0,u1,v1), depth)
```

## How to Answer

1. For DWI methods: check `dwi.h:9–56`
2. For winding conventions: check `RenderingConventions` in `gfxenv.h:166–182`
3. For vertex format used: `SVtxV12C4T16` in `gfxvtxbuf_structs.h`
4. For Python: check `pyext_gfx.cpp:298–322`
