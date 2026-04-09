---
name: orklev2-particles
description: Answer questions about orkid's dataflow-based particle system, emitters (nozzle/ring/line/elliptical), forces (gravity/turbulence/vortex/drag/attractors), renderers (sprite/streak/light), particle materials, pool management, and Python particle bindings. Use when the user asks about particles, emitters, forces, or particle rendering.
user-invocable: false
---

# Orkid Particle System Reference

When answering questions about particles in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | Header | Implementation |
|-----------|--------|----------------|
| Particle Core | `inc/ork/lev2/gfx/particle/particle.h` | |
| Modular System | `inc/ork/lev2/gfx/particle/modular_particles2.h` | `src/gfx/particles/modular_particles2.cpp` |
| Emitters | `inc/ork/lev2/gfx/particle/modular_emitters.h` | `src/gfx/particles/modules_emitter_*.cpp` |
| Forces | `inc/ork/lev2/gfx/particle/modular_forces.h` | `src/gfx/particles/modules_force_*.cpp`, `modules_attractor_*.cpp` |
| Renderers | `inc/ork/lev2/gfx/particle/modular_renderers.h` | `src/gfx/particles/modules_renderer_*.cpp` |
| Materials | (in modular_renderers.h) | `src/gfx/particles/renderer_materials.cpp` |
| Drawable Integration | `inc/ork/lev2/gfx/particle/drawable_data.h` | |
| Pool Module | | `src/gfx/particles/modules_pool.cpp` |
| Global Module | | `src/gfx/particles/modules_global.cpp` |
| Plug Types | | `src/gfx/particles/particle_plugs.cpp` |
| Python Bindings | | `pyext/src/pyext_gfx_particles.cpp` |
| Unit Test | | `unittests/particles.cpp` |
| Example | | `examples/python/scenegraph/particles1.py` |

## Architecture Overview

The particle system is **dataflow-graph-based** — particles are processed by composable modules connected via typed plugs in a topologically-sorted computation graph.

### Signal Flow
```
GlobalModule (time) ──→ Pool (allocate/age) ──→ Emitter (spawn)
                                                    │
                                              ──→ Forces (modify velocity)
                                                    │
                                              ──→ Renderer (draw)
```

### Module Lifecycle
1. **Data Phase** (`ModuleData`): created with `createShared()`, plugs defined in `_reshapeIOs()`
2. **Instance Phase** (`ModuleInst`): `createInstance(GraphInst*)` → `onLink()` → per-frame `compute()`
3. **Execution**: topological sort ensures dependencies resolve before dependents

## Class Hierarchy

```
dflow::DgModuleData
└── ModuleData (abstract, modular_particles2.h:72)
    ├── GlobalModuleData (time/noise outputs)
    └── ParticleModuleData (abstract, has pool in/out plugs)
        ├── ParticlePoolData (pool allocation, particle aging)
        ├── Emitters
        │   ├── NozzleEmitterData (cone/nozzle)
        │   ├── RingEmitterData (rotating ring)
        │   ├── LineEmitterData (line segment P1→P2)
        │   └── EllipticalEmitterData (parametric ellipse surface)
        ├── Forces
        │   ├── GravityModuleData (newtonian gravity)
        │   ├── TurbulenceModuleData (noise perturbation)
        │   ├── VortexModuleData (swirl force)
        │   ├── DragModuleData (velocity damping)
        │   ├── SphAttractorModuleData (spherical attractor)
        │   ├── EllipticalAttractorModuleData
        │   └── PointAttractorModuleData
        └── Renderers
            ├── SpriteRendererData (billboard sprites, 128K max)
            ├── StreakRendererData (motion trails, 128K max)
            └── LightRendererData (point lights per particle)

Object
└── MaterialBase (abstract, modular_renderers.h:30)
    ├── FlatMaterial (solid color)
    ├── GradientMaterial (age-based gradient + modulation texture)
    ├── TextureMaterial (single texture)
    ├── TexGridMaterial (animated grid texture)
    └── VolTexMaterial (volume texture)
```

## Plug Types

| Plug Type | Used For | Max Fanout |
|-----------|----------|------------|
| `ParticleBufferPlugTraits` | Pool data (particle buffer) | 1 |
| `FloatXfPlugTraits` | Scalar params (rate, lifespan, size) | — |
| `Vec3XfPlugTraits` | Vector params (position, direction) | — |
| `FloatPlugTraits` | Simple floats (UnitAge output) | — |

