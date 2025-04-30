## Orkid Dataflow Architecture

The Orkid dataflow system provides a dataflow programming model with these key components:

1. **Graphs** - Container structures that hold and manage modules and their connections
   - `GraphData` - Defines the topology of connected modules
   - `GraphInst` - Runtime instance of a graph

2. **Modules** - Processing units that receive input, process data, and produce output
   - `ModuleData` - Abstract definition of module behavior
   - `ModuleInst` - Runtime instance of a module
   - `DgModuleData` & `DgModuleInst` - Specializations for dataflow graphs
   - `LambdaModuleData` - Supports functional-style module implementation

3. **Plugs** - Connection points on modules for data to flow through
   - `PlugData` - Base class for plug definition
   - `InPlugData` & `OutPlugData` - Directional data connectors
   - Various typed implementations (float, vector, quaternion, image, user-defined)
   - Transformer plugs with modifiers (XF variants)

4. **Scheduler** - Execution system for processing the graph
   - Handles parallel execution of processing tasks
   - Manages dependencies between modules
   - Supports heterogeneous processing (CPU/GPU through affinity)

## Execution Model

1. Graph topology is analyzed to determine module execution order
2. Modules are sorted by dependencies to ensure correct execution sequence
3. Processing is distributed across processing units (potentially in parallel)
4. Data flows through connected plugs from outputs to inputs

## Notable Features

- **Transformation Pipeline**: Elaborate transformation system for data modification
- **Morphable Data**: Support for interpolating between different data states
- **Topological Sorting**: Ensures modules execute in the correct order
- **Register Management**: Virtual register system to manage data lifetimes, dependencies and computational resources
- **Python Bindings** 

## Usage in orkid

- **Particle Systems**
- **2D Procedural Textures** *pending
