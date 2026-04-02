---
name: orkecs-core
description: Answer questions about orkid's ECS (Entity Component System) architecture, entities, components, systems, archetypes, spawners, scenes, simulation lifecycle, dual Python binding system (pybind11 + nanobind), PythonSystem subinterpreters, and Python ECS bindings. Use when the user asks about ECS, entities, components, scenes, simulation, or ECS Python scripting.
user-invocable: false
---

# Orkid ECS Architecture Reference

When answering questions about the ECS system in orkid, consult the files below. All under `ork.ecs/`.

## Key Files

| Component | Header |
|-----------|--------|
| Entity | `inc/ork/ecs/entity.h` |
| Component/ComponentData | `inc/ork/ecs/component.h` |
| System/SystemData | `inc/ork/ecs/system.h` |
| Archetype | `inc/ork/ecs/archetype.h` |
| ReferenceArchetype | `inc/ork/ecs/ReferenceArchetype.h` |
| SceneData | `inc/ork/ecs/scene.h` |
| SceneObject/SceneDagObject | `inc/ork/ecs/sceneobject.h` |
| Simulation | `inc/ork/ecs/simulation.h` |
| Controller | `inc/ork/ecs/controller.h` |
| SceneGraphComponent | `inc/ork/ecs/SceneGraphComponent.h` |
| Physics (Bullet) | `inc/ork/ecs/physics/bullet.h` |
| Python Scripts | `inc/ork/ecs/pysys/PythonComponent.h` |
| Lua Scripts | `inc/ork/ecs/lua/LuaComponent.h` |
| Types/Tokens | `inc/ork/ecs/types.h` |
| DataTable | `inc/ork/ecs/datatable.h` |
| EcsRuntime Helper | `obt.project/scripts/ork/ecs/__init__.py` |

**Applications:**
| Program | Location | Purpose |
|---------|----------|---------|
| `ork.ecsedit.py` | `obt.project/bin/ork.ecsedit.py` | Scene editor (edit/stage/play modes) |
| `ork.ecsplay.py` | `obt.project/bin/ork.ecsplay.py` | Scene player (stage then play) |

**Editor Implementation:**
| File | Purpose |
|------|---------|
| `obt.project/scripts/ork/editor/ecsedit.py` | Full ECS editor: outliner, propsheet, 3D viewport, toolbar |
| `obt.project/scripts/ork/editor/ecs_outliner_model.py` | OutlinerModel for Archetypes/Spawners/Systems |
| `obt.project/scripts/ork/editor/scene_editor_base.py` | Base class for editor modes |
| `obt.project/scripts/ork/editor/scene_io.py` | Scene load/save via FilesystemBrowser |
| `obt.project/scripts/ork/editor/light_editor.py` | Light property editing |
| `obt.project/scripts/ork/editor/ptc_factories.py` | PropertySheet custom editor factories |

**Python Bindings:** `pyext/` directory:
- `pyext_controller.cpp`, `pyext_scene.cpp`, `pyext_entity.cpp`
- `pyext_component.cpp`, `pyext_system.cpp`, `pyext_simulation.cpp`
- `pyext_archetype.cpp`, `pyext_physics.cpp`, `pyext_scenegraph.cpp`

## Architecture Overview

### Core Types
- **Entity** — runtime game object, holds ComponentTable + transform
- **Component/ComponentData** — modular behavior; data class is serializable, component is runtime
- **System/SystemData** — global simulation subsystem; processes components across all entities
- **Archetype** — template defining entity composition (which components + configuration)
- **SpawnData** — blueprint for spawning entities (transform, autospawn, count, interval)
- **SceneData** — complete scene definition (archetypes, spawners, systems, imports)

### Simulation Lifecycle
```
NEW -> INITIALIZED -> COMPOSED -> LINKED -> STAGED -> ACTIVATED -> RUN/PAUSE
```

