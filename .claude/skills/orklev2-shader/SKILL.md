---
name: orklev2-shader
description: Answer questions about orkid's FXV2 shader system, .fxv2 file format, uniform blocks, SSBOs, techniques/passes, shader compilation pipeline, SPIR-V transpilation, and built-in shaders. Use when the user asks about shaders, materials, GLSL, rendering techniques, or the shader language.
user-invocable: false
---

# Orkid FXV2 Shader System Reference

When answering questions about shaders in orkid, consult these files.

## Key Files

| Component | Location |
|-----------|----------|
| Shader Data Structures | `ork.lev2/inc/ork/lev2/gfx/shadman.h` |
| FX Pipeline | `ork.lev2/inc/ork/lev2/gfx/fx_pipeline.h` |
| Shader Loading (Vulkan) | `ork.lev2/src/gfx/vulkan/vulkan_fxi_load.cpp` |
| Shadlang Parser | `ork.lev2/src/gfx/shadlang/shadlang_impl.cpp` |
| SPIR-V Backend | `ork.lev2/src/gfx/shadlang/shadlang_backend_spirv.cpp` |
| Scanner Grammar | `ork.data/grammars/shadlang.scanner` |
| Parser Grammar | `ork.data/grammars/shadlang.parser` |
| Built-in Shaders | `ork.data/platform_lev2/shaders/fxv2/` |
| Include Libraries | `ork.data/platform_lev2/shaders/fxv2/*.i2` |

## FXV2 File Format

```glsl
// Configuration block
fxconfig fxcfg_default {
  glsl_version = "150";
  import "orkshader://mathtools.i2";
  import "orkshader://pbrtools.i2";
}

// Uniform block (maps to UBO)
uniform_block ublock_vtx (descriptor_set 0) {
  mat4 mvp;
  mat4 m;
  vec4 modcolor;
  float time;
}

// Sampler set
sampler_set samplers (descriptor_set 0) {
  sampler2D ColorMap;
  sampler2D NormalMap;
}

// Shader Storage Block (SSBO)
storage_interface sif_data (descriptor_set 0) {
  buffer layout(std430) my_data {
    vec4 items[$$$COUNT];
    vec4 params;
  };
}

// Vertex interface
vertex_interface iface_vtx : ublock_vtx {
  inputs {
    vec3 position : POSITION;
    vec3 normal : NORMAL;
    vec2 uv : TEXCOORD0;
  }
  outputs {
    vec4 out_color;
    vec2 out_uv;
  }
}

// Fragment interface
fragment_interface iface_frag : ublock_vtx : samplers {
  inputs {
    vec4 in_color;
    vec2 in_uv;
  }
  outputs {
    layout(location = 0) vec4 out_color;
  }
}

// Vertex shader
vertex_shader vs_main : iface_vtx {
  gl_Position = mvp * vec4(position, 1.0);
  out_color = modcolor;
  out_uv = uv;
}

// Fragment shader
fragment_shader ps_main : iface_frag {
  vec4 texel = texture(ColorMap, in_uv);
  out_color = texel * in_color;
}

// State block
state_block sb_default : default {
  DepthTest = LESS;
  DepthMask = true;
  CullTest = PASS_FRONT;
  BlendMode = ALPHA;
}

// Technique (combines shaders + state)
technique tek_forward {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader = vs_main;
    fragment_shader = ps_main;
    state_block = sb_default;
  }
}

// Compact form
technique tek_compact {
  fxconfig = fxcfg_default;
  vf_pass = { vs_main, ps_main, sb_default }
}
```

## Import System

`.i2` include files for shared code:
- `stdtools.i2` — standard library functions
- `mathtools.i2` — math utilities (hash, trig, color)
- `pbrtools.i2` — PBR-specific functions
- `fwdtools.i2` — forward rendering utilities
- `gbuftools.i2` — G-buffer utilities
- `envtools.i2` — environment map tools
- `particle_common.i2` — particle support

## Compilation Pipeline

```
.fxv2 file
  → Import expansion (recursive, circular detection)
  → Content hash computation (for caching)
  → PEG-based parser (shadlang grammar)
  → AST generation
  → SPIR-V transpilation (shadlang_backend_spirv.cpp)
  → VkShaderModule creation
  → Cache compiled binary by content hash
```

Cache bypass: `ORKID_DISABLE_SHADER_CACHE=1`

## C++ Data Structures

```
FxShader (program)
├── _techniques (map: name → FxShaderTechnique)
├── _parameterByName (map: name → FxShaderParam)
├── _uniformBlockByName (map: name → FxUniformBlock)
├── _storageBlockByName (map: name → FxShaderStorageBlock)
├── _samplerSets (map: name → FxSamplerSet)
└── _computeShaderByName (map: name → FxComputeShader)

FxShaderTechnique
├── _techniqueName
├── _passes (vector of FxShaderPass)
└── _shader (parent FxShader ptr)

FxShaderParam
├── _name, mParameterType
├── _blockinfo (parent uniform block)
└── _annotations
```

## Built-in Shaders

| Shader | Purpose |
|--------|---------|
| `basic.fxv2` | Basic vertex/fragment with material |
| `pbr.fxv2` | PBR rendering |
| `deferred.fxv2` | Deferred rendering (largest) |
| `compositor.fxv2` | Post-processing |
| `blit.fxv2` | Texture blitting |
| `particle.fxv2` | Particle system |
| `grid.fxv2` | Grid visualization |
| `prim_canvas.fxv2` | PrimCanvas drawing |

## Runtime Shader Loading

```cpp
// From file
FxShader* shader = ctx->FXI()->shaderFromFile("path/to/shader.fxv2");

// From string (runtime)
FxShader* shader = ctx->FXI()->shaderFromShaderText("name", shader_text);
```

## How to Answer

1. For shader syntax: read `.fxv2` files in `ork.data/platform_lev2/shaders/fxv2/`
2. For data structures: read `shadman.h`
3. For compilation: read `vulkan_fxi_load.cpp` and `shadlang_backend_spirv.cpp`
4. For shared functions: read the `.i2` include files
5. Always check actual shader files — the format is custom to orkid
