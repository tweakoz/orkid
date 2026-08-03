---
name: orkcore-dataflow
description: Answer questions about orkid's dataflow graph system, modules, plugs, connections, topological sorting, register allocation, particle system integration, and Python dataflow bindings. Use when the user asks about dataflow graphs, plug connections, particle modules, or graph execution.
user-invocable: false
---

# Orkid Dataflow Graph System Reference

When answering questions about the dataflow system in orkid, consult these files.

Full current architecture: `ork.dox/core/dataflow.md` (accuracy-passed 2026-08-01, incl. diagrams).

## What's new (2026-06/07)

- Composite modules `SubGraphModuleData` / `LoopModuleData` — nested GraphData with promoted boundary plugs, loop carries, index feeds (`ork.core/inc/ork/dataflow/subgraph_module.h`)
- `CookGraphDriver` — family-neutral hook interface the composite runtime delegates nested cook/bake execution to (`ork.core/inc/ork/dataflow/cook_driver.h`)
- Per-node Merkle cook cache — opt-in via `GraphData::_cacheable`; modules opt in per class via `DgModuleInst::cookComputeHash` / `cookStore` / `cookLoad`; `GraphInst::computeNodeHashes()` + `cachedCompute()`
- `TypeKeyedVars _impl` on GraphData AND GraphInst — per-family runtime storage keyed by type, so families (hypermesh/terrain/particles) coexist on one graph
- Bypass (`DgModuleData::_bypassed`) + select-as-output (`GraphData::_output_node`) — first-class serialized graph state; bypassed modules splice out via `resolveConnectedOutput()`
- Python introspection `dflow.plugSpec()` / `dflow.moduleClasses()` — plug schemas and node registry straight from reflection (`ork.core/pyext/pyext_dataflow.cpp`)
- Generic family-neutral modules Min/Max/Lerp/Pow/Vec4Combine (`ork.core/inc/ork/dataflow/basic_modules.h`)

## Key Files

| Component | Location |
|-----------|----------|
| Main Types | `ork.core/inc/ork/dataflow/dataflow.h` |
| Module Definitions | `ork.core/inc/ork/dataflow/module.h` |
| Module Templates | `ork.core/inc/ork/dataflow/module.inl` |
| Composite Modules (SubGraph/Loop) | `ork.core/inc/ork/dataflow/subgraph_module.h` |
| Cook Driver Seam | `ork.core/inc/ork/dataflow/cook_driver.h` |
| Generic Modules (Min/Max/Lerp/Pow/Vec4Combine) | `ork.core/inc/ork/dataflow/basic_modules.h` |
| Plug Data | `ork.core/inc/ork/dataflow/plug_data.h` |
| Plug Templates | `ork.core/inc/ork/dataflow/plug_data.inl` |
| Plug Instances | `ork.core/inc/ork/dataflow/plug_inst.h` |
| Enums | `ork.core/inc/ork/dataflow/enum.h` |
| Scheduler | `ork.core/inc/ork/dataflow/scheduler.h` |
| GraphData Impl | `ork.core/src/dataflow/graph_data.cpp` |
| GraphInst Impl | `ork.core/src/dataflow/graph_inst.cpp` |
| Topology Sorter | `ork.core/src/dataflow/dataflow_sorter.cpp` |
| Register Context | `ork.core/src/dataflow/dataflow_context.cpp` |
| Python Bindings | `ork.core/pyext/pyext_dataflow.cpp` |
| Python Test | `ork.core/pyext/tests/dataflow.py` |
| Particle Modules | `ork.lev2/inc/ork/lev2/gfx/particle/modular_particles2.h` |
| Particle Emitters | `ork.lev2/inc/ork/lev2/gfx/particle/modular_emitters.h` |
| Particle Forces | `ork.lev2/inc/ork/lev2/gfx/particle/modular_forces.h` |
| Particle Renderers | `ork.lev2/inc/ork/lev2/gfx/particle/modular_renderers.h` |

## Architecture Overview

### Graph Structure (DAG)
- **GraphData** — static topology: modules, plugs, connections (serializable); plus graph-level state `_cacheable`, `_output_node`, `_editor_layout`, and a `TypeKeyedVars _impl` slot
- **GraphInst** — runtime: module instances, execution state, register allocations; also carries a `TypeKeyedVars _impl` (per-family environments, cook drivers)
- **Topology** — topologically sorted execution order
- **DgSorter** — computes sort with register allocation
- **dgcontext** — register pool management per data type

Graphs need not be flat: composite SubGraph/Loop modules nest a full GraphData inside a module (see subgraph_module.h).

