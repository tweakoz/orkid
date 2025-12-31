## Orkid Dataflow Architecture

The Orkid dataflow system provides a visual/modular programming model for processing data through connected nodes. It's designed for real-time applications where data flows through a directed acyclic graph (DAG) of processing modules.

---

## Core Concepts

### 1. Data vs Instance Separation

A key architectural pattern in the dataflow system is the separation between **Data** (definition/template) and **Inst** (runtime instance):

- **Data objects** define the structure, parameters, and connections - they are serializable and represent the "blueprint"
- **Inst objects** are created at runtime from Data objects and hold mutable state during execution

This separation allows:
- Multiple runtime instances from a single definition
- Clean serialization (only Data is saved)
- Clear ownership and lifetime semantics

### 2. Graphs

Container structures that hold and manage modules and their connections.

**GraphData** (`ork::dataflow::GraphData`)
- Defines the topology of connected modules
- Contains `orklut<std::string, object_ptr_t> _modules` - named modules in the graph
- Tracks pending deserialization connections via `_deser_connections`
- Signals topology changes via `_sigTopologyUpdated`
- Provides methods: `addModule()`, `removeModule()`, `safeConnect()`, `disconnect()`

**GraphInst** (`ork::dataflow::GraphInst`)
- Runtime instance of a graph created via `GraphData::createGraphInst()`
- Maintains ordered module lists for execution: `_ordered_module_datas`, `_ordered_module_insts`
- Lifecycle methods: `link()`, `stage()`, `activate()`, `compute()`
- Holds reference to the topology and scheduler

### 3. Modules

Processing units that receive input, process data, and produce output.

**ModuleData** / **DgModuleData**
- Abstract definition of module behavior and configuration
- Declares input and output plugs
- Creates corresponding ModuleInst via `createInstance(GraphInst*)`

**ModuleInst** / **DgModuleInst**
- Runtime instance of a module
- Implements `compute(GraphInst*, ui::updatedata_ptr_t)` for processing
- Implements `onLink(GraphInst*)` for initialization after connections are resolved

**LambdaModuleData**
- Supports functional-style module implementation using lambdas

### 4. Plugs

Connection points on modules for data to flow through. Plugs are strongly typed using C++ templates.

**PlugData** / **PlugInst**
- Base classes for plug definition and instances
- Track parent module, direction (input/output), rate, and data type

**InPlugData** / **OutPlugData**
- Directional data connectors
- InPlugData can connect to one OutPlugData (single source)
- OutPlugData can fan out to multiple inputs (configurable via `max_fanout`)

**Plug Traits**
Plugs use a traits-based system for type configuration:

```cpp
struct FloatPlugTraits {
  using elemental_data_type = float;     // Data-side type
  using elemental_inst_type = float;     // Instance-side type
  using xformer_t = nullpassthrudata;    // Optional transformer
  using range_type = float_range;        // Value constraints
  using out_traits_t = FloatPlugTraits;  // Output plug traits
  static constexpr size_t max_fanout = 0; // 0 = unlimited
  static std::shared_ptr<float> data_to_inst(std::shared_ptr<float> inp);
};
```

**Transformer Plugs (XF variants)**
- `FloatXfPlugTraits`, `Vec3XfPlugTraits`, `QuatXfPlugTraits`
- Support data modification through the connection (scaling, offsetting, etc.)
- The `xformer_t` type defines the transformation applied

### 5. Registers

Virtual register system to manage data lifetimes, dependencies, and computational resources.

**DgRegister**
- Abstraction of a "machine register" - holds data flowing between modules
- Tracks downstream dependents for lifetime management
- Bound to a specific plug

**DgRegisterBlock**
- Pool of registers for a given datatype
- Typically one register block per datatype in a graph
- Provides `Alloc()` and `Free()` for register management

### 6. Topology and Sorting

**DgSorter**
Topologically sorts modules into execution order ensuring:
1. All modules are computed
2. No module is computed before its inputs are ready
3. Modules compute soon after their parents (cache locality)
4. Minimal register usage

**Topology**
- Contains `_flattened` - ordered list of modules for execution
- Stores `_hash` for change detection

**DgNodeInfo**
- Tracks module metadata for sorting: `_serial`, `_depth`, `_modifier`
- `computeSorkKey()` generates sort priority

### 7. Scheduler

Execution system for processing the graph.

- Handles parallel execution of processing tasks
- Manages dependencies between modules
- Supports heterogeneous processing (CPU/GPU through affinity)
- Assigned to GraphInst via `dgcontext::assignSchedulerToGraphInst()`

---

## Execution Model

