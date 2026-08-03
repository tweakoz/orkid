---
name: hypersyn
description: How to author procedural content in orkid using HyperSyn (Reality Synthesizer) — a Python-as-DSL system for composing particle systems, 2D/3D procedural textures, mesh/SDF operations, and (eventually) audio/sequencer graphs into compact runtime experiences. Use when the user asks about HyperSyn, ptex2d, ptex3d, hypermesh, dataflow DSL, FXV2 codegen from dataflow, subprocess graph validation, or the hypergraph composition layer.
user-invocable: false
---

# HyperSyn — Reality Synthesizer

HyperSyn is orkid's unified authoring stack for procedural content. It layers a Python expression DSL on top of the existing `ork::dataflow` graph runtime to let users compose particles, procedural textures (2D and 3D), mesh/SDF operations, and — eventually — audio and sequencer graphs from compact, declarative Python. Each authored graph traces into a `dflow::graphdata_ptr_t`, is validated in a subprocess (the editor must never be brought down by a bad graph), then materializes into a typed runtime artifact (PBRMaterial, Texture, Mesh, SDF grid, GraphInst).

The naming: **HyperSyn** because the long-term target is a single composable hypergraph — modeled on Farbrausch's *werkkzeug* and Houdini's multi-context model — that synthesizes an entire realtime experience (textures, meshes, particles, sound, sequencing) from a compact procedural description.

