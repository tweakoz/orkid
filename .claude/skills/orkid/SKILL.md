---
name: orkid
description: Answer questions about the orkid media engine. Routes to specialized sub-skills for threading, reflection, filesystem, graphics, UI, audio, ECS, asset catalog, Python bindings, application framework, FSM, subsystems, and EzApp. Use for any orkid question including architecture overview, build system, or cross-cutting concerns.
user-invocable: false
---

# Orkid Media Engine Reference

Orkid is a C++ media engine with comprehensive Python bindings (pybind11). When answering questions about orkid, first identify which subsystem is relevant, then consult the corresponding skill's source files.

## Repository Structure

```
orkid/
├── ork.core/          — Core runtime: threading, reflection, filesystem, FSM, asset catalog
├── ork.lev2/          — Level 2: graphics, UI, audio (Singularity synth), EzApp
├── ork.ecs/           — Entity Component System
├── ork.data/          — Data files, shaders, test data
├── obt.project/       — Build tools, Python scripts, CLI tools
│   ├── bin/           — CLI tools (ork.catalog.tool.py, ork.catalog.import.py, etc.)
│   └── scripts/ork/   — Python modules (catalog_import.py, app/, ui/, editor/)
└── .claude/skills/    — These skill files
```

## Sub-Skill Index

| Skill | Covers | Key Files |
|-------|--------|-----------|
### ork.core Skills
| Skill | Covers | Key Files |
|-------|--------|-----------|
| **orkcore-math** | fvec2/3/4, fmtx3/4, fquat, DecompTransform, MultiCurve1D, Klein GA | `ork.core/inc/ork/math/` |
| **orkcore-varmap** | VarMap container, svar128_t variant, Python attribute access, nesting | `ork.core/inc/ork/kernel/varmap.inl`, `svariant.h` |
| **orkcore-threading** | OPQ, LockedResource, concurrent queues, task graphs, futures | `ork.core/inc/ork/kernel/opq.h`, `mutex.h`, `taskgraph.h` |
| **orkcore-reflection** | RTTI, property registration, serialization, signals/slots | `ork.core/inc/ork/reflect/`, `ork.core/inc/ork/object/` |
| **orkcore-filesystem** | Path, FileEnv, URI protocols, path expanders, DataBlock | `ork.core/inc/ork/file/` |
| **orkcore-fsm** | Hierarchical FSM, states, transitions, guards, FsmGroup | `ork.core/inc/ork/util/fsm.h` |
| **orkcore-catalog** | Asset catalog, manifests, chunked downloads, encryption, import | `ork.core/inc/ork/asset/catalog/` |
| **orkcore-dataflow** | Dataflow graphs, modules, plugs, topological sort, particles | `ork.core/inc/ork/dataflow/`, `ork.lev2/inc/ork/lev2/gfx/particle/` |
| **orkcore-python** | pybind11 patterns, type codec, GIL safety, factory patterns | `ork.core/inc/ork/python/` |
| **orkcore-application** | AppInitData, ComponentizedApplication, lifecycle, env vars | `ork.core/inc/ork/application/`, `obt.project/scripts/ork/app/` |
| **orkcore-subsystem** | Subsystem framework, dependency-driven init/shutdown, FSM lifecycle | `ork.core/inc/ork/application/subsystem.h` |
| **orkcore-logging** | Log channels, backends (stdout/file/HTML/HTTP), perf metrics | `ork.core/inc/ork/util/logger.h` |