## Emitter Parameters

All emitters share common inputs via `FloatXfPlugTraits`:
- **LifeSpan** (0–20 sec), **EmissionRate** (0–1000/sec), **EmissionVelocity** (-100–100), **DispersionAngle** (0–1 rad)

| Emitter | Extra Inputs |
|---------|-------------|
| Nozzle | Offset (vec3), Direction (vec3) |
| Ring | EmissionRadius, EmitterSpinRate, Direction, Offset |
| Line | P1, P2 (vec3 endpoints) |
| Elliptical | Scalar, MinU/MaxU, MinV/MaxV, P1, P2 |

## Force Parameters

| Force | Inputs | Formula |
|-------|--------|---------|
| Gravity | G, Mass, OthMass, MinDistance, Center | F = (mass * othmass * G) / dist^2 |
| Turbulence | Amount (vec3) | Noise-based perturbation |
| Vortex | VortexStrength, OutwardStrength, Falloff | Swirl force |
| Drag | (scalar factor) | Velocity damping |
| SphAttractor | Radius, Inertia, Dampening | Spherical attraction |
| EllipticalAttractor | Inertia, Dampening, Scalar | Elliptical volume |
| PointAttractor | (point position) | Single-point attraction |

## Pool Management

`ParticlePoolModuleInst::compute()` (modules_pool.cpp:26):
- Updates all alive particles each frame: `age += dt`, `position += velocity * dt`
- Reaps particles when `age > lifespan`
- Pool re-initializes if size changes at runtime

## Renderer Details

### Sprite Renderer
- Billboard quads, vertex format `SVtxV12N12B12T16`
- **Triple-buffered** for thread-safe CPU→GPU sync
- Input: Size (float), Material (any MaterialBase subclass)
- Optional depth sorting

### Streak Renderer
- Line segments from current to previous position
- Inputs: Length, Width, Scale (floats)
- Same vertex format and triple-buffering as sprites

## Python API

```python
from orkengine.lev2 import particles, dflow

# Build dataflow graph
graphdata = dflow.GraphData.createShared()

# Create modules
ptc_pool = graphdata.create("POOL", particles.Pool)
emitter  = graphdata.create("EMIT", particles.NozzleEmitter)
gravity  = graphdata.create("GRAV", particles.Gravity)
renderer = graphdata.create("REND", particles.SpriteRenderer)

# Configure
ptc_pool.pool_size = 16384
emitter.inputs.LifeSpan = 10
emitter.inputs.EmissionRate = 800
emitter.inputs.EmissionVelocity = 5

# Connect pipeline: pool → emitter → gravity → renderer
graphdata.connect(emitter.inputs.pool, ptc_pool.outputs.pool)
graphdata.connect(gravity.inputs.pool, emitter.outputs.pool)
graphdata.connect(renderer.inputs.pool, gravity.outputs.pool)

# Materials
mat = particles.GradientMaterial.createShared()
mat.colorIntensity = 2.0
renderer.material = mat
renderer.depth_sort = True

# Create drawable for scene graph
drawable_data = lev2.ParticlesDrawableData()
drawable_data.graphdata = graphdata
```

### Available Python Classes

**Modules:** `Pool`, `Globals`, `NozzleEmitter`, `RingEmitter`, `LineEmitter`, `EllipticalEmitter`, `Gravity`, `Turbulence`, `Vortex`, `Drag`, `SphAttractor`, `EllipticalAttractor`, `PointAttractor`, `SpriteRenderer`, `StreakRenderer`, `LightRenderer`

**Materials:** `FlatMaterial`, `GradientMaterial`, `TextureMaterial`, `TexGridMaterial`, `VolTexMaterial`

## How to Answer

1. For module types: check `modular_emitters.h`, `modular_forces.h`, `modular_renderers.h`
2. For plug wiring: check `modular_particles2.h` for plug traits, `_reshapeIOs()` in each module's .cpp
3. For rendering: check `modules_renderer_sprite.cpp` and `renderer_materials.cpp`
4. For Python usage: check `pyext_gfx_particles.cpp` and `examples/python/scenegraph/particles1.py`
5. For dataflow fundamentals: consult the **orkcore-dataflow** skill