**This skill is the contract — the API surface, the authoring pattern, the materializer outputs, the runtime modes.** Implementation status, milestone roadmap, M0 hardening checklist, and team-review-driven design corrections live in the sibling file `PLAN.md`. When you are using HyperSyn (authoring graphs, asking how a family works, looking up materializer outputs), this skill is the source of truth. When you are building HyperSyn (deciding what to commit next, tracking what's done), consult `PLAN.md`.

## Core concepts

### Trace-then-materialize

A HyperSyn graph is authored as a Python class that subclasses a family base. `__init__` builds an in-memory **expression tree** using DSL operations. `self.surface(...)` / `self.texture(...)` / `self.mesh(...)` / `self.render(...)` records the output bindings. `generatedflow()` walks the tree and emits a typed `dflow::graphdata_ptr_t` (a family-specific subclass of `GraphData`). The graph is then validated and materialized.

```
Subclass(FamilyBase).__init__()      build DSL tree (each call returns a DslNode)
                .surface/...(...)    declare output bindings (channel name → DslNode)
                .generatedflow()     materialize tree into dflow::graphdata_ptr_t

dflow.validate(graph)                spawn subprocess: deserialize + DgSorter + link + stage + activate + compute
                                     return {ok: bool, errors: [(module, plug, msg)], warnings: [...]}

family.materialize(graph, ctx)       per-family final artifact build (also in subprocess for codegen)
                                     → dict[str, Artifact]  (multi-sink, see "Multi-sink materializer outputs")
                                       or None on failure (whole-graph failure)
```

The editor process never executes a freshly-authored graph in-proc. It always calls `validate()` and `materialize()` through the subprocess harness so that compile errors, runtime asserts, or infinite loops are caught and reported.

### Family

A *family* is a vertical slice of HyperSyn — a base class, a plug-type set, a DSL vocabulary, a materializer, and an output artifact type. Families:

| Family | Plug types | Materializer output | Status |
|---|---|---|---|
| `particles` | `ParticleBuffer` | `graphinst_ptr_t` (runtime exec) | planned |
| `ptex2d` | `Image2DBuffer`, scalar/vec | `texture_ptr_t` (FXV2 → RT pool) | planned |
| `ptex3d` | `SurfNode` expression tree, `SurfaceCtx` | baked forward-PBR `.fxv2` (+ `PBRMaterial` via the asset layer, `materialize_ptex3d`) | **shipped** — TOP-LEVEL `ork.hypergraph.ptex3d` (8 files; NOT `dflow.ptex3d`) |
| `hypermesh` | `MeshBuffer`, `SdfGrid` (DENSE; NANOVDB reserved), `XfNodeGraph` (LANDED — the L-system spine), `Skeleton`-adapter (planned) | today: single-sink `self.output()` → GpuMesh (bake `.ogeo` / live `ComputeDrawable`); named multi-sink (`collider`/`sdf`/`skeleton`) = target design | **shipped** — see `ork.dox/hyper/HYPERMESH.md` |
| `sdf` | `SdfGrid` { DENSE shipped, NANOVDB reserved/unbuilt } | dense brick (GPU-writable) + mesh via marching tetrahedra (`SdfToMesh`); future NanoVDB blob | **shipped (E.7 track)** — see note below table |
| `terrain` | `TerrainNode` field expressions, `ScatterSet`, masks-as-fields | multi-sink bake: channel images (EXR/PNG16) + `.terrain.json` manifest (scale contract) + `.ogeo` scatter point sets; rendered by `TerrainChunkDrawableData`, consumed by physics/ECS | **shipped** — see `ork.dox/hyper/HYPERTERRAIN.md` |
| `hyperprim` | `PrimSlot`, `Float`, `Vec3`, `MeshRef` | multi-sink: `mesh` → `xgmmodel_ptr_t`, `collider` → `shapedata_ptr_t`, plus named-slot dict (cake/lamppost/bench parametric props) | forward |
| `hyperarch` | `Footprint`, `Storey`, `Facade`, `Opening`, `Roof` | multi-sink: `mesh` → `xgmmodel_ptr_t` bundle, `collider` → `shapedata_ptr_t`, `nav_mesh` → `navmesh_ptr_t`, plus opening-slot dict | forward |
| `hypercity` | `StreetGraph`, `Parcel`, `BlockMask`, `BuildingDist` | multi-sink: streamed urban tile, instanced-drawable cloud, path-network drawable, `nav_graph` → AI/teleport graph | forward |
| `hyperanim` | `AnimClip`, `BlendNode`, `Pose`, `IKChain`, `EventStream` | `posegraph_ptr_t` (per-frame pose evaluator wrapping `XgmAnimInst` + blend tree + IK) | forward |
| `hyperlight` | `LightDescriptor`, `IBL`, `FogProfile`, `SkyModel` | `light_ptr_t[]` + `EnvironmentProbe` + `FogParameters` | forward |
| `hypershot` | `CameraDescriptor`, `Lens`, `DollyPath`, `FocusCurve` | `camera_ptr_t` (per-shot camera authoring; shot *sequencing* belongs to the future `sequence` family) | forward |
| `behavior` | `State`, `Transition`, `Predicate`, `SceneQuery`, `EventStream` | `behavior_ptr_t` (stateful FSM runtime wrapping `FsmInstance`; drives pose graphs, particle systems, etc.) | forward |
| `singularity` | `SoundNode` signal expressions, params, mod routes | `MaterializedSound` (Singularity `ProgramData`/bank + patch manifest) via `materialize_sound_patch` | **shipped-v1 (hypersound)** — `obt.project/scripts/ork/hypergraph/sound/{dsl,emitter,caps}.py` (`SoundPatch`/`S.*`); tests `ork.lev2/pyext/tests/singularity/test_hypersound_{basic,arith}.py`; scene `ork.data/scenes/scn_spatial_audio_showcase.py` |
| `sequence` | event stream, automation lane | `sequence_ptr_t` (timeline) | future |

> **`sdf` family (shipped, E.7 track).** SDF is a first-class shipped dataflow family with its own 6
> C++ modules under `ork.lev2/src/gfx/sdf/` (`SdfEval`, `MeshToSdf`, `Csg`, `SdfToMesh`, …) and a Python
> algebra at `obt.project/scripts/ork/hypergraph/dflow/sdf/` (sphere/box/capsule, `|`/`&`/`−`,
> `smooth_union`, `.offset`). Plug type: `SdfGrid` (the third `interchange.h` GPU-resource handle).
> Materializer sinks: a **dense voxel brick** (GPU-writable; voxelize / CSG / marching-tetrahedra) and a
> **mesh** via `SdfToMesh`. It is primarily consumed *within* hypermesh workflows (booleans-as-SDF per
> A3 #4) but is a standalone family in its own right. **NANOVDB is DECLARED-but-UNBUILT:** the
> `SdfRepr::NANOVDB` enum slot exists in `interchange.h` reserved for milestone M3, but only the DENSE
> representation is implemented today — consumers must not branch on an unimplemented repr.

> **`XfNodeGraph` / structural spine (foundation LANDED 2026-06-25; advanced layer forward).** The shared
> transform-graph currency that lets flora / cities / creatures be thin specializations — and the one
> rewrite/grammar engine that drives them — are specified in `UNIFIED_SUBSTRATE.md` §11–§16. The
> `XfNodeGraph` currency (G0a), the `LSweep` skinner (G0b), and the first generator — the **`lsystem`
> family** (G1, M1) — are in code (`interchange.h`, `hmdflow_module_lsystem/lsweep.cpp`). Forward: the
> reflected-grammar generalization + GPU rewrite, the other generators (space-colonization / phyllotaxis /
> tensor-field / straight-skeleton siblings), and the creature rig/skin path — all land on `XfNodeGraph`.

Each family lives at:

- C++ runtime: `ork.lev2/{inc,src}/ork/lev2/gfx/<family>/`
- Python DSL base class + family module: `obt.project/scripts/ork/hypergraph/dflow/<family>/__init__.py`
- DSL op library (one .py per op): `obt.project/scripts/ork/hypergraph/dflow/<family>/ops/`
- Materializer (Python wrapper around C++ codegen): `obt.project/scripts/ork/hypergraph/dflow/<family>/materialize.py`

The Python root `ork.hypergraph.dflow.<family>` is a historical handle dating from when every family was a dataflow graph; not every family is strictly dataflow today (see "Family kinds" below). The path stays for consistency. (Pre-namespace-refactor the path was `ork.dflow.<family>`; the import-path rewrite preserves the `dflow` segment to keep the convention intact even for non-dataflow families.)

**Shipped exceptions — TOP-LEVEL packages.** Two shipped families are pure expression/trace DSLs, not dflow graphs, and live at the TOP level of the namespace: `ork.hypergraph.ptex3d` (surface DSL + fxv2 codegen; canonical import `from ork.hypergraph.ptex3d import Ptex3d, P`) and `ork.hypergraph.sound` (hypersound; `from ork.hypergraph.sound import S, SoundPatch, materialize_sound_patch`). There is no `dflow.ptex3d` — never write that import.

### Multi-sink materializer outputs

A graph derives **multiple distinct typed artifacts** from a shared procedural source, not a single bundled output. Visible meshes, physics colliders, navigation meshes, audio occlusion meshes, and LOD chains are **distinct artifacts** with different topology, resolution, and structural invariants — even when derived from the same upstream SDF expression or parametric construction. Pretending they're one thing is wrong.

Each family declares its sink set. The family base class exposes one sink method per typed output (e.g. `self.mesh()`, `self.collider()`, `self.nav_mesh()`, `self.occlusion()`); each sink call records a (name, derivation) pair. `generatedflow()` resolves these into output nodes inside the `dflow::graphdata_ptr_t`.

`materialize(graph, ctx) -> dict[str, Artifact]` returns a dict keyed by sink name. Consumers ask for only the typed outputs they need:

```python
artifacts = materialize(my_walls.generatedflow(), ctx)
renderer.attach(artifacts["mesh"])                # xgmmodel_ptr_t
bullet_world.add_static(artifacts["collider"])    # shapedata_ptr_t
ai_pathing.add_blocker(artifacts.get("nav_mesh")) # navmesh_ptr_t or None
# artifacts dict may omit sinks the graph didn't declare
```

The procedural source is shared (the SDF expression, parametric construction, FXV2 fragment); the artifacts are independent. The materializer cache keys per-sink (visible mesh cached separately from collider, since they take different reduction parameters).

Where collision geometry and visible geometry genuinely share derivation (a wall is the same shape coarsened for physics), the shared upstream subgraph is automatically deduplicated by topology. Where they don't share (an animal's visible mesh is skinned PBR, its collider is per-bone capsules), the sinks pull from completely separate sub-constructions in the same graph:

```python
from ork.hypergraph.dflow.hypermesh import Hypermesh
from ork.hypergraph.dflow import hypermesh as H     # module alias (H.make_drawable, rigging sketches)

class CourtyardWalls(Hypermesh):
    def __init__(self):
        super().__init__()
        # shared procedural source — one SDF CSG expression, framed per consumer resolution
        # (real API: self.sdf(dim=, extent=) binds a brick; brick dim IS the mesh resolution)
        hi = self.sdf(dim=512, extent=12.0)          # fine brick → visible mesh
        lo = self.sdf(dim=96,  extent=12.0)          # coarse brick → collider
        walls_hi = hi.box(size=(5.3, 2.0, 5.3)) - hi.box(size=(5.0, 2.5, 5.0))
        walls_lo = lo.box(size=(5.3, 2.0, 5.3)) - lo.box(size=(5.0, 2.5, 5.0))
        # distinct typed artifacts, each derived from the source at its own resolution
        self.mesh(walls_hi.to_mesh(weld=True))       # visible (marching tetrahedra, welded)
        self.collider(walls_lo.to_mesh(blocky=True), # collider (cuberille voxel blocks, cheap)
                      kind="mesh", static=True)

class Cheetah(Hypermesh):
    def __init__(self, params):
        # visible mesh derived one way (skinned organic) — rigging surface: target design, unbuilt
        skel, mesh, weights = build_quadruped(params)
        self.skinned(mesh, skel, weights)
        # collider derived completely differently (per-bone capsules, no shared source)
        self.collider(H.per_bone_capsules(skel, radii=params.bone_radii),
                      kind="compound", static=False)
```

> **Status note.** The SDF construction above is the real shipped API (`dflow/sdf/__init__.py`,
> consumed via `self.sdf(...)` / `self.sdf_to_mesh(node, weld=, blocky=)` in
> `dflow/hypermesh/__init__.py:509`). The NAMED-SINK surface (`self.mesh` / `self.collider` / …)
> is the target multi-sink design — today's shipped hypermesh terminal is single-sink
> `self.output(mesh)` (see `UNIFIED_SUBSTRATE.md` §11.3), and the rigging surface (`skinned`,
> `per_bone_capsules`) is feasibility-gated and unbuilt.

Common sink names across families (each family declares which it supports):
- `mesh` — `xgmmodel_ptr_t` rigid or skinned visible geometry
- `collider` — `shapedata_ptr_t` physics collider (orkid's bullet binding today; abstract enough that a future physics backend swap is contained to the materializer)
- `nav_mesh` / `nav_graph` / `nav_blocker` — navigation surfaces for AI pathing + VR teleport-target tagging
- `occlusion` — simplified geometry for audio/visibility raycasts
- `lod[n]` — distance-LOD chain (`lod0` is base, `lod1`/`lod2`/... are coarsened)
- `sdf` — `sdfgrid_ptr_t` (openvdb FloatGrid; useful as input to other graphs or as a deformation field)
- `skeleton` — `xgmskeleton_ptr_t`
- `texture` (ptex2d) — `texture_ptr_t`
- `surface` (ptex3d) — `pbrmaterial_ptr_t`
- `heightfield` (terrain) — baked `Heightfield`
- `drawable` (terrain, particles) — `drawable_ptr_t`
- `pose` (hyperanim) — `posegraph_ptr_t`
- `camera` (hypershot) — `camera_ptr_t`

A graph declares only the sinks it has meaning for. Each sink is optional from the consumer's perspective — `artifacts.get(name)` returns `None` if undeclared.

### Family kinds — dataflow vs stateful

HyperSyn is the **procedural-composition DSL stack**, not strictly the dataflow DSL stack. Families fall into two runtime kinds with the same authoring shape but different execution models:

| Kind | Runtime | Materializes to | Examples |
|---|---|---|---|
| **Dataflow** | `dflow` graph (`DgSorter` topo-sort + serial `compute()`); deterministic per-frame function of plug inputs | `graphinst_ptr_t` or typed codegen artifact | particles, ptex2d, ptex3d, hypermesh, terrain, hyperprim, hyperarch, hypercity, hyperanim (pose pipeline), hyperlight, hypershot |
| **Stateful** | FSM step (orkcore-fsm) or timeline tick; persistent state across frames; reactive to scene queries + events | `behavior_ptr_t`, `sequence_ptr_t` | behavior (forward), sequence (future M5) |

Both kinds share:
- Same Python authoring pattern (subclass FamilyBase, build in `__init__`, declare output via sink method, `generatedflow()` materializes)
- Same validator harness (`validate()` / `dry_run()`); kind-specific check rules add to the shared diagnostic format
- Same editor canvas (visualizations differ — dataflow modules vs FSM states — but both expose nodes + edges + properties)
- Same hypergraph composition (cross-family edges connect dataflow outputs to stateful inputs and vice versa)

They differ in:
- Runtime execution shape (graph compute vs FSM step)
- Materializer output type (typed artifact vs stateful interpreter)
- Validator checks (DAG/cycle/fanout for dataflow; reachability/cycle-without-escape for FSM)

A stateful family's `generatedflow()` still returns a `dflow::graphdata_ptr_t` for serialization compatibility — the FSM is encoded as a graph where states are modules and transitions are connections with predicate guards. The *runtime* differs (FSM step vs graph compute), but the *on-disk format* and the *editor representation* are uniform with the dataflow families.

### DSL operations

**(STATUS: M1.B design — not the shipped shape.)** The shipped families register ops differently today: particles uses `chain_op`-based vocab, ptex3d uses `SurfNode` methods in `ptex3d/dsl.py`, and both ptex3d and hypersound fall back to the minimal shipped `ork.hypergraph.registry` for extensions (see "Op registry introspection API" below). In the M1.B design, each DSL op is a single Python file declaring a class with an imperative `.build(graph, **inputs)` method, registered in a per-family central registry so introspection tools (and the editor) can enumerate available ops without filesystem scanning.

```python
# M1.B design sketch — obt.project/scripts/ork/hypergraph/dflow/<family>/ops/mix.py
from ork.hypergraph.dflow.dsl import op, DslNode
from ork.ptex3d.types import Float, Vec3, Vec4

@op(family="ptex3d", name="mix")
class Mix:
    """Linear interpolation: mix(a, b, t) = a*(1-t) + b*t. Vector forms broadcast."""
    inputs  = {"a": (Vec3, Vec4, Float), "b": "match a", "t": Float}
    outputs = {"out": "match a"}

    @staticmethod
    def build(graph, a, b, t):
        # Emit a dflow MixModuleData into `graph`, wire inputs, return its output plug
        # as a DslNode so downstream ops can chain.
        node = graph.create_anon(MixModuleData)
        graph.connect(node.inputs.a, _as_dflow_output(a))
        graph.connect(node.inputs.b, _as_dflow_output(b))
        graph.connect(node.inputs.t, _as_dflow_output(t))
        return DslNode(node.outputs.out)

    # For shader-codegen families, the op also declares its FXV2 fragment template.
    fxv2_fragment = "${out_t} ${out} = mix(${a}, ${b}, ${t});"
```

**Multi-output ops.** An op with more than one output (e.g. Worley noise returning `value`, `gradient`, and `cell_id` simultaneously, or surface ops returning a tangent frame) declares each output in `outputs` and `build()` returns a `dict[str, DslNode]` keyed by output name. Callers receive a `DslNodeBundle` exposing each output as an attribute:

```python
# M1.B design sketch — obt.project/scripts/ork/hypergraph/dflow/<family>/ops/worley.py
@op(family="ptex3d", name="worley")
class Worley:
    """Worley (cellular) noise: returns nearest-cell distance, gradient, and cell id."""
    inputs  = {"pos": Vec3, "scale": Float}
    outputs = {"value": Float, "gradient": Vec3, "cell_id": Float}

    @staticmethod
    def build(graph, pos, scale):
        node = graph.create_anon(WorleyModuleData)
        graph.connect(node.inputs.pos, _as_dflow_output(pos))
        graph.connect(node.inputs.scale, _as_dflow_output(scale))
        return {
            "value":    DslNode(node.outputs.value),
            "gradient": DslNode(node.outputs.gradient),
            "cell_id":  DslNode(node.outputs.cell_id),
        }

    fxv2_fragment = """
        vec4 ${_tmp} = worley(${pos}, ${scale});
        float ${value}    = ${_tmp}.x;
        vec3  ${gradient} = ${_tmp}.yzw;
        float ${cell_id}  = ${value} * 7919.0 + ${pos}.x * 31.0;
    """
```

User-side usage:
```python
n = P.worley(ctx.xyz, scale=14.0)
rosette = P.smoothstep(0.35, 0.55, n.value) - P.smoothstep(0.15, 0.30, n.value)
flow    = P.normalize(n.gradient)
hash_   = P.fract(n.cell_id * 0.123)
```

Single-output ops continue to return a bare `DslNode` from `build()` (the existing `mix` example above); both forms are first-class. `OpInfo.outputs` always reports the full dict (size 1 for the sugar case).

The user-facing call surface uses aliased imports — the shipped form is `from ork.hypergraph.ptex3d import P` (top-level package); dflow families follow `from ork.hypergraph.dflow import <family> as X`. In the M1.B design each callable is `lambda *args, **kw: registry["<family>"]["mix"].build(_current_graph(), *args, **kw)` (or equivalent). Star-imports are explicitly avoided so module origin stays visible at every call site (debuggability and tooling introspection).

### Source provenance

Two source positions are recorded automatically and flow through to diagnostics, materializer comments, and editor "go to definition":

1. **Op declaration site** — at `@op` decoration time, the decorator captures `inspect.getsourcefile(cls)` + `inspect.getsourcelines(cls)[1]` and stores them in `OpInfo.source` (see "Op registry introspection API"). This is the file:line of the op class itself.
2. **DSL call site** — every `DslNode` records `(file, lineno)` of the call that produced it, via `sys._getframe(1)` inside the op-factory thunk. This is the user's call site (e.g. the line in `SpottedSkin.__init__` that wrote `P.mix(black, yellow, lerp)`).

Both feed `Diagnostic.source` (see "Validation") so error markers point at the exact line that produced the issue, not just at the offending module/plug name. The materializer additionally emits `// <user_file>:<line>` comments above each generated GLSL statement (codegen families), so a debugger reading the compiled shader can trace any line back to the authoring source.

### Op registry introspection API

**(STATUS: PLANNED, M1.B — not yet built.)** Do not confuse this design with the REAL, shipped `ork.hypergraph.registry` (`register_op` / `@op(family, name)` / `get_op` / `has_op` / `list_ops` / `list_families`) — a minimal name→callable table that ptex3d (`ptex3d/dsl.py`) and hypersound (`sound/dsl.py`) already consult as their external-extension hook; the `OpInfo`/`PlugSpec` introspection surface below is the aspirational `dflow.dsl.registry` layer that would sit on top of it.

External tools — editor autocompletion, documentation generators, JSON-schema exporters, type-checkers, code-completion bridges — must enumerate the DSL surface without importing every op module. The op registry exposes:

```python
ork.hypergraph.dflow.dsl.registry.list_families() -> list[str]
ork.hypergraph.dflow.dsl.registry.list_ops(family: str) -> list[OpInfo]
ork.hypergraph.dflow.dsl.registry.list_examples(family: str) -> list[ExampleInfo]
ork.hypergraph.dflow.dsl.registry.get_op(family: str, name: str) -> OpInfo | None

@dataclass(frozen=True)
class OpInfo:
    name: str                               # "mix"
    family: str                             # "ptex3d"
    inputs: dict[str, PlugSpec]             # parameter name → spec
    outputs: dict[str, PlugSpec]            # output name → spec
    docstring: str                          # cls.__doc__
    fxv2_fragment: str | None               # codegen-family ops only
    fxv2_prelude: str | None                # uniform/sampler contributions (set at M3+)
    source: tuple[str, int]                 # (file, lineno) of @op decoration
    examples: list[ExampleInfo]             # cross-ref to examples that use this op
    phase: MaterializationPhase             # default INSTALL/STARTUP/.../LIVE

@dataclass(frozen=True)
class PlugSpec:
    accepted_types: tuple[type, ...]        # e.g. (Vec3, Vec4, Float); singleton if scalar
    constraint: str | None                  # "match a" etc. (cross-input type binding)
    default: Any | None                     # default value if input omitted
    range: tuple[float, float] | None       # numeric range hint (editor + validator)
    required: bool                          # M0 _required annotation
```

Each `@op`-decorated class also gains a `to_json_schema()` classmethod that emits a JSON Schema document for the op's input parameters — used by editor tooling, by external schema-driven validators, and by any caller that needs typed parameter constraints.

The `source` field is populated automatically by `@op` via `inspect.getsourcefile(cls)` + `inspect.getsourcelines(cls)[1]`. The `examples` field is back-populated by the examples loader (see "File layout reference") after a family's `examples/` directory has been indexed.

### Trace context

During `__init__`, a thread-local *trace context* holds the currently-building graph. DSL op invocations consult it via `_current_graph()`. The family base class sets it up before calling user `__init__` and tears it down after:

```python
class Ptex3d(HyperSynFamily):
    def __init_subclass__(cls, **kw):
        super().__init_subclass__(**kw)
        # Optionally: run a type-check trace of subclass __init__ at class-define time

    def __init__(self):  # called by user subclass via super().__init__()
        self._graph = Ptex3dGraphData.createShared()
        self._output_bindings = {}

    def surface(self, albedo=None, normal=None, metallic=None, roughness=None, **kw):
        self._output_bindings.update({k: v for k, v in
            dict(albedo=albedo, normal=normal, metallic=metallic, roughness=roughness, **kw).items()
            if v is not None})

    def generatedflow(self) -> "dflow.graphdata_ptr_t":
        # Resolve self._output_bindings into the graph as output sinks
        # Return self._graph (a Ptex3dGraphData)
        return self._graph
```

## Author helpers

Universal vocabulary used across all family examples. These are not family-specific; they live at `ork.hypergraph.*` (above the per-family namespaces) and produce values that drop into any DSL slot expecting a color, transform, or material.

### Colors — `ork.hypergraph.colors`

Four color constructors, all returning a `_LazyColor` subclass that auto-coerces to `vec3` or `vec4` at the consumption slot. Material dispatchers recognize them and convert per-field; other code calls `.to_vec3()` / `.to_vec4()` explicitly. Alpha != 1 fed into a vec3 slot raises `ValueError`.

```python
from ork.hypergraph import hsv, wavelength, colortemp, colors

base_color       = hsv(0,   1.0, 1.0)        # HSV: hue in degrees [0,360], s/v in [0,1]
base_color       = hsv(0,   1.0, 1.0, 0.5)   # vec4 with alpha
subsurface_color = hsv(20,  0.6, 1.0)        # → vec3 (alpha defaults to 1)

laser_red        = wavelength(650)           # visible-spectrum monochromatic, nm
laser_green      = wavelength(532)
sodium_yellow    = wavelength(589)           # streetlamp / sodium-D line

candle           = colortemp(1900)           # black-body Kelvin, [1000, 40000]
tungsten         = colortemp(3200)
daylight_D65     = colortemp(6500)
overcast_sky     = colortemp(10000)

car_paint        = colors.ruby               # named palette (64 entries)
oak_table        = colors.oak1
horizon_sky      = colors.skyblue1
```

**Spectrum reference** (for `wavelength(nm)`): 405 violet laser, 450 blue, 532 green laser, 555 peak photopic green, 589 sodium-D yellow, 650 red laser. Returns black outside ~380-780 nm.

**Temperature reference** (for `colortemp(k)`): 1900 K candle, 2800 K incandescent bulb, 3200 K tungsten/halogen, 5500 K noon daylight, 6500 K D65 monitor white, 10000 K cool overcast / open shade.

**Palette** (for `colors.<name>`): primaries (red/green/blue/yellow/cyan/magenta/orange/purple/pink/lime/teal/indigo), neutrals (white/black/gray/lightgray/darkgray/charcoal/cream/tan), skin tones (skin1..skin6, light→dark), wood (oak1/oak2/mahogany/birch), stone (granite/marble/slate/jade/ruby/sapphire), plants (leaf1/leaf2/grass/moss/sage/autumn), sky/water (skyblue1=horizon / skyblue2=zenith / sunset/dawn/fog/ocean/lagoon/lake), metals (gold/silver/copper/brass/iron/rust), fire/warm (ember/flame/terracotta/coral), misc (chocolate/wine/lavender/salmon).

### Transforms — `ork.hypergraph.ecs.scene`

Two equivalent forms. The dict form maps `lev2.Transform` field-for-field; the kwargs form is the same fields as Python keyword arguments. Both are accepted everywhere a transform is expected (`Scene.entity(...)`, `Scene.spawner(...)`).

```python
from ork.hypergraph import Transform, axis_angle
from orkengine.core import vec3
import math

# kwargs form
xf = Transform(translation=vec3(-5.5, 0, 0))

# equivalent dict form — passed directly to entity/spawner
self.entity("statue",
    transform={"translation": vec3(-5.5, 0, 0),
               "orientation": axis_angle(vec3(0, 1, 0), math.pi/4),
               "scale": 1.5},
    components=[...])
```

**`axis_angle(axis, angle)`** is a thin alias for `quat.createFromAxisAngle` — short name reads better inside transform dicts than the long static-method form.

### PbrMaterial — dual-form authoring (`ork.hypergraph.asset_core.material.pbr`)

Two equivalent forms for the same material — flat (LLM-friendly, glTF-aligned long names) and nested (human-friendly, mirrors glTF JSON structure). Mixing forms across different lobes is allowed; mixing within one lobe is rejected with a clear error.

```python
# Flat form — preferred when generating from glTF importers or LLM-authored code
PbrMaterial("jade",
    base_color = colors.marble,
    subsurface_color = colors.jade, subsurface_factor = 1.0,
    transmission_factor = 0.15, transmission_roughness = 0.3, ior = 1.55)

# Nested form — preferred when writing by hand (visual grouping)
PbrMaterial("jade",
    base = {"color": colors.marble, "roughness": 0.85},
    subsurface = {"color": colors.jade, "factor": 1.0,
                  "radius": vec3(0.15, 0.55, 0.05)},
    transmission = {"factor": 0.15, "roughness": 0.3},
    ior = 1.55,
    volume = {"attenuation_color": colors.jade, "attenuation_distance": 1.0})
```

**TypedDicts per lobe** (`BaseLobe`, `TransmissionLobe`, `VolumeLobe`, `DiffuseTransmissionLobe`, `SpecularLobe`, `ClearcoatLobe`, `SheenLobe`, `IridescenceLobe`, `SubsurfaceLobe`) declare the closed inner-key vocabulary so IDEs and type checkers can complete and validate keys. The `_NESTED_LOBES` mapping table drives the flat→nested translation with explicit remap rules for fields that don't take a lobe-name prefix (volume's `attenuation_*` fields, base's `color` → `base_color`).

`hsv`/`wavelength`/`colortemp` values auto-coerce per-slot through `PbrMaterial._coerce_hsv` (`isinstance(value, _LazyColor)` check). The same dispatcher pattern applies to any future material-like wrapper.

## Authoring examples

### Particles (test bed for the DSL pattern; runtime exists)

Reference imperative form: `ork.lev2/pyext/tests/renderer/particles/ptc_elliptical3.py` and `ptc_emitter_line.py`.

```python
from orkengine.lev2 import ParticleSystem
from ork.hypergraph.dflow import particles as P

class EllipticalSystem(ParticleSystem):
    def __init__(self, app, layer):
        # build the graph; assign DslNodes to self so onUpdate can mutate inputs live
        self.pool = P.pool_data(size=50000, name="POOL")
        self.emit = P.elliptical_emitter(self.pool, name="EMITN",
                                         EmissionVelocity=1.0, DispersionAngle=180,
                                         LifeSpan=2.0, EmissionRate=10000)
        self.turb = P.turbulence(self.emit, name="TURB", Amount=vec3(1,1,1))
        self.vort = P.vortex(self.turb, name="VORT",
                             VortexStrength=-5, OutwardStrength=-1, Falloff=0.001)
        self.ell  = P.elliptical_attractor(self.vort, name="SPHR",
                                           Inertia=0.01, P1=vec3(0,1,0), P2=vec3(0,-1,0.1),
                                           Dampening=0.999)
        self.grav = P.gravity(self.ell, name="GRAV",
                              G=0.01, Mass=1, OthMass=1, MinDistance=1)

        # material is interleaved with module setup, set as a non-plug member on the renderer
        mat = P.GradientMaterial.createShared()
        mat.blending = tokens.ADDITIVE
        mat.depthtest = tokens.OFF
        mat.gradient.setColorStops({0.0: vec4(1,1,1,1), 0.5: vec4(.2,.4,1,1), 1.0: vec4(0,0,0,1)})

        self.streaks = P.streak_renderer(self.grav, name="STRK", material=mat,
                                         Length=0.15, Width=0.015)
        self.render(self.streaks)

    def onUpdate(self, updinfo):
        # per-frame live control — DslNode handles retain inputs.X = value semantics
        T = updinfo.absolutetime * 0.25
        self.emit.inputs.MinV = 0.6 + math.sin(T*4) * 0.2
        self.turb.inputs.Amount = vec3(math.sin(T) * 20)
        self.ell.inputs.P1 = vec3(0, math.sin(T), 0)
```

`EllipticalSystem(app, layer).generatedflow()` produces a `dflow.graphdata_ptr_t` equivalent to the imperative chain:
```python
g = dflow.GraphData.createShared()
pool = g.create("POOL", particles.Pool)
emit = g.create("EMITN", particles.EllipticalEmitter)
# ... etc
g.connect(emit.inputs.pool, pool.outputs.pool)
g.connect(turb.inputs.pool, emit.outputs.pool)
# ... etc
emit.inputs.LifeSpan = 2.0
# ... etc
```

This equivalence is the round-trip test for the DSL machinery.

**Particle DSL requirements** (driven by the reference examples):
- DslNode handles must expose `inputs.X = value` and remain valid for per-frame mutation in `onUpdate`
- Module names default to the DSL-call's variable assignment with sequence suffix (`self.emit` → `"emit_0"`) but accept explicit `name=` override (matching the existing `"POOL"`, `"EMITN"` convention)
- Renderer ops accept `material=` kwarg AND allow post-construction `node.material = mat` (material is a non-plug member)
- `self.render(node)` records the output renderer; the particles materializer wraps `self.graphdata` in a `ParticlesDrawableData` and returns `{"graphinst": graphinst_ptr_t, "drawable": drawable_ptr_t}` — the caller attaches `drawable` to a scenegraph layer and advances `graphinst.compute(updata)` per frame
- The chain is enforced linear by `ParticleBufferPlugTraits::max_fanout = 1` (M0 will catch violations at `safeConnect` time)

### ptex3d — PBR surface shader (shipped)

Real, working form (the pattern every material under `obt.project/scripts/ork/hypergraph/assets/materials/` follows — e.g. `wood.py`, `timber.py`):

```python
from ork.hypergraph.ptex3d import Ptex3d, P, rgb   # TOP-LEVEL package — NOT dflow.ptex3d

class SpottedSkin(Ptex3d):
    def __init__(self, ctx, *, scale=8.0):
        p     = ctx.P_object * ctx.param("scale", scale)   # runtime-bindable param
        spots = P.fbm(p, 3)                                # 3-octave value-noise fbm
        lerp  = P.smoothstep(0.4, 0.6, spots)
        a = P.mix(rgb(0.05, 0.04, 0.02), rgb(0.85, 0.66, 0.10), lerp)
        r = P.mix(0.5, 0.7, lerp)
        self.surface(albedo=a, roughness=r, metallic=0.0)
```

Materializing this:
```python
from ork.hypergraph.ptex3d import materialize_ptex3d
fxv2_path = materialize_ptex3d(SpottedSkin, scale=6.0)   # bakes the .fxv2 into <staging>/dslshadercache/
# scene/asset-layer consumption (binds params + builds the PBRMaterial):
#   mat = self.asset.Ptex3d("skin", dsl_class=SpottedSkin)
```

(`materialize_ptex3d_full` additionally returns the bindable-param specs + declared PBR lobes; reusable sub-expressions live as plain Python functions in `ork.hypergraph.ptex3d.functions`.)

### ptex2d — procedural texture

```python
from orkengine.lev2 import Ptex2d
from ork.hypergraph.dflow import ptex2d as P

class StripedPanel(Ptex2d):
    def __init__(self, ptx2dctx, dim=(512, 512)):
        self.dim = dim
        stripes = P.squarewave(ptx2dctx.uv.x * 8.0)
        c = P.mix(P.red, P.blue, stripes)
        self.texture(rgba=vec4(c, 1.0))
```

`materialize(graph, ctx)` allocates an RTGroup from the ptex2d pool, runs the FXV2 fragment shader into it, returns `{"texture": texture_ptr_t}`.

### terrain — heightmap + surfacing + placement (shipped)

Full current-state documentation: **`ork.dox/hyper/HYPERTERRAIN.md`**. Reference recipes: `obt.project/scripts/ork/hypergraph/assets/terrain/` (`hf1.py`, `swestvale.py`, `hamletvale.py`, `scatterdemo.py`, `erox.py`, …). The renderer is `TerrainChunkDrawableData` (`ork.lev2/src/gfx/terrain/terrain_chunk_drawable.cpp`) — NOT `GeoClipMapDrawable`, which exists only in the standalone `pyext/tests/renderer/geoclip/` tests and is unrelated to the terrain family.

Terrain is **baked, not simulated at runtime**: the DSL traces a compute-dataflow DAG of field ops (one compute dispatch per node over a `dim × dim` R32F field); the bake products are ordinary images + a JSON manifest + `.ogeo` point sets, and everything downstream consumes those artifacts, never the graph. Heights are TRUE METERS (natural units — `HeightField.EXTENT_M` is the only scale attr; there is no vertical scale constant). A real, working recipe (verbatim from `assets/terrain/hf1.py`):

```python
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

class HF1(HeightField):
    def __init__(self, octaves=6, steps=6):
        super().__init__()
        AMPLITUDE_M = 4000.0   # authored vertical relief in meters (natural units)
        h = (T.Fbm(frequency=3.0, octaves=octaves) * 0.5 + 0.5) * AMPLITUDE_M
        self.capture(T.Terrace(h, step_m=AMPLITUDE_M / steps, sharpness=4.0), "height")
```

**Bake output** (multi-sink, per `HYPERTERRAIN.md`):
- channel images (EXR/PNG16) per `capture(node, channel)` — height, masks, feature fields (slope/curvature/flow)
- `.terrain.json` manifest — the authoritative scale contract (`extent_m`, `dim`, per-channel stats; `dflow/terrain/manifest.py`)
- `.ogeo` point sets for each scatter/placement sink
- rendered by `TerrainChunkDrawableData`; physics + CPU consumers read the baked heightfield through the manifest

**Composition with other families is inherent.** Ground surfacing reuses ptex3d materials (see `assets/terrain/xxx3.py`, `swestvale.py` — full `Ptex3d` classes with terrain `vertex_source`), scattered instances reuse hypermesh/lsystem plants (`xxx3_trees.py`, `scatterdemo.py`), and roads ride the `dflow/roads/` layer. Terrain is the first family that **proved the hypergraph design is load-bearing, not optional** — per-family `GraphData` subclassing fights inherent cross-family composition here. See "Hypergraphs (future)" section.

### hypermesh — SDF + mesh ops (shipped)

Real, working form (the pattern of `obt.project/scripts/ork/hypergraph/assets/hypermesh/sdf_boolean.py`, `sdf_conform.py`, `sdf_clean.py`; full doc `ork.dox/hyper/HYPERMESH.md`):

```python
from ork.hypergraph.dflow.hypermesh import Hypermesh

class HollowSphere(Hypermesh):
    def __init__(self):
        super().__init__()
        s     = self.sdf(dim=256, extent=2.5)      # dense-brick framing; brick dim IS mesh resolution
        shell = s.sphere(1.0) - s.sphere(0.8)      # CSG operators: | union, & intersect, - subtract
        self.output(shell.to_mesh(weld=True))      # marching tetrahedra → indexed GpuMesh
```

The free expression algebra composes the same shapes without a bound brick (`dflow/sdf/__init__.py`):

```python
from ork.hypergraph.dflow import sdf
expr = sdf.box((1.0, 0.5, 2.0)) - sdf.sphere(0.9, center=(0.5, 0.0, 0.0))
expr = sdf.smooth_union(expr, sdf.capsule(a=(0,-0.5,0), b=(0,0.5,0), radius=0.4), k=0.25)
node = self.sdf_eval(expr, dim=128, extent=4.0)   # bake the expression as a brick
mesh = self.sdf_to_mesh(node, weld=True)          # or blocky=True for cuberille voxel blocks
```

The whole chain (`sdf_eval` → `csg` → `sdf_to_mesh`, `dflow/hypermesh/__init__.py:509`) is GPU-resident and animatable per frame — `sdf_boolean.py` pokes `sphere.inputs.offset` each frame and the bite re-carves live. `sdf_to_mesh_clean` is the one-shot CPU terminal (openvdb curvature-adaptive volumeToMesh + xatlas UV unwrap) for clean low-poly output. Today's terminal is single-sink `self.output(mesh)`; the named multi-sink surface (`self.collider(...)`, `self.sdf(...)`, `self.skeleton(...)`) is target design — see "Multi-sink materializer outputs" in Core concepts and `UNIFIED_SUBSTRATE.md` §11.3.

### Procedural rigging + skinning (hypermesh extension)

hypermesh authors **procedural** skeletons and skinning weights — not artist-imported assets (asset import is out of scope; see "Out of scope" at the end). Use cases include parametric characters (quadruped/biped generators), procedural plant rigs (per-frond bones), and structural rigs (cable / chain / rope).

Plug types (RECONCILED with the structural-spine decision in `UNIFIED_SUBSTRATE.md` §11): there is no
standalone `Skeleton`/`WeightMap` plug type. A skeleton is a **`XfNodeGraph` tagged `SKELETON`** (the
family-neutral transform-graph currency) plus a thin **`XfNodeGraph → XgmSkeleton` adapter** at the bake
seam; `WeightMap` = the already-reserved `BONEIDX`/`BONEWT` GpuMesh channels; `BoneRef` = a Python-side
`int` index into the `XfNodeGraph`. Ops sketched:

> **STATUS (feasibility-gated, UNBUILT).** This entire rigging+skinning surface is gated on three
> foundations that do NOT exist today and must land first, in order: (1) the `XfNodeGraph` interchange
> plug + a `skeleton` sink; (2) writable `XgmSkeleton` Python bindings (the binding is read-only today)
> + the `XfNodeGraph → XgmSkeleton` flattener (a C++ function mirroring `meshutil_import_assimp_skeleton.cpp`
> — the genuinely buildable M-sized piece); (3) a GPU-skinning render path consuming `BONEIDX`/`BONEWT`
> (a new FWD_SSBO_CUSTOM skinned variant + bone-matrix SSBO — none exists). `auto_skin`'s weight compute
> is itself M-sized but produces invisible data until (3) lands. `XgmJointProperties` has no
> limits/orient fields, so joint limits are a separate rig-props unit, not part of this. See
> `UNIFIED_SUBSTRATE.md` §16 for the full feasibility-adjusted order. The sketch below is the target
> authoring shape, not a committed near-term API.

```python
from ork.hypergraph.dflow import hypermesh as H

class QuadrupedRig(Hypermesh):
    def __init__(self, body_len=1.2, leg_len=0.6, neck_len=0.4):
        # build skeleton procedurally — each call returns BoneRef
        root  = H.bone(name="root",   pos=(0,0.6,0))
        spine = H.bone_chain(name="spine", parent=root, count=4, length=body_len)
        head  = H.bone_chain(name="neck",  parent=spine[-1], count=2, length=neck_len)
        legs  = [H.bone_chain(name=f"leg_{n}", parent=spine[i], count=3, length=leg_len)
                 for n, i in [("FL",0),("FR",0),("BL",-1),("BR",-1)]]
        sk = H.skeleton([root, *spine, *head] + [b for chain in legs for b in chain])

        # build mesh via SDF-of-primitives (placeholder; real quadruped is harder) —
        # this part is the REAL shipped SDF API (see the hypermesh example above)
        s    = self.sdf(dim=256, extent=3.0)
        body = s.capsule(a=(0, 0, 0), b=(body_len, 0, 0), radius=0.18)
        head = s.sphere(0.15, center=(body_len + 0.1, 0.1, 0))
        mesh = (body | head).to_mesh(weld=True)

        # auto-skin: heat-diffusion weights from each vertex to nearest bones
        weights = H.auto_skin(mesh, sk, method="proximity", max_influences=4)

        self.skinned(mesh, sk, weights)   # → xgmmodel_ptr_t with skinning bound
```

The materializer outputs an `xgmmodel_ptr_t` whose `XgmSkeleton` is the procedural one and whose `XgmMesh` carries proper vertex weights (`SVtxV12N12B12T8I4W4` or similar). The resulting model plugs directly into the renderer's existing skinned-PBR path and is consumed by `hyperanim` as the binding target.

Procedural-skin ops on the roadmap (in `obt.project/scripts/ork/hypergraph/dflow/hypermesh/ops/`): `bone`, `bone_chain`, `bone_mirror`, `skeleton`, `auto_skin` (proximity / heat-diffusion / capsule-influence variants), `paint_weight_region` (SDF-mask-driven manual override), `skinned`, `rigid` (the no-skeleton path; mesh-only output).

## Validation

`orkengine.dflow.validate(graph) -> ValidationResult` is the always-on safety net for editor-driven authoring. **(STATUS: PLANNED, M2 — not yet built.)** It sends the graph (serialized via the existing `ConnectionsProperty` JSON path) over a **zmq REQ/REP socket** to a long-lived worker process from a pre-warmed pool.

> **Disambiguation (important).** The planned `dflow.validate` harness is a *family-neutral dataflow
> graph validator* and is genuinely unbuilt (nothing exists under `obt.project/scripts/ork/hypergraph/`
> by this name). It is NOT the existing `_ork.hypermesh.validate.py` mesh-vet ("meshvet") tool under
> `pyext/tests/` — that is a per-asset trimesh/artist-vet render-time geometry check for a specific mesh,
> unrelated to this graph validator. When `dflow.validate` lands it must live under
> `obt.project/scripts/ork/hypergraph/dflow/validate/`. Do not read the same-named meshvet tool as
> evidence that this harness already exists. Workers import orkid once at startup (the editor shows a one-time "Initializing validator..." progress bar to hide the 0.3–3s cold-import cost) and stay resident for the editor session. Each `validate(graph)` call is sub-ms RTT against a warm worker. The same transport scales from local-only (`ipc://` or `inproc://`) to cross-machine for distributed sims when that need arrives.

On the worker, validate:

1. `DgSorter::generateTopology` (catches cycles after the M0 hardening lands)
2. `GraphInst::link` / `stage` / `activate`
3. One pass of `compute()` with a default `updatedata`

It captures all exceptions, `OrkAssert` failures, and the cycle-detection error from M0. The result is a structured `ValidationResult`:

```python
@dataclass(frozen=True)
class ValidationResult:
    ok: bool
    errors: list[Diagnostic]
    warnings: list[Diagnostic]
    timing: dict[str, float]              # phase timings

@dataclass(frozen=True)
class Diagnostic:
    module: str                           # offending module name in the graph
    plug: str                             # offending plug name ("" if not plug-specific)
    severity: str                         # "error" | "warning" | "info"
    code: str                             # stable: "PLUG_TYPE_MISMATCH", "REQUIRED_INPUT_UNBOUND",
                                          # "CYCLE_DETECTED", "FANOUT_EXCEEDED", "COMPILE_FAILED", ...
    message: str                          # human-readable description
    hint: str | None                      # optional fix suggestion, e.g.
                                          # "expected Vec3, got Float — wrap with Vec3(x)"
    source: tuple[str, int] | None        # (file, lineno) from DslNode/op provenance
```

Stable `code` fields let downstream tools switch on diagnostic kind (editor renders typed UI markers, CLI lint emits machine-parseable output, schema-driven validators surface error categories). The free-form `hint` field carries fix suggestions. The `source` field — populated from `OpInfo.source` and `DslNode` call-site provenance (see "Op registry introspection API" and "DSL operations") — points the caller at the exact line that produced the issue.

The editor never crashes; on failure, it shows error markers on the offending nodes/plugs (color/icon driven by `severity` + `code`) and lets the user keep editing.

### dry_run — in-process variant

`orkengine.dflow.dry_run(graph) -> ValidationResult` runs validation strictly in-process (no subprocess, no zmq). It executes the same DgSorter + link/stage/activate + one-compute sequence as `validate()` and returns the same `ValidationResult` shape. Use cases:

- **CI lint** — running every example in `ork.lev2/pyext/tests/hypersyn/<family>/` through validation as a regression gate; the per-call subprocess spawn cost would dominate the loop, but `dry_run()` in a single process handles thousands of graphs in seconds.
- **Batch authoring** — programmatic graph generation (parameter sweeps, design exploration) where the caller already owns the python process and has no editor to protect.
- **Headless callers** — command-line tools, asset import pipelines, build steps that need correctness checking without the worker-pool infrastructure.

**Tradeoff vs `validate()`**: `dry_run()` is faster (no IPC, no serialize/deserialize roundtrip) but has zero crash isolation. A graph that triggers a native `OrkAssert` or infinite-loops in `compute()` will take down the calling process. Use `dry_run()` only when the caller is willing to fail-fast on bad graphs; use `validate()` whenever the calling process must survive bad input (editor, live-mutable runtime, anything user-driven).

Both functions return the identical `ValidationResult` shape, so callers can swap between them based on context without changing downstream consumer code.

## Materialization

Materializers are per-family. Each runs in a subprocess (same harness as `validate`) so codegen / shader compile errors never bring down the editor. Every materializer returns a `dict[str, Artifact]` keyed by sink name (see "Multi-sink materializer outputs" in Core concepts), or `None` on whole-graph failure:

| Family | Materialize step | Typical sinks |
|---|---|---|
| `particles` | `createGraphInst(graph) → bindTopology` | `{"graphinst": graphinst_ptr_t, "drawable": drawable_ptr_t}` |
| `ptex2d` | Walk graph → stitch FXV2 fragments → `material.gpuInitFromShaderText(ctx, "tek", text)` → render into RT pool | `{"texture": texture_ptr_t}` |
| `ptex3d` | Walk graph → emit per-channel GLSL fragments → assemble full FXV2 → compile → instantiate PBRMaterial with rigid/instanced/skinned shader permutations | `{"surface": pbrmaterial_ptr_t}` |
| `hypermesh` | Walk graph → run openvdb / mesh ops per declared sink (cached per-sink) | `{"mesh", "collider", "sdf", "skeleton", ...}` (whichever the graph declared) |
| `terrain` | Bake heightfield, materialize ground PBR shader, run scatter placement, build paths | `{"terrain", "heightfield", "instances", "paths", "collider"}` |

The FXV2 codegen for ptex2d/ptex3d works by:
1. Each DSL op declares an `fxv2_fragment` template (e.g. `vec3 ${out} = mix(${a}, ${b}, ${t});`).
2. The materializer walks the graph in topological order, assigns SSA names to each output, substitutes the template variables, and concatenates fragments into the fragment-shader body.
3. The result is a complete FXV2 string passed to `material.gpuInitFromShaderText` — the existing API (see usage in `ork.lev2/pyext/tests/ui/widget_pack.py:391`).

## ECS instantiation (future)

Every HyperSyn graph should eventually be instantiable as an ECS entity component. A `HyperSynGraphComponent` (or per-family variants like `Ptex3dSurfaceComponent`, `HypermeshGeometryComponent`, `ParticleSystemComponent`) holds a `dflow::graphdata_ptr_t` (shared, authored once) and constructs a `graphinst_ptr_t` per entity at spawn time. The materialized artifact (PBRMaterial / Texture / Mesh / Drawable) is the component's runtime output, consumed by sibling components (Renderable, Collider, etc.).

Design constraints this places on the family work being done now:
- Each family's `GraphData` subclass must be a fully-reflected `ork::Object` (already required for ConnectionsProperty serialization)
- Materialization must be re-entrant per entity (a `Ptex3dSurfaceComponent` on 100 entities should share the PBRMaterial unless per-entity parameter overrides apply)
- Per-entity plug-value overrides flow through component properties → graph instance's input plug values at construct time
- See [[orkecs-core]] for the component/archetype/spawner pattern this will plug into

## Runtime (Farbrausch-style realtime)

Authoring is editor + zmq worker pool + validator-loop. **Shipped runtime is a smaller subset.** A shipped HyperSyn experience is a self-contained file (JSON or Python) that loads compact procedural graph files from disk, materializes them in-process during a scripted preload phase, and runs at 120+fps. The editor and the zmq worker pool are authoring-time-only.

**The validator subprocess persists into runtime for any experience that supports live content modification while it plays** — adding a new graph, editing a graph's plug values, or hot-swapping a materialized artifact must never crash the running experience. In those scenarios, runtime keeps a long-lived validator process and routes new/modified graphs through validate→materialize the same way the editor does. For purely static experiences (graph set baked at install, no runtime modification), the validator subprocess can be omitted entirely and materialize runs straight in-process at preload.

**The materializer code is identical across all modes.** What differs is the invocation:
- **Authoring**: editor → zmq → worker process → `validate()` + `materialize()` (subprocess isolation; failure shows error markers, editor survives)
- **Runtime, static**: scene loader → `materialize()` in-process during the timeline's preload window (failure is a content bug; experience fails fast at startup)
- **Runtime, live-mutable**: same `validate()` + `materialize()` pipeline as authoring but invoked by the running experience itself against a persistent validator process. Successful materialization swaps the artifact atomically into the live scene; failure surfaces as an error event to the experience without disrupting anything currently playing

**Materialization phases** — every `DgModuleData` declares which phase its materializer should run in. M0 adds this enum field:

| Phase | When | Budget | Example |
|---|---|---|---|
| `INSTALL` | Content-pack build (offline) | unbounded | Texture compression, mesh LOD generation |
| `STARTUP` | App launch, before main loop | seconds | PBR base materials, shader variant precompile |
| `SCENE_LOAD` | Scene boundary, loading screen | hundreds of ms | Terrain heightfield bake, scatter placement bake |
| `PRELOAD` | Timeline N seconds ahead of need | tens of ms (background thread) | Material variants for the next scene's reveal |
| `LIVE` | Every frame | per-frame budget (e.g. <1ms) | Particle compute, audio-reactive plug values |

Default is `LIVE`. The hypergraph coordinator (future) walks the timeline and schedules each module's materialization at the right point. Heavy bakes (terrain, hypermesh CSG, ptex2d/3d codegen) go to `SCENE_LOAD` or `PRELOAD`; only the cheap stuff stays `LIVE`.

**Compact representation** — Farbrausch / werkkzeug spirit: the on-disk demo is the serialized graph (kilobytes), not the materialized artifacts (megabytes-gigabytes). Materialized artifacts are produced fresh at the right phase. This is the design's actual payoff — a 64KB demo that expands to a multi-GB experience at runtime is realistic because every artifact is a deterministic function of a small graph.

**Determinism** — given identical graph + identical seed plugs, materializers must produce bit-identical output across machines. Concretely: no `time()` in materializer code paths, no `dict` iteration order leakage into hashing, seed every RNG explicitly from a plug.

**Runtime budget enforcement** is the demo's responsibility, not the engine's — but the engine surfaces per-materialize timing via the `ValidationResult.timing` field (same field used at authoring time) so the demo's preload scheduler can verify the timeline holds.

## Hypergraphs (future)

A hypergraph is a project-level container that owns multiple per-family graphs plus a timeline. Cross-family connections are typed bridges: a `hypermesh::HollowSphere` graph's `mesh_ptr_t` output feeds a `ptex3d::SurfaceCtx.mesh` input; a `singularity` synth's RMS output drives a `ptex3d` plug value via the timeline.

Design choices being preserved now (so the hypergraph layer slots in cleanly later, but without committing to its file format):

- **Stable plug IDs**: every plug is addressable as `(graph_id, module_name, plug_name)`. Today's `ConnectionsProperty` serializer (`graph_data.cpp:113-137`) uses `(inp_module, inp_plug, out_module, out_plug)` — the hypergraph form prepends `graph_id`.
- **Cross-family plug type registry**: plug-trait types (`Mesh`, `SDFGrid`, `Texture`, `AudioSignal`) are defined once and addressable across families.
- **Materialization phase annotation**: each module declares its phase (`INSTALL` / `STARTUP` / `SCENE_LOAD` / `PRELOAD` / `LIVE`) — see the "Runtime" section. The hypergraph coordinator walks the timeline and schedules each module's materialization at the right point. Heavy bakes go to `SCENE_LOAD` or `PRELOAD`; only the cheap stuff stays `LIVE`.
- **Timeline as a graph**: the sequencer is itself a family — its outputs are automation lanes that feed any other family's inputs.

Goal: compact procedural representation of a complete realtime experience (Farbrausch werkkzeug spirit) with Houdini-like multi-context composition.

## Forward families (rough specs)

These families are designed but not implemented. The spec sketches plug types + materializer output + 1-2 representative ops per family so the API surface is stable enough that ECS components, hypergraph plug-type registry, and per-family directory layouts can be planned today. Full authoring examples + op libraries land when each family is built.

### hyperprim — parametric primitives

Catalog of named parametric props with slot-based composition. Sits between `hypermesh`'s raw SDF/CSG primitives and full architectural grammar — each `hyperprim` op generates a complete prop (cake-on-plate, lamppost, bench, simple chair) parameterized by a handful of inputs and exposing named output slots downstream code can attach to (the cake's top surface, the lamppost's bulb position, the chair's seat normal).

Plug types: `PrimSlot` (named attachment point with position + orientation + tag), `Float`, `Vec3`, `MeshRef`. Multi-sink materializer: `mesh` → `xgmmodel_ptr_t` (visible), `collider` → `shapedata_ptr_t` (often a convex_hull or compound), plus the named-slot dict.

Representative ops: `cake_tiered(tier_count, base_radius, frosting_color)`, `lamppost(height, lamp_style, base_style)`, `bench(length, slat_count)`. Each is internally a small `hypermesh` graph; `hyperprim` is the *catalog layer* that names common shapes and standardizes their slot vocabulary.

### hyperarch — grammar-based architecture

CGA-Shape lineage (CityEngine, Houdini's L-systems): given a footprint polygon + a grammar of split/repeat/extrude/replace rules, generate building geometry with semantic slots (windows, doors, parapets, columns). Distinct from `terrain` (which encloses outdoor space) and from `hypermesh` (which CSGs SDFs).

Plug types: `Footprint` (2D polygon with marked edges), `Storey`, `Facade`, `Opening`, `Roof`, `Material`. Multi-sink materializer: `mesh` → `xgmmodel_ptr_t`, `lod[n]` → coarser LOD variants, `collider` → `shapedata_ptr_t`, `nav_blocker` → walkable-surface blockers, plus per-opening slot dict (window/door positions for door swing animation, light placement).

Representative ops: `extrude(footprint, storeys=[storey_def_a, storey_def_b])`, `split_facade(facade, [bay_def, opening_def, bay_def])`, `repeat_opening(facade, opening, spacing)`, `roof_hipped(footprint, pitch)`. Composes with `ptex3d` (facade materials) and `hyperprim` (door/window/balcony props slotted into openings).

### hypercity — urban layout

Urban-scale: street graph → parcel subdivision → per-parcel `hyperarch` invocations. Composes `hyperarch` the way `terrain` composes `hypermesh`. Authors the "surrounding city" context.

Plug types: `StreetGraph` (typed road segments with widths + sidewalk widths + lighting density), `Parcel` (closed polygon owned by one footprint), `BlockMask` (rules for which parcels get which `hyperarch` archetype + which `StyleContext`), `BuildingDist` (statistical distribution over archetypes). Multi-sink materializer (per spatial tile): `tile` → streamed `TerrainChunkDrawableData`-style tile, `instances` → instanced-drawable cloud (every building is an instance, materials shared by archetype), `paths` → path-network drawable (street ribbons + sidewalk strips), `nav_graph` → AI pathing + VR teleport graph, `collider` → coarse city-block collider for chunked physics.

Representative ops: `grid_street(rows, cols, spacing)`, `radial_street(center, ring_count, ring_spacing)`, `subdivide_block(block, target_parcel_size)`, `populate_parcels(parcels, archetypes, distribution)`. Tile-partition pattern (per PDG): each tile materializes independently and caches separately for streaming.

### hyperanim — procedural pose pipeline

**Scope note**: hyperanim is the **dataflow-shaped** pose-generation pipeline only — procedural clip synthesis + weighted blending + IK overlay + event emission. It is *not* a behavior system. Behavior FSMs ("approach the cake, then bite, then chew, watch for the dog") are a separate concern that lives outside HyperSyn and composes with a hyperanim graph at the hypergraph level — see "Behavior composition" below. This split mirrors how Houdini KineFX (dataflow-shaped: bones are points, skinning is a SOP operation) is separate from runtime behavior FSMs that live in the consuming engine (Unreal AnimBP / Unity Animator).

Grounded in orkid's existing `XgmAnim` / `XgmAnimInst` / `XgmBlendPoseInfo` / `XgmAnimMask` (see `ork.lev2/inc/ork/lev2/gfx/gfxanim.h:197-478`). Materializes a `posegraph_ptr_t` whose `evaluate(time, params) -> xgmlocalpose_ptr_t` is a deterministic per-frame function of its inputs — no hidden state, no scene queries, no triggered transitions.

Plug types:
- `AnimClip` — wraps a single `XgmAnim` (procedurally authored: sin-driven jaw flap, Catmull-Rom keyframe sequence, mass-spring secondary motion) or a clip loaded from a `.xga` file
- `Pose` — wraps an `xgmlocalpose_ptr_t`; output of every clip/blend/IK node
- `BlendNode` — weighted blend of multiple input poses, optionally masked by `XgmAnimMask` (e.g. upper-body-only)
- `IKChain` — 2-bone or n-bone IK overlay (foot-plant, look-at, hand-reach); composes on top of an upstream pose
- `EventStream` — out-plug; fires events at clip beats (footstep frame, bite-impact frame). Out-edges are consumed by `particles` (footprint dust, chew crumbs) and `singularity` (footstep audio, chew sound) at the hypergraph level.

Materializer output: `posegraph_ptr_t` exposing `evaluate(time, params) -> xgmlocalpose_ptr_t`. Held by a component or invoked directly from a behavior FSM. The pose is fed into the existing skinning pipeline (orkid's skinned-PBR path consumes `xgmlocalpose_ptr_t` against an `xgmskeleton_ptr_t`).

Authoring sketch — the dataflow-shaped piece only:
```python
from orkengine.lev2 import Hyperanim
from ork.hypergraph.dflow import hyperanim as A

class QuadrupedPoseGraph(Hyperanim):
    """Per-frame pose evaluator: given (speed, jaw_open), produces a quadruped pose."""
    def __init__(self, skeleton):
        # procedural clips — pure functions of time + parameters
        idle  = A.clip(skeleton, "idle",  procedural=A.breathing(rate=0.4))
        walk  = A.clip(skeleton, "walk",  procedural=A.gait_quadruped(stride=1.0))
        bite  = A.clip(skeleton, "bite",  procedural=A.jaw_flap(rate=4.0, amp=0.3))
        chew  = A.clip(skeleton, "chew",  loop=True, procedural=A.jaw_flap(rate=8.0, amp=0.1))

        # weighted blending — pure function of input poses + weight params
        locomotion = A.blend_1d([idle, walk], param="speed")     # speed ∈ [0,1]
        jaw_layer  = A.blend_states([("none", A.zero_pose()),
                                     ("bite", bite),
                                     ("chew", chew)], param="jaw_state")

        # layered composition (full-body locomotion + upper-body jaw overlay)
        layered = A.layered(base=locomotion, overlay=jaw_layer,
                            mask=A.mask_upper_body(skeleton))

        # IK overlay — foot-plant on ground heightfield (looked up at evaluate-time)
        with_feet = A.ik_foot_plant(layered, ground_query="terrain_heightfield",
                                    foot_chains=["leg_FL", "leg_FR", "leg_BL", "leg_BR"])

        self.pose(with_feet)
        self.event("on_bite_impact", bite.at_beat(0.4))   # outgoing to particles + singularity
```

The output `posegraph_ptr_t` is parameterized by `speed` and `jaw_state` — the *caller* (a behavior FSM) sets these each frame and calls `evaluate(time, params)`. No FSM lives inside the graph.

Procedural clip authoring (`A.breathing`, `A.gait_quadruped`, `A.jaw_flap`) is the load-bearing piece — these synthesize `XgmAnim` channels from parameters rather than loading from disk, matching HyperSyn's procedural-everywhere philosophy. Loaded clips are also supported (`A.clip_from_file(path)`) but are not the primary authoring path.

**Behavior composition** (the stateful `behavior` family, in HyperSyn): the `behavior` family (see "behavior" rough spec below) wraps an FSM authored in the same Python DSL pattern; it holds a reference to a hyperanim `posegraph_ptr_t`, picks per-frame parameter values by FSM state, and consumes the EventStream out-edges. Example wiring: `LeopardBehavior(Behavior)` (stateful family) → drives `QuadrupedPoseGraph.params` (dataflow family) → reads `on_bite_impact` events → fires a hypergraph event → particle emitter spawns crumbs. The pose pipeline stays a deterministic per-frame function; the FSM is the stateful piece — both authored uniformly in HyperSyn, composed via the hypergraph.

### hyperlight — lighting + sky + fog

Lighting as a graph. Authors directional/point/spot/area lights, IBL probes, fog volumes, sky models. Consumes `StyleContext` so palette + atmosphere flow from one declaration. Composes with `terrain` (sun direction implies shadow shape on heightfield), `hypercity` (per-block streetlight density), `sequence` (time-of-day automation).

Plug types: `LightDescriptor` (type + color + intensity + falloff + cookie), `IBL` (HDRI source — file path or procedural sky), `FogProfile` (density + height falloff + color + scattering), `SkyModel` (Preetham / Hosek-Wilkie analytical sky, or HDR cubemap). Materializer output: `light_ptr_t[]` (registered with existing [[orklev2-lighting]] LightManager) + `EnvironmentProbe` (radiance/irradiance mips) + `FogParameters` (consumed by compositor).

Representative ops: `sun(direction, color, intensity)`, `point_array(positions, color, intensity, attenuation)`, `analytical_sky(turbidity, ground_albedo, sun_dir)`, `fog_height(density, falloff, color)`, `style_palette()` (helper consuming `StyleContext` to derive an entire lighting rig).

### hypershot — per-shot camera authoring

**Scope note**: hypershot covers the **dataflow-shaped** piece of cinematography only — authoring one camera at a time (lens, dolly path, focus pull, framing helpers). Shot *sequencing* ("shot A for 3 seconds, then cut to shot B with a wipe, then dolly through shot C") is timeline-driven and belongs to the future `sequence` family — same reasoning as hyperanim's behavior-FSM split: deterministic per-frame plug→artifact computation is dataflow; ordered triggering of which-thing-is-active-when is timeline/state-machine territory.

Plug types: `CameraDescriptor` (FOV/aspect/near/far), `Lens` (focal length + aperture + DoF), `DollyPath` (spline a camera moves along, parameterized by t∈[0,1]), `FocusCurve` (focus distance as a function of t∈[0,1] or distance to a target).

Materializer output: `camera_ptr_t` exposing `evaluate(t) -> CameraData` — a deterministic per-frame function of input parameters. Held by a `sequence` shot entry, or driven directly by an editor preview slider, or wired to a controller in the consuming app.

Representative ops: `static_camera(eye, target, up, fov)`, `dolly_along(path, lookat, lens)`, `orbit(target, radius, height, period)`, `focus_pull_target(curve_keyframes)`, `dolly_focus_combo(path, focus_target_node)` (helper composing dolly + focus curve).

Shot sequencing (when this camera is active, transitions to next shot, cut/dissolve/wipe) sits in `sequence` at M5 — a `sequence` timeline holds the ordered list of `camera_ptr_t` handles + per-transition data and drives the active camera at runtime.

### behavior — stateful FSM authoring (non-dataflow family)

The first **stateful** HyperSyn family. Wraps orkid's existing `[[orkcore-fsm]]` (hierarchical FSM with predicated guards, lambda callbacks, DOT visualization, Python bindings) in the same trace-then-materialize Python pattern the dataflow families use, so an LLM (or human) authors behavior alongside dataflow content with one uniform DSL surface.

Plug types:
- `State` — labeled state with optional `params` dict (parameter values active while in this state, fed to downstream pose/particle/material consumers each frame)
- `Transition` — typed edge from one state to another, guarded by a `Predicate`
- `Predicate` — boolean expression over `SceneQuery` results, `EventStream` inputs, timers, parameter comparisons
- `SceneQuery` — pluggable in-plug bound at materialize-time to a scene-side query function (distance to named entity, line-of-sight test, named-attribute read)
- `EventStream` — in-plug consuming events fired by dataflow families (e.g. hyperanim's `on_bite_impact`, particles' `on_collision`)

Materializer output: `behavior_ptr_t` exposing `step(dt, scene_ctx) -> StateChange | None` per frame. The consuming app advances the FSM each tick; state-change events flow back into the hypergraph to drive other families' parameters.

Authoring sketch:
```python
from orkengine.lev2 import Behavior
from ork.hypergraph.dflow import behavior as B

class LeopardBehavior(Behavior):
    def __init__(self, pose_graph):
        # states — each carries the params downstream pose graph reads while active
        wandering   = B.state("wandering",   params={"speed": 0.3, "jaw_state": "none"})
        approaching = B.state("approaching", params={"speed": 0.6, "jaw_state": "none"})
        biting      = B.state("biting",      params={"speed": 0.0, "jaw_state": "bite"})
        chewing     = B.state("chewing",     params={"speed": 0.0, "jaw_state": "chew"})

        # transitions — predicates over scene queries + event streams + timers
        # NOTE: B.sees_target / B.within_distance are consumer-registered predicates
        # (any external library / app supplies them via @op extension; see "extensibility" below).
        # Only B.event_from / B.timer are engine-neutral defaults in orkid.
        B.transition(wandering,   approaching, when=B.sees_target("cake"))
        B.transition(approaching, biting,      when=B.within_distance("cake", 0.5))
        B.transition(biting,      chewing,     when=B.event_from(pose_graph, "on_bite_impact"))
        B.transition(chewing,     wandering,   when=B.timer(2.0))

        # composition with dataflow families
        self.drives(pose_graph)             # state.params → pose_graph.evaluate(params)
        self.listens_to(pose_graph)         # pose_graph EventStream → transition predicates
        self.initial(wandering)
```

Representative engine-neutral ops (shipped with orkid):
- States: `B.state(name, params=...)`, `B.composite_state(name, sub_fsm=...)` (hierarchical)
- Transitions: `B.transition(from_state, to_state, when=predicate)`, `B.any_state_transition(to_state, when=...)`
- Predicates (engine-neutral): `B.event_from(family_graph, event_name)`, `B.timer(seconds)`, `B.param_eq(name, value)`, `B.and_(p1, p2)`, `B.or_(...)`, `B.not_(...)`
- Composition: `B.drives(family_graph)`, `B.listens_to(family_graph)`, `B.triggers(family_graph, action_name)`
- Lifecycle: `B.on_enter(state, callback)`, `B.on_exit(state, callback)`, `B.on_step(state, callback)`

**Predicate + action vocabularies are op-registry-extensible.** Predicates that need scene knowledge (`sees_target`, `within_distance`, `gazed_at`, `grasped_by`, `touched_by`, `proximity_to_user`) and actions that need runtime side-effects (`haptic_pulse`, controller rumble, audio cue) are **not** in orkid — they belong in the consuming app or layer. Any consumer can register additional predicates and actions via the same `@op`-style decorator mechanism the other families use:

```python
# example consumer-side extension (lives in the consumer's package, not in orkid)
from ork.hypergraph.dflow.dsl import op
from ork.hypergraph.dflow.behavior.types import Predicate, Action

@op(family="behavior", kind="predicate", name="within_distance")
class WithinDistance:
    inputs = {"target_name": str, "max_distance": float}
    @staticmethod
    def evaluate(scene_ctx, target_name, max_distance):
        target = scene_ctx.find(target_name)
        return target is not None and (scene_ctx.self_pos - target.pos).length() <= max_distance

@op(family="behavior", kind="action", name="haptic_pulse")
class HapticPulse:
    inputs = {"controller": str, "intensity": float, "duration_ms": int}
    @staticmethod
    def execute(scene_ctx, controller, intensity, duration_ms):
        scene_ctx.vr.haptic_pulse(controller, intensity, duration_ms)
```

Registered ops become available on the `B.*` namespace the same as built-ins. The behavior validator (see check codes below) verifies predicate/action references resolve via the registry; an unresolved reference produces `UNDEFINED_PREDICATE_REFERENCE` or `UNDEFINED_ACTION_REFERENCE` so the LLM authoring loop catches it at validate time.

`SceneQuery` is a pluggable in-plug bound by the consumer at materialize time — orkid has no opinion on what scene queries exist; the consumer supplies the query implementation when it instantiates the behavior runtime.

Validator (subprocess + dry_run) adds FSM-specific checks beyond the dataflow checks:
- `UNREACHABLE_STATE` — state has no incoming transitions and is not the initial state
- `DEAD_END_STATE` — state has no outgoing transitions and isn't terminal-by-design
- `CYCLE_WITHOUT_ESCAPE` — strongly-connected component with no exit transition
- `UNDEFINED_PREDICATE_REFERENCE` — predicate name not registered in the behavior op registry (engine-neutral defaults missing, or a consumer-extension predicate that wasn't registered before validate)
- `UNDEFINED_ACTION_REFERENCE` — action name not registered (same cause as predicate)
- `MISSING_DRIVE_TARGET` — `B.drives(X)` references a graph that doesn't exist in the hypergraph

Composes naturally with every dataflow family: drives `hyperanim` pose-graph parameters, gates `particles` system start/stop, swaps `ptex3d` materials, triggers `singularity` synth patches, advances `sequence` timeline cues.

### Sub-system: StyleContext

Not a family — a *cross-family context* every materializer reads. A `StyleContext("new_wave_1983")` (or any named profile) carries a palette set, signage glyph atlas, fog descriptors, music keys, era cues. Every materializer that consults `ctx.style` produces output tinted/styled accordingly: ptex3d picks palette-aware albedo defaults; hyperlight derives a key-fill-rim color triad from the palette; hypercity selects building archetypes consistent with the era; hyperprim restricts prop catalogs to era-appropriate items.

Determinism requirement: a style hash feeds the materializer cache key, so "same graph + same style" always yields the same cached artifact. Styles are first-class graphs in their own right (authored once, referenced from any other graph).

Lives at `obt.project/scripts/ork/hypergraph/dflow/style/` with a small registry of curated profiles (`new_wave_1983`, `cyberpunk_2077`, `arts_and_crafts_1900`, etc.) plus user-authorable custom profiles.

### Out of scope

- **Artist asset import.** [[orkcore-catalog]] handles imported artist content (modeled meshes, baked textures, hand-authored animations). HyperSyn is the **procedural** authoring layer — meshes generated from SDFs/grammar, textures generated from shader codegen, animations synthesized from parameters. The two systems coexist (a scene can consume both) but they are separate authoring stacks. HyperSyn does not have an `assetdb.query()` op.
- **Skeletal animation from imported `.xga` files**, except as one input to `hyperanim` (`A.clip_from_file`). The primary authoring path is procedural clip synthesis (`A.breathing`, `A.gait_quadruped`).

## File layout reference

```
ork.core/inc/ork/dataflow/                        dataflow runtime headers
ork.core/src/dataflow/                            dataflow runtime impl
ork.core/pyext/pyext_dataflow.cpp                 dataflow python bindings

ork.lev2/inc/ork/lev2/gfx/particle/               particles runtime
ork.lev2/inc/ork/lev2/gfx/hypermesh/              hypermesh runtime (see ork.dox/hyper/HYPERMESH.md)
ork.lev2/inc/ork/lev2/gfx/terrain/                terrain runtime (dflow/hfdflow.h bake driver +
                                                  terrain_chunk_drawable; see ork.dox/hyper/HYPERTERRAIN.md)
ork.lev2/src/gfx/sdf/                             sdf family C++ modules (SdfEval/Csg/SdfToMesh/…)
(ptex3d has NO C++ family runtime — it is Python fxv2 codegen; ptex2d is planned M3)

obt.project/scripts/ork/hypergraph/                          HyperSyn Python root (namespace layout: PLAN.md)
obt.project/scripts/ork/hypergraph/ptex3d/                   SHIPPED ptex3d surface DSL (TOP-LEVEL, 8 files)
obt.project/scripts/ork/hypergraph/sound/                    SHIPPED hypersound DSL v1 (dsl/emitter/caps)
obt.project/scripts/ork/hypergraph/registry.py               SHIPPED minimal op registry (register_op/op/get_op)
obt.project/scripts/ork/hypergraph/dflow/dsl/                trace-mode introspection machinery (PLANNED M1.B)
obt.project/scripts/ork/hypergraph/dflow/validate/           subprocess validation harness (PLANNED M2)
obt.project/scripts/ork/hypergraph/dflow/particles/          particles DSL vocab + base class
obt.project/scripts/ork/hypergraph/dflow/ptex2d/             ptex2d DSL (PLANNED M3)
obt.project/scripts/ork/hypergraph/dflow/hypermesh/          hypermesh DSL vocab + base class + materializer
obt.project/scripts/ork/hypergraph/dflow/sdf/                sdf expression algebra + fluent brick builder
obt.project/scripts/ork/hypergraph/dflow/lsystem/            L-system grammar DSL + presets
obt.project/scripts/ork/hypergraph/dflow/roads/              roads layer (route/mask/roadmesh)
obt.project/scripts/ork/hypergraph/dflow/terrain/            terrain DSL vocab + base class + manifest + bake
obt.project/scripts/ork/hypergraph/assets/                   real recipes (hypermesh/ materials/ terrain/ …)

ork.lev2/pyext/tests/hypersyn/                    HyperSyn example/test root (visual, runnable)
ork.lev2/pyext/tests/hypersyn/particles/          particle DSL examples (one .py per scene)
ork.lev2/pyext/tests/hypersyn/ptex2d/             ptex2d DSL examples
ork.lev2/pyext/tests/hypersyn/ptex3d/             ptex3d DSL examples (LeopardSkin, etc.)
ork.lev2/pyext/tests/hypersyn/hypermesh/          hypermesh DSL examples
ork.lev2/pyext/tests/hypersyn/terrain/            terrain DSL examples
```

Each family directory has the same shape:
```
ork/dflow/<family>/
  __init__.py          # exports family base class + DSL ops (the aliased-import surface)
  base.py              # FamilyBase subclass (e.g. Ptex3d, Hypermesh)
  types.py             # plug value types (Float, Vec3, MeshRef, ...)
  ops/                 # one .py per DSL op
    __init__.py        # populates the family registry
    mix.py
    randnormal.py
    ...
  materialize.py       # graph → typed artifact (or None) — runs in subprocess
  context.py           # input-context types (e.g. ptx3dctx with .xyz, .nxyz)
```

### Examples per family

Each family ships runnable visual examples at `ork.lev2/pyext/tests/hypersyn/<family>/`, mirroring the existing `ork.lev2/pyext/tests/renderer/particles/ptc_*.py` convention. Each example is a complete runnable app that constructs a HyperSyn subclass, calls `generatedflow()` → `materialize()`, attaches the resulting artifact to a scenegraph, and renders to screen (the existing visual-test convention — for human inspection, not callback counting).

The registry indexes examples via `ork.hypergraph.dflow.dsl.registry.list_examples(family) -> list[ExampleInfo]`:

```python
@dataclass(frozen=True)
class ExampleInfo:
    name: str                             # "leopard_skin"
    family: str                           # "ptex3d"
    path: str                             # absolute path to the .py file
    description: str                      # extracted from the file's module docstring
    subclass_name: str                    # "LeopardSkin"
    ops_used: list[tuple[str, str]]       # [(family, op_name), ...] back-populated for OpInfo.examples
```

Examples serve double duty as integration test fixtures: the validate+materialize CI loop runs every example as a regression baseline. Materializer output hashes are recorded so any unexpected change in generated FXV2 / mesh / texture content is caught.

**Minimum example counts per family** (target, not a hard rule): particles ≥10, ptex2d ≥10, ptex3d ≥25 (richest vocabulary, most active downstream consumers), hypermesh ≥10, terrain ≥5. New ops should land with at least one example demonstrating them.

## How to use this skill

When a user asks about authoring procedural content with HyperSyn:

1. Identify the **family** they want (particles / ptex2d / ptex3d / hypermesh / sdf / terrain / future). `sdf` is a shipped standalone family (see the sdf note in Core concepts); `terrain` and `hypermesh` are shipped — the authoritative current-state docs are `ork.dox/hyper/HYPERTERRAIN.md` and `ork.dox/hyper/HYPERMESH.md` (plus `OGEO.md`/`HYPERECS.md` and `ork.dox/core/dataflow.md` for the substrate). `lsystem` shipped as the first structural-spine family; its flora/city/creature siblings are gated on the spine — see `UNIFIED_SUBSTRATE.md` §11–§16.
2. Show the **subclass + `__init__` + `self.<sink>(...)`** pattern (the trace).
3. Show **`generatedflow()` → `validate()` → `materialize()`** for the full pipeline.
4. For codegen families (ptex2d, ptex3d), explain the **FXV2 fragment** mechanism and show how an op declares its fragment template.
5. Always recommend that materialization run in subprocess (via the provided harness) so the editor never crashes.

When a user asks how to add a new DSL op:

1. New file at `obt.project/scripts/ork/hypergraph/dflow/<family>/ops/<name>.py`.
2. Class decorated with `@op(family=..., name=...)` declaring `inputs` / `outputs` and a `build(graph, **inputs)` static method.
3. For codegen families, declare `fxv2_fragment` template string.
4. Add the op to the family's `ops/__init__.py` registry.

When a user asks how to add a new family:

1. C++ side: family-base `DgModuleData`/`DgModuleInst`, plug traits, plug-template instantiation .cpp, registration in `lev2_init.cpp`, pyext bindings. Follow the particles pattern (`ork.lev2/src/gfx/particles/`).
2. Python side: `obt.project/scripts/ork/hypergraph/dflow/<family>/` per the file-layout reference.
3. Materializer: subprocess-safe; return typed artifact or None.

When a user asks about hypergraphs, the timeline, or audio integration: they are **future (M4+)**. The current architecture preserves stable plug IDs, a cross-family type registry, and per-module phase annotations to accommodate them, but no API is committed yet. Don't fabricate one.

## Cross-references

- [[orkcore-dataflow]] — the underlying graph runtime (modules, plugs, sorter, registers)
- [[orklev2-particles]] — the existing particles family (the M1 DSL test bed)
- [[orklev2-shader]] — FXV2 shader format (the codegen target for ptex2d/ptex3d)
- [[orklev2-pbr]] — PBRMaterial structure (the ptex3d materializer output)
- [[orklev2-audio]] — Singularity synth (future M5 family)
- [[orkcore-reflection]] — the reflect system used by `ConnectionsProperty` serialization