1. **Graph Construction**: Modules are added to GraphData, plugs are connected
2. **Topology Analysis**: Graph is sorted to determine execution order
3. **Instance Creation**: GraphInst created, ModuleInsts created for each ModuleData
4. **Linking**: `link()` called - instances resolve their plug connections
5. **Staging**: `stage()` called - instances prepare for execution
6. **Activation**: `activate()` called - instances enter active state
7. **Computation**: `compute()` called each frame:
   - Modules execute in topologically sorted order
   - Data flows through connected plugs from outputs to inputs
   - Registers manage data lifetimes across the execution

---

## Creating Custom Modules

### Step 1: Define the Module Data Class

```cpp
struct MyModuleData : public dflow::DgModuleData {
  DeclareConcreteX(MyModuleData, dflow::DgModuleData);
public:
  MyModuleData();
  static std::shared_ptr<MyModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;

  // Input plugs
  dflow::floatinplugdata_ptr_t _inputValue;

  // Output plugs
  dflow::floatoutplugdata_ptr_t _outputValue;

  // Parameters
  float _multiplier = 1.0f;
};
```

### Step 2: Define the Module Instance Class

```cpp
struct MyModuleInst : public dflow::DgModuleInst {
  MyModuleInst(const MyModuleData* data, dflow::GraphInst* ginst);

  void onLink(dflow::GraphInst* inst) final;
  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) final;

  dflow::floatinpluginst_ptr_t _input;
  dflow::floatoutpluginst_ptr_t _output;
  const MyModuleData* _data;
};
```

### Step 3: Implement the Module

```cpp
MyModuleData::MyModuleData() {
  _inputValue = createInputPlug<dflow::FloatPlugTraits>(this, "Input");
  _outputValue = createOutputPlug<dflow::FloatPlugTraits>(this, "Output");
}

dflow::dgmoduleinst_ptr_t MyModuleData::createInstance(dflow::GraphInst* ginst) const {
  return std::make_shared<MyModuleInst>(this, ginst);
}

void MyModuleInst::onLink(dflow::GraphInst* inst) {
  _input = typedInputInst<dflow::FloatPlugTraits>(_data->_inputValue);
  _output = typedOutputInst<dflow::FloatPlugTraits>(_data->_outputValue);
}

void MyModuleInst::compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) {
  float input_val = _input->value();
  float result = input_val * _data->_multiplier;
  _output->setValue(result);
}
```

---

## Custom Plug Types

For domain-specific data types, define custom plug traits:

```cpp
struct MyDataType { /* ... */ };

struct MyPlugTraits {
  using elemental_data_type = MyDataType;
  using elemental_inst_type = MyDataType;
  using data_impl_type_t = MyDataType;
  using inst_impl_type_t = MyDataType;
  using xformer_t = dflow::nullpassthrudata;
  using range_type = no_range;
  using out_traits_t = MyPlugTraits;
  static constexpr size_t max_fanout = 1;  // Limit connections

  static std::shared_ptr<MyDataType> data_to_inst(std::shared_ptr<MyDataType> inp);
};

using my_inplugdata_t = dflow::inplugdata<MyPlugTraits>;
using my_outplugdata_t = dflow::outplugdata<MyPlugTraits>;
```

---

## Notable Features

- **Transformation Pipeline**: Elaborate transformation system for data modification through XF plug variants
- **Morphable Data**: Support for interpolating between different data states via `morphable` interface
- **Topological Sorting**: Ensures modules execute in correct dependency order
- **Register Management**: Virtual register system optimizes data lifetimes and resource usage
- **Python Bindings**: Full Python API for graph construction and manipulation
- **Serialization**: GraphData and all module configurations are fully serializable

---

## Usage in Orkid

### Particle Systems

See the dedicated section below.

### 2D Procedural Textures

*Pending implementation*

---

## Particle System Implementation

The particle system is the primary user of the dataflow architecture in Orkid. It demonstrates a complete implementation of the dataflow pattern for real-time simulation.

### Architecture Overview

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  ParticlePool   │────▶│    Emitter      │────▶│     Force       │────▶ ...
│   (source)      │     │ (Ring/Nozzle/   │     │ (Gravity/Drag/  │
│                 │     │  Line/Ellipse)  │     │  Vortex/etc.)   │
└─────────────────┘     └─────────────────┘     └─────────────────┘
                                                         │
                                                         ▼
                                               ┌─────────────────┐
                                               │    Renderer     │
                                               │ (Sprite/Streak/ │
                                               │     Light)      │
                                               └─────────────────┘
