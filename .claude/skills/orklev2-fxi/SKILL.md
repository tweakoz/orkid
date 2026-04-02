---
name: orklev2-fxi
description: Answer questions about orkid's FxInterface (FXI), shader loading, technique/pass management, parameter binding (scalar/vector/matrix/texture), uniform blocks (UBO), storage buffers (SSBO), descriptor sets, raster state, compute shader access, and Python FXI bindings. Use when the user asks about shader binding, FXI, parameter upload, or uniform/storage buffers.
user-invocable: false
---

# Orkid FxInterface (FXI) Reference

When answering questions about shader parameter binding, FXI operations, or UBO/SSBO management in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| FXI Interface | `inc/ork/lev2/gfx/fxi.h` (lines 21–135) |
| Shader System Types | `inc/ork/lev2/gfx/shadman.h` |
| Vulkan FXI | `src/gfx/vulkan/headers/vulkan_ctx.h` (lines 462–582) |
| Python Shader Bindings | `pyext/src/pyext_gfx_shader.cpp` |
| Python Material Bindings | `pyext/src/pyext_gfx_material.cpp` |

## FXI Interface (fxi.h:21–135)

### Frame & Block Lifecycle
- `BeginFrame()` / `EndFrame()` — frame boundaries
- `BeginBlock(technique, RCID)` → int (pass count)
- `EndBlock()` — end technique block
- `CommitParams()` — flush parameter changes to GPU
- `reset()` — reset state

### Shader/Technique/Parameter Lookup
- `technique(shader, name)` → `FxShaderTechnique*`
- `parameter(shader, name)` → `FxShaderParam*`
- `uniformBlock(shader, name)` → `FxUniformBlock*`
- `samplerSet(shader, name)` → `fxsamplerset_constptr_t`
- `computeShader(shader, name)` → `FxComputeShader*`
- `storageBlock(shader, name)` → `fxparamstorageblock_constptr_t`
- `findStorageMember(block, member_name)` → `fxbuffer_member_constptr_t`

### Scalar/Vector Parameter Binding
| Method | Type |
|--------|------|
| `bindParamBool(param, bool)` | bool |
| `bindParamInt(param, int)` | int |
| `bindParamFloat(param, float)` | float |
| `bindParamU32(param, uint32_t)` | uint32 |
| `bindParamU64(param, uint64_t)` | uint64 |
| `bindParamVect2(param, fvec2)` | vec2 |
| `bindParamVect3(param, fvec3)` | vec3 |
| `bindParamVect4(param, fvec4)` | vec4 |

### Array Parameter Binding
| Method | Type |
|--------|------|
| `bindParamFloatArray(param, floats, count)` | float[] |
| `bindParamVect2Array(param, vecs, count)` | vec2[] |
| `bindParamVect3Array(param, vecs, count)` | vec3[] |
| `bindParamVect4Array(param, vecs, count)` | vec4[] |

### Matrix Binding
| Method | Type |
|--------|------|
| `bindParamMatrix(param, fmtx4)` | mat4 |
| `bindParamMatrix(param, fmtx3)` | mat3 |
| `bindParamMatrixArray(param, mats, count)` | mat4[] |

### Texture Binding
| Method | Type |
|--------|------|
| `bindParamTexture(param, Texture*)` | single texture |
| `bindParamTextureArray(param, TextureArray*)` | texture array |
| `bindParamTextureList(param, rawlist)` | texture list |

### Storage Buffer (SSBO) Management
- `createStorageBuffer(length)` → `FxShaderStorageBuffer*`
- `mapStorageBuffer(ssbo, base, length, access)` → mapping ptr
- `unmapStorageBuffer(mapping)`
- `copyBufferIntoStorageBuffer(ssbo, buffer, dest_offset)`
- `bindStorageBuffer(block, buffer)` — bind to shader block

### Uniform Buffer (UBO) Management
- `createUniformBuffer(length)` → `FxUniformBuffer*`
- `mapUniformBuffer(ubo, base, length)` → mapping ptr
- `unmapUniformBuffer(mapping)`
- `bindUniformBuffer(block, buffer)` — bind to shader block

### Shader Loading
- `LoadFxShader(path, shader)` → bool
- `shaderFromShaderText(name, text)` → `FxShader*`

### Raster State
- `pushRasterState(rs)` / `popRasterState()` → `rasterstate_ptr_t`
- `applyRasterState(rstate)` — immediate application

### Descriptor Sets (Modern API)
- `numDescriptorSetBindPoints(technique)` → size_t
- `descriptorSetBindPoint(technique, slot)` → bind point

## Shader System Types (shadman.h)

### FxShaderTechnique (line 59)
- `_techniqueName` — identifier
- `_passes` — vector of `FxShaderPass*`
- `_validated` — compilation status

### FxShaderParam (line 74)
- `_name` — parameter name
- `meParamType` — `EPropType` (float, vec3, texture, etc.)
- `mBindable` — can be bound at runtime
- `_annotations` — shader metadata

### FxUniformBlock (line 90)
- `_name` — block name (e.g., "PerFrame", "PerObject")
- `_subparams` — named members

### FxShaderStorageBlock (line 170)
- `_name` — storage block name
- `_buffer_size` — total size in bytes
- `_members` — map of `FxBufferMember`

### FxBufferMember (line 146)
- `_name`, `_datatype` — member identity ("vec4", "mat4", "uint")
- `_offset`, `_size`, `_stride` — layout info
- `_is_array`, `_array_length` — array support

### FxShader
Master container with maps of techniques, parameters, blocks, compute shaders.

## Python API

```python
fxi = ctx.FXI

# Shader loading
shader = lev2.FxShader()
fxi.loadShader(shader, "orkshader://pbr")
# or via FreestyleMaterial:
mtl = lev2.FreestyleMaterial(ctx, "orkshader://custom")

# Lookup
tek = mtl.technique("tek_name")
par = mtl.param("ParamName")
ublock = mtl.uniblk("BlockName")
storage = mtl.storage("StorageName")
compute = mtl.computeShader("cs_name")

# Direct binding
mtl.bindParamFloat(par, 1.0)
mtl.bindParamVec4(par, fvec4(1,2,3,4))
mtl.bindParamMatrix(par, fmtx4())
mtl.bindParamTexture(par, texture)

# Begin/end technique
passes = mtl.begin(tek, rcfd)
# ... draw ...
mtl.end(rcfd)
```

## How to Answer

1. For FXI methods: check `fxi.h:21–135`
2. For shader types (technique, param, block): check `shadman.h`
3. For SSBO layout: check `FxBufferMember` in `shadman.h:146–165`
4. For Vulkan implementation: check `vulkan_ctx.h:462–582`
5. For Python: check `pyext_gfx_shader.cpp` and `pyext_gfx_material.cpp`
6. For pipeline-level binding (higher level): consult **orklev2-fxpipeline** skill