### Module System
```
DgModuleData (base, serializable; carries _bypassed + cachepoint/viewable markers)
├── LambdaModuleData (runtime-defined compute)
├── SubGraphModuleData (composite: nested GraphData, promoted boundary plugs)
│   └── LoopModuleData (iteration count, carries, index feeds)
├── ParticleModuleData (particle system nodes)
└── [Custom subclasses]

DgModuleInst (runtime)
├── onLink(), onStage(), onActivate()
└── compute(graphinst, updatedata)
```

### Plug Types and Traits
Input/output plugs are typed via traits templates:

| Traits | Data Type | Transform Chain |
|--------|-----------|----------------|
| `FloatPlugTraits` | float | none |
| `Vec3fPlugTraits` | fvec3 | none |
| `QuatfPlugTraits` | fquat | none |
| `FloatXfPlugTraits` | float | transform chain |
| `Vec3XfPlugTraits` | fvec3 | transform chain |
| `QuatXfPlugTraits` | fquat | transform chain |

**Plug Rates:**
- `EPR_EVENT` — constant during event
- `EPR_UNIFORM` — constant per compute call
- `EPR_VARYING1` / `EPR_VARYING2` — per-item variation

### Float Transform Chain (Xf plugs)
Input plugs with `Xf` traits can apply a chain of transforms:
- `mod` (modulo), `scale`, `bias`, `sine`, `abs`, `smoothstep`
- `quantize`, `power`, `multicurve` (spline interpolation)

### Register Machine Model
- Output plug results stored in `DgRegister` objects
- Registers pooled per data type (`DgRegisterBlock`)
- Allocated during topological sort, freed when downstream modules complete
- Minimizes memory footprint

### Execution Flow
```
1. GraphData.createShared()           — create graph
2. graphdata.create("name", Class)    — add modules
3. module.createUniform*Plug("name")  — add plugs
4. graphdata.connect(input, output)   — wire connections
5. DgSorter.generateTopology()        — sort + register alloc
6. graphdata.createGraphInst()        — instantiate
7. graphinst.bindTopology(topo)       — bind execution order (forwards to updateTopology: link/stage/activate)
8. graphinst.compute(updatedata)      — execute in order
```

When `GraphData::_cacheable` is set, `cachedCompute()` runs instead: per-node Merkle hashes, cache hit = `cookLoad()` and skip compute (see the dox for the full contract). Realtime families (particles) leave it false.

### Python Usage
```python
from orkengine.core import dataflow

# Define custom module
class MyModule(dataflow.PyLambdaModule):
  def onCompute(self, graphinst, updatedata):
    # Read inputs, compute, write outputs
    pass

# Build graph
graphdata = dataflow.GraphData.createShared()
a = graphdata.create("gen", MyModule)
b = graphdata.create("proc", dataflow.LambdaModule)

aout = a.createUniformFloatOutputPlug("value")
binp = b.createUniformFloatXfInputPlug("input")
graphdata.connect(binp, aout)

# Sort and execute
ctx = dataflow.DgContext.createShared()
ctx.createFloatRegisterBlock("floats", 16)
sorter = dataflow.DgSorter.createShared(graphdata, ctx)
topo = sorter.generateTopology()

graphinst = graphdata.createGraphInst()
graphinst.bindTopology(topo)
graphinst.compute(update_data)
```

### Particle System Integration
Particle modules extend `DgModuleData` with `ParticleBuffer` plugs:

```
Emitter (Ring/Nozzle/Line/Elliptical)
  ↓ ParticleBuffer
Force (Gravity/Turbulence/Vortex/Drag/Attractor)
  ↓ ParticleBuffer
Renderer (Sprite/Streak/Light)
  ↓ vertex data to GPU
```

Emitter types: `RingEmitterData`, `NozzleEmitterData`, `LineEmitterData`, `EllipticalEmitterData`

Force types: `GravityModuleData`, `TurbulenceModuleData`, `VortexModuleData`, `DragModuleData`, `SphAttractorModuleData`, `PointAttractorModuleData`

Renderer types: `SpriteRendererData`, `StreakRendererData`, `LightRendererData`

### Data/Instance Separation
- `*Data` classes — static definition (serializable, shared)
- `*Inst` classes — runtime state (per-graph-instance)

## How to Answer

1. For graph construction: read `graph_data.cpp` and `pyext_dataflow.cpp`
2. For plug types: read `plug_data.h` for traits, `plug_data.inl` for templates
3. For execution order: read `dataflow_sorter.cpp` for topological sort algorithm
4. For particles: read `modular_particles2.h` and specific emitter/force/renderer headers
5. For Python API: check `pyext_dataflow.cpp` and `tests/dataflow.py`
6. For composites, cook cache, bypass, or introspection: read `subgraph_module.h`, `cook_driver.h`, `module.h` (cook hooks), and `ork.dox/core/dataflow.md`