### Component/System Lifecycle (Template Method Pattern)
```
_onLink()       — initialize resources
_onStage()      — prepare for use
_onActivate()   — become active
_onUpdate()     — per-frame (System only)
_onRender()     — render (System only)
_onDeactivate() — stop updating
_onUnstage()    — release staged resources
_onUnlink()     — teardown
_onUninitialize() — final cleanup
```

### Entity Lifecycle
```
Compose -> Link -> Stage -> Activate -> [running] -> Deactivate -> Unstage -> Unlink -> Decompose
```

### Controller — Top-Level Orchestrator
```python
controller = ecs.Controller()
controller.bindScene(scene_data)
controller.createSimulation()
controller.stageSimulation()
controller.startSimulation()
# Game loop:
controller.update()
controller.render(draw_event)
```

### SceneData Composition
```
SceneData
+-- _sceneObjects (archetypes, spawn data, groups)
+-- _systemDatas (system configurations)
+-- _imports (imported sub-scenes)
+-- _sceneScriptPath (optional script)
```

### Available Components

| Component | Purpose |
|-----------|---------|
| SceneGraphComponent | Rendering via scenegraph |
| BulletObjectComponent | Physics via Bullet engine (rigid body, collision shapes, constraints) |
| PythonComponent | Python script execution |
| LuaComponent | Lua script execution |
| TransformCurveComponent | Transform animation via curves |
| BoidsComponent | Flocking behavior |
| ProbeComponent | Lightprobe rendering |
| SimpleSoundEmitterComponent | Sound playback |
| StochWavSoundEmitterComponent | Stochastic sound |

### Available Systems

| System | Purpose |
|--------|---------|
| SceneGraphSystem | Master rendering (owns scene graph, camera, drawable cache) |
| BulletSystem | Bullet physics simulation (rigid bodies, collision detection, constraints, gravity) |
| PythonSystem | Python script coordination |
| LuaSystem | Lua script coordination |
| GlobalSynthSystem | Global synthesizer/audio |

### Spawning
```python
spawn = ecs.SpawnData()
spawn.archetype = my_archetype
spawn.autospawn = True          # Spawn on scene start
spawn.spawnCount = 10           # Number to spawn
spawn.spawnInterval = 0.5       # Time between spawns
spawn.lifetimeMin = 5.0         # Lifespan
spawn.positionRandomRadius = fvec3(2, 0, 2)  # Scatter
```

Runtime spawning:
```python
simulation.enqueueActivateDynamicEntity(archetype, spawn_data)
simulation.enqueueDespawnEntity(entity)
```

### Event/Request System
- Notify (fire-and-forget): `component._notify(sim, token, data)`
- Request (with response): `component._request(sim, response, token, data)`
- Tokens: `"UpdateFramebufferSize"_ecstok`

### Scene Imports
- `ReferenceArchetype` — proxy to archetype in imported scene
- Namespace-based: `_importNamespace` (e.g., "env", "env:props")
- Cross-scene component sharing

### ork.ecsedit.py — Full Scene Editor

CLI: `ork.ecsedit.py [-s scene.json] [-f] [--ssaa N]`

Launches `EcsEditor` (from `ork.editor.ecsedit`) — a full-featured `ComponentizedApplication` scene editor with:
- **Edit mode**: outliner (Archetypes/Spawners/Systems), PropertySheet, 3D viewport with grid, toolbar
- **Play mode**: live simulation with all ECS systems ticking (physics, scripting, audio, rendering)
- **Scene I/O**: load/save via `FilesystemBrowser` in secondary window
- **3D manipulation**: SceneGraphViewport with EzUiCam camera control
- **Property editing**: PropertySheet wired to reflection model (`ReflectionPropertySheetModel`) — auto-generates UI for any reflected component
- **Overlay system**: curve editors, dropdown menus, custom overlays
- **Extension hooks**: custom toolbar buttons, keyboard shortcuts, GPU init callbacks
- **Key implementation files:**
  - `ork.editor.ecsedit` — main editor application
  - `ecs_outliner_model.py` — OutlinerModel with categories and factories
  - `scene_io.py` — scene load/save
  - `light_editor.py` — light property editing
  - `ptc_factories.py` — custom PropertySheet editor factories