```

### Particle Buffer Plug Type

Particles flow through the graph via a custom plug type:

```cpp
struct ParticleBufferPlugTraits {
  using elemental_data_type = ParticleBufferData;
  using elemental_inst_type = ParticleBufferInst;
  using xformer_t = dflow::nullpassthrudata;
  using range_type = no_range;
  static constexpr size_t max_fanout = 1;  // Single consumer per output
};
```

The `ParticleBufferInst` contains:
- `pool_ptr_t _pool` - The actual particle pool with live particles

### Module Hierarchy

```
dflow::DgModuleData
  └── particle::ModuleData
        └── particle::ParticleModuleData
              ├── GlobalModuleData      (system globals)
              ├── ParticlePoolData      (particle storage)
              ├── Emitters:
              │     ├── RingEmitterData
              │     ├── NozzleEmitterData
              │     ├── LineEmitterData
              │     └── EllipticalEmitterData
              ├── Forces:
              │     ├── GravityModuleData
              │     ├── TurbulenceModuleData
              │     ├── VortexModuleData
              │     ├── DragModuleData
              │     ├── SphAttractorModuleData
              │     ├── EllipticalAttractorModuleData
              │     └── PointAttractorModuleData
              └── Renderers:
                    ├── SpriteRendererData
                    ├── StreakRendererData
                    └── LightRendererData
```

### Module Types

**ParticlePoolData / ParticlePoolModuleInst**
- Source module - creates and manages the particle pool
- Configurable pool size (default 16384 particles)
- Manages particle lifecycle (aging, death, recycling)
- Output: particle buffer

**Emitter Modules**
- Spawn new particles into the pool
- Various emission patterns: ring, nozzle (directional cone), line, elliptical
- Configure: emission rate, initial velocity, position distribution
- Input: particle buffer, Output: particle buffer (pass-through with spawning)

**Force Modules**
- Modify particle velocities each frame
- Types:
  - **Gravity**: constant directional force
  - **Drag**: velocity-dependent resistance
  - **Turbulence**: noise-based chaotic motion
  - **Vortex**: rotational force around an axis
  - **Attractors**: point/spherical/elliptical attraction
- Input: particle buffer, Output: particle buffer (modified velocities)

**Renderer Modules**
- Terminal modules - consume particles for rendering
- Types:
  - **Sprite**: camera-facing billboards
  - **Streak**: velocity-aligned trails
  - **Light**: dynamic point lights per particle
- Materials: Flat, Gradient, Texture, TexGrid, VolTex
- Input: particle buffer (no output - end of chain)

### Particle Data Flow Example

```cpp
// In compute(), forces iterate the pool and modify particles:
void GravityModuleInst::compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) {
  float dt = updata->_dt;
  fvec3 gravity_vec = _data->_gravity;

  for (auto& particle : *_pool) {
    if (particle.isAlive()) {
      particle.velocity += gravity_vec * dt;
      particle.position += particle.velocity * dt;
    }
  }
}
```

### Materials System

Renderers use pluggable materials:

```cpp
struct MaterialBase : public ork::Object {
  virtual void gpuInit(const RenderContextInstData& RCID) = 0;
  virtual void update(const RenderContextInstData& RCID) {}

  freestyle_mtl_ptr_t _material;
  fxpipeline_ptr_t _pipeline;
  fxtechnique_constptr_t _tek_sprites;
  fxtechnique_constptr_t _tek_streaks;
};
```

Material types:
- **FlatMaterial**: solid color
- **GradientMaterial**: color based on particle age via gradient lookup
- **TextureMaterial**: textured sprites
- **TexGridMaterial**: sprite sheet animation
- **VolTexMaterial**: 3D texture sampling

### Python API Example

```python
import ork.lev2.gfx.particle as ptc

# Create graph
graph = ptc.GraphData()

# Add modules
pool = ptc.ParticlePoolData.createShared()
pool.poolSize = 10000
graph.addModule("pool", pool)

emitter = ptc.RingEmitterData.createShared()
graph.addModule("emitter", emitter)

gravity = ptc.GravityModuleData.createShared()
graph.addModule("gravity", gravity)

renderer = ptc.SpriteRendererData.createShared()
renderer.material = ptc.GradientMaterial.createShared()
graph.addModule("renderer", renderer)

# Connect modules
graph.safeConnect(emitter.inputBuffer, pool.outputBuffer)
graph.safeConnect(gravity.inputBuffer, emitter.outputBuffer)
graph.safeConnect(renderer.inputBuffer, gravity.outputBuffer)
```

### Rendering Integration

Particle systems integrate with the Orkid rendering pipeline:

1. **DrawableData**: `ParticleSystemDrawableData` wraps a particle GraphData
2. **DrawableInst**: Created per-scene-instance, holds the GraphInst
3. **Enqueue**: During render traversal, particle renderers enqueue draw calls
4. **Render**: GPU renders particles using the material's pipeline

The compute/render split allows:
- Simulation on the update thread
- Render buffer snapshot for GPU submission
- Double-buffering for thread safety