### ork.lev2 Skills
| Skill | Covers | Key Files |
|-------|--------|-----------|
| **orklev2-gfxcontext** | Context class, sub-interfaces (GBI/TXI/FXI/DWI/CI/FBI), GfxEnv | `ork.lev2/inc/ork/lev2/gfx/gfxenv.h` |
| **orklev2-gbi** | GeometryBufferInterface, vertex/index buffers, vertex formats, draw | `ork.lev2/inc/ork/lev2/gfx/gbi.h` |
| **orklev2-txi** | TextureInterface, Texture/TextureArray, Image, sampling, ShmTex | `ork.lev2/inc/ork/lev2/gfx/txi.h` |
| **orklev2-fxi** | FxInterface, shader binding, params, UBO/SSBO, raster state | `ork.lev2/inc/ork/lev2/gfx/fxi.h` |
| **orklev2-dwi** | DrawingInterface, 2D/3D quads, lines, fullscreen quads | `ork.lev2/inc/ork/lev2/gfx/dwi.h` |
| **orklev2-ci** | ComputeInterface, dispatch, barriers, image binding | `ork.lev2/inc/ork/lev2/gfx/ci.h` |
| **orklev2-fbi** | FrameBufferInterface, RtGroup, viewport/scissor, capture | `ork.lev2/inc/ork/lev2/gfx/fbi.h` |
| **orklev2-shader** | FXV2 shader language, .fxv2 format, SPIR-V compilation, built-in shaders | `ork.data/platform_lev2/shaders/fxv2/`, `ork.lev2/inc/ork/lev2/gfx/shadman.h` |
| **orklev2-scenegraph** | Scene/Layer/Node, drawables, cameras, lighting, SceneGraphViewport | `ork.lev2/inc/ork/lev2/gfx/scenegraph/` |
| **orklev2-ui** | Widgets, layout, events, overlays, PropertySheet, Outliner, PrimCanvas | `ork.lev2/inc/ork/lev2/ui/` |
| **orklev2-audio** | Singularity synth, DSP, oscillators, filters, modulation, sampling | `ork.lev2/inc/ork/lev2/aud/singularity/` |
| **orklev2-coreaudio** | CoreAudio backend (macOS), AudioUnit graph, HAL callbacks | `ork.lev2/src/aud/coreaudio/` |
| **orklev2-alsa** | ALSA backend (Linux), dual-thread, PCM device, int16 output | `ork.lev2/src/aud/alsa/` |
| **orklev2-portaudio** | PortAudio backend (cross-platform), callback-based audio | `ork.lev2/src/aud/portaudio/` |
| **orklev2-pipewire** | PipeWire backend (Linux), SPA buffer, pw_stream callback | `ork.lev2/src/aud/pipewire/` |
| **orklev2-particles** | Dataflow particle system, emitters, forces, renderers, materials | `ork.lev2/inc/ork/lev2/gfx/particle/` |
| **orklev2-pbr** | PBR materials, IBL, environment maps, lightmaps, XGM serialization | `ork.lev2/inc/ork/lev2/gfx/material_pbr.inl` |
| **orklev2-fxpipeline** | FxPipeline, permutations, cache, parameter binding, state lambdas | `ork.lev2/inc/ork/lev2/gfx/fx_pipeline.h` |
| **orklev2-lighting** | Light types, LightManager, shadows, cookies, probes, SSBO layout | `ork.lev2/inc/ork/lev2/gfx/lighting/gfx_lighting.h` |
| **orklev2-renderer** | Drawables, DrawQueue, IRenderer, radix sort, instancing, skinning | `ork.lev2/inc/ork/lev2/gfx/renderer/drawable.h` |
| **orklev2-compositor** | Compositor pipeline, render/post-FX/output nodes, presets, RtGroup | `ork.lev2/inc/ork/lev2/gfx/renderer/compositor.h` |
| **orklev2-vulkan** | Vulkan backend, VkContext, swapchain, pipelines, sync, memory | `ork.lev2/src/gfx/vulkan/` |
| **orklev2-ezapp** | OrkEzApp, main loop, secondary windows, refresh policies | `ork.lev2/inc/ork/lev2/ezapp.h` |

### Other Skills
| Skill | Covers | Key Files |
|-------|--------|-----------|
| **orkecs-core** | ECS entities, components, systems, archetypes, simulation | `ork.ecs/inc/ork/ecs/` |
| **obt-build** | Build system, ork.build.py, CMake, ork.python, staging, env vars | `obt.project/bin/ork.build.py`, `CMakeLists.txt` |

## Cross-Cutting Patterns

### Smart Pointer Conventions
- `thing_ptr_t = std::shared_ptr<Thing>` (owning)
- `thing_constptr_t = std::shared_ptr<const Thing>` (const owning)
- `thing_wkptr_t = std::weak_ptr<Thing>` (non-owning)
- `thing_rawptr_t = Thing*` (unmanaged, used in Python bindings via `unmanaged_ptr<T>`)

### Variant Types
- `svar64_t` — 64-byte tagged variant (small values, annotations)
- `svar128_t` — 128-byte tagged variant (most property values)
- `svar160_t` — 160-byte variant (futures, larger values)
- `svar256_t` — 256-byte variant (large compound types)

### CrcString / Tokens
Hashed to `uint32_t` at compile time (C++) or init time (Python) for O(1) comparison.
```python
# Python
from orkengine.core import CrcStringProxy
tokens = CrcStringProxy()
pipeline.bindParam(param, tokens.RCFD_Camera_MVP_Mono)  # named provider
tex.setAddressMode(tokens.WRAP, tokens.WRAP, tokens.CLAMP)
```
```cpp
// C++
uint32_t id = "FORWARD_PBR"_crcu;
pipeline->bindParam(param, CrcString("RCFD_EYE_POSITION"));
```

### VarMap (Dynamic Key-Value)
```python
from orkengine.core import VarMap
vm = VarMap()
vm.key = value           # __setattr__
val = vm.key             # __getattr__
setattr(vm, "dotted.key", val)  # For dotted keys
```

### Python App Skeleton
```python
from orkengine import core, lev2
from ork.app.application import ComponentizedApplication

class MyApp(ComponentizedApplication):
  def __init__(self):
    super().__init__()
    self.createEzApp(width=1280, height=720)

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    # Build UI

  def _onGpuInit(self, ctx):
    self.uicontext = self.ezapp.uicontext
    # GPU resources

  def _onUpdate(self, updinfo):
    # Per-frame logic

app = MyApp()
app.ezapp.mainThreadLoop()
```

### Build System
- Uses `ork.build.py` (no args) from the orkid repo root
- Serial build: `ork.build.py --serial`
- CMake-based, automatic source file discovery (no manual CMakeLists edits for new .cpp/.h)
- Python environment: `ork.python` interpreter (includes orkengine modules)

### Environment Variables
- `ORKID_ASSET_MANIFEST_DIRS` — colon-separated manifest directories
- `ORKID_WORKSPACE_DIR` — workspace root
- `ASSETCACHE` — asset cache directory

## How to Answer

1. **Identify the subsystem** from the user's question
2. **Consult the matching sub-skill** — each lists key headers and patterns
3. **Read actual source files** before answering — training data may be outdated
4. **Cite file paths** so the user can follow up
5. For cross-subsystem questions, consult multiple skills
6. For build/env questions, check `obt.project/` scripts