### ork.ecsplay.py — Scene Player

CLI: `ork.ecsplay.py [-s scene.json] [-f]`

Simpler `ComponentizedApplication` for playing back scenes:
- Fullscreen SceneGraphViewport
- `EcsRuntime` helper for scene load, camera, simulation control
- Two states: staged (entities created, not ticking) and playing (simulation running)
- Audio enabled (Singularity synth)
- SPACEBAR toggles play/pause
- **Key patterns to reference:**
  - `EcsRuntime.load_scene()`, `stage_simulation()`, `start_simulation()`
  - `SceneGraphViewport.forkDB()` for independent draw queue
  - Camera event handler wiring

### Dual Python Binding System (pybind11 + nanobind)

The ECS uses **two separate binding systems** because pybind11 cannot support Python subinterpreters.

| | pybind11 (`pyext/`) | nanobind (`pyext_sim/`) |
|---|---|---|
| **Module** | `_ecs` → `orkengine.ecs` | `_ecssim` → `orkengine.ecssim` |
| **Interpreter** | Main (global GIL) | Subinterpreter (own GIL) |
| **Used by** | Controller app, scene setup, external API | PythonSystem scripts running in simulation |
| **Type codec** | `pb11_typecodec_t` | `obind_typecodec_t` |
| **Object type** | `pybind11::object` | `obind::object` |
| **Binding files** | 15+ `pyext_*.cpp` | 5 core `pyext_*.cpp` |

**Why two systems:**
1. pybind11 doesn't support `Py_mod_multiple_interpreters` module slot
2. PythonSystem runs simulation scripts on the update thread, isolated from main app
3. Python 3.12+ per-interpreter GIL (`PyInterpreterConfig_OWN_GIL`) prevents GIL contention
4. nanobind declares `Py_MOD_PER_INTERPRETER_GIL_SUPPORTED` in its module slots

**Subinterpreter management** (`ork.core/src/python/context.cpp`):
```cpp
struct Context2 {
  // Created via Py_NewInterpreterFromConfig() with OWN_GIL
  void bindSubInterpreter();    // Swap to sub's thread state
  void unbindSubInterpreter();  // Swap back to main
};
```

**PythonSystem callback pattern** (`ork.ecs/src/scripting/Python/PythonSystem.cpp`):
```cpp
namespace py = obind;  // Uses nanobind, NOT pybind11!

_pythonContext->bindSubInterpreter();
_pymethodOnComponentActivate(wrapped_sim, wrapped_component);
_pythonContext->unbindSubInterpreter();
```

**Code reuse** — common bindings (VarMap, CrcString, math) use `template<typename ADAPTER>`:
```cpp
// Both paths call the same template, different adapter:
_init_crcstring<pybind11adapter>(module, codec);   // in pyext/
_init_crcstring<nanobindadapter>(module, codec);    // in pyext_sim/
```

**Key files:**
- `pyext/` — pybind11 controller bindings (15+ files)
- `pyext_sim/` — nanobind simulation bindings (5 files)
- `inc/ork/ecs/pysys/PythonComponent.h` — PythonSystem/PythonComponent declarations
- `src/scripting/Python/PythonSystem.cpp` — subinterpreter callback dispatch
- `ork.core/inc/ork/python/pycodec.h` — dual adapter type codec
- `ork.core/src/python/context.cpp` — Context2 subinterpreter management

## How to Answer

1. For component creation: check `component.h` for base class, then specific `*Component.h`
2. For system lifecycle: read `system.h` virtual methods
3. For simulation flow: read `simulation.h` and `controller.h`
4. For controller-side Python API: check `pyext/pyext_*.cpp` (pybind11)
5. For simulation-side Python API: check `pyext_sim/pyext_*.cpp` (nanobind)
6. For PythonSystem scripting: read `PythonComponent.h` and `PythonSystem.cpp`
7. For subinterpreter mechanics: read `ork.core/src/python/context.cpp`
8. For scene composition: read `scene.h` for SceneData
