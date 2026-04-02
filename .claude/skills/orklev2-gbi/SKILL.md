---
name: orklev2-gbi
description: Answer questions about orkid's GeometryBufferInterface (GBI), vertex buffers, index buffers, vertex formats (SVtxV12C4T16, SVtxV12N12B12T8C4, etc.), VtxWriter, draw primitives, PrimitiveType, dynamic/static buffers, and Python geometry bindings. Use when the user asks about vertex buffers, geometry, draw calls, or vertex formats.
user-invocable: false
---

# Orkid GeometryBufferInterface (GBI) Reference

When answering questions about geometry, vertex buffers, or draw calls in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| GBI Interface | `inc/ork/lev2/gfx/gbi.h` (lines 18–152) |
| Vertex/Index Buffers | `inc/ork/lev2/gfx/gfxvtxbuf.h` |
| Vertex Format Structs | `inc/ork/lev2/gfx/gfxvtxbuf_structs.h` |
| Vertex Buffer Inline | `inc/ork/lev2/gfx/gfxvtxbuf.inl` |
| Enums (formats, primitives) | `inc/ork/lev2/gfx/gfxenv_enum.h` |
| Base Implementation | `src/gfx/context/gbi.cpp` |
| Vulkan GBI | `src/gfx/vulkan/vulkan_gbi.cpp` |
| Python Bindings | `pyext/src/pyext_gfx.cpp` (lines 259–370) |

## GBI Interface (gbi.h:18–152)

### Vertex Buffer Operations
- `LockVB(VBuf, ivbase, icount)` → `void*` — lock for writing
- `UnLockVB(VBuf)` — unlock after writing
- `ReleaseVB(VBuf)` — release GPU resources
- `FlushVB(VBuf)` — flush to GPU

### Index Buffer Operations
- `LockIB(IBuf, ibase, icount)` → `void*`
- `UnLockIB(IBuf)`
- `ReleaseIB(IBuf)`

### Draw Methods (EML — direct, no material management)
- `DrawPrimitiveEML(VBuf, type, ivbase, ivcount)` — from vertex buffer
- `DrawPrimitiveEML(SSBO, type, ivbase, ivcount)` — from storage buffer
- `DrawIndexedPrimitiveEML(VBuf, IdxBuf, type)` — indexed draw
- `DrawInstancedIndexedPrimitiveEML(VBuf, IdxBuf, type, instance_count)` — instanced
- `DrawInstancedIndexedPrimitiveEML(VBuf, IdxBuf, type, instance_count, first_instance)` — with offset

### Draw Methods (with material)
- `DrawPrimitive(GfxMaterial*, VtxWriter, type, icount)` — from writer
- `DrawPrimitive(GfxMaterial*, VBuf, type, ivbase, ivcount)` — from buffer
- `DrawIndexedPrimitive(GfxMaterial*, VBuf, IdxBuf, type)` — indexed

### 2D Helpers
- `render2dQuadEML(quadrect, uvrecta, uvrectb, depth)` — screen-space quad
- `render2dQuadEMLCCL(quadrect, uvrecta, uvrectb, depth)` — CCL variant

## PrimitiveType (gfxenv_enum.h:165–178)

| Type | Description |
|------|-------------|
| `POINTS` | Point list |
| `LINES` | Line list |
| `LINESTRIP` | Connected line strip |
| `LINELOOP` | Closed line loop |
| `TRIANGLES` | Triangle list |
| `TRIANGLESTRIP` | Triangle strip |
| `TRIANGLEFAN` | Triangle fan |
| `QUADS` | Quad list |
| `PATCHES` | Tessellation patches |

## Vertex Format Structs (gfxvtxbuf_structs.h)

| Struct | BPV | Components |
|--------|-----|-----------|
| `VtxV12C4` | 16 | position(3f) + color(u32) |
| `SVtxV12C4T16` | 32 | position(3f) + color(u32) + uv0(2f) + uv1(2f) |
| `SVtxV12N12B12T8C4` | 48 | position(3f) + normal(3f) + binormal(3f) + uv(2f) + color(u32) |
| `SVtxV12N12B12T16` | 52 | position(3f) + normal(3f) + binormal(3f) + uv0(2f) + uv1(2f) |
| `SVtxV12N12T8I4W4` | 40 | position(3f) + normal(3f) + uv(2f) + boneIndices(u32) + boneWeights(u32) |
| `SVtxV16T16C16` | 48 | position(4f) + texcoord(4f) + color(4f) |

### EVtxStreamFormat enum (gfxenv_enum.h:350–401)
Compact format identifiers: `V12`, `V12C4`, `V12T8`, `V12C4T16`, `V12N12B12T8C4`, `V12N12B12T16`, `V12N12T8I4W4`, `V16T16C16`, `VU32`, `VU32INST`, etc.

## Vertex Buffers (gfxvtxbuf.h)

### VertexBufferBase (line 79)
- `GetMax()`, `GetNumVertices()`, `GetVtxSize()`
- `GetStreamFormat()` → `EVtxStreamFormat`
- `IsStatic()` — pure virtual
- `IsLocked()`, `Lock()`, `Unlock()`
- `CreateVertexBuffer(format, numverts, static)` — factory

### Typed Buffers
- `CVtxBuffer<T>` — base typed buffer
- `StaticVertexBuffer<T>` — GPU-resident, write-once
- `DynamicVertexBuffer<T>` — CPU-writable each frame

### Common Aliases
```cpp
using dvb_V12C4T16 = DynamicVertexBuffer<SVtxV12C4T16>;
using dvb_V12N12B12T8C4 = DynamicVertexBuffer<SVtxV12N12B12T8C4>;
using dvb_V16T16C16 = DynamicVertexBuffer<SVtxV16T16C16>;
```

## Index Buffers (gfxvtxbuf.h:26–75)

- `IndexBufferBase` — base with `GetNumIndices()`, `indexSize()`
- `StaticIndexBuffer<T>` — write-once
- `DynamicIndexBuffer<T>` — per-frame writable
- Index types: typically `uint16_t` or `uint32_t`

## VtxWriter (gfxvtxbuf.h:140–163)

Dynamic geometry helper for building vertex data:

```cpp
VtxWriter<SVtxV12C4T16> writer;
writer.Lock(ctx, &dynVB, vertexCount);
auto& vtx = writer.RefAndIncrement();
vtx._position = fvec3(x, y, z);
vtx._color = 0xFFFFFFFF;
vtx._uv0 = fvec2(u, v);
writer.UnLock(ctx);

ctx->GBI()->DrawPrimitiveEML(writer, PrimitiveType::TRIANGLES);
```

Members: `miWriteBase`, `miWriteCounter`, `miWriteMax`, `mpBase`, `mpVB`

## Python API

```python
gbi = ctx.GBI

# VtxWriter
writer = lev2.Writer_V12N12B12T8C4()

# Lock VB for raw access
memview = gbi.lockVB(vb, base, count)  # Returns memoryview
gbi.unlockVB(vb)

# Draw from writer
gbi.drawTriangles(writer)
gbi.drawTriangleStrip(writer)
gbi.drawLines(writer)
```

## How to Answer

1. For GBI methods: check `gbi.h:18–152`
2. For vertex formats: check `gfxvtxbuf_structs.h` — search by struct name
3. For buffer types: check `gfxvtxbuf.h` (Static/Dynamic)
4. For primitive types: check `gfxenv_enum.h:165–178`
5. For Vulkan mapping: check `vulkan_gbi.cpp` (topology + vertex stream config)
6. For Python: check `pyext_gfx.cpp:259–370`
