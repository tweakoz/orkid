# HyperSyn — Implementation Plan

This is the implementation roadmap. The contract / API surface lives in `SKILL.md`. When the user is *using* HyperSyn (authoring graphs, asking how families work, looking up materializer outputs), the skill is the source of truth. When you are *building* HyperSyn (deciding what to commit next, tracking which milestones are done, applying review findings), this plan is the source of truth.

## Status

The implementation status of each component is the source of truth; when in doubt, check the code, not this plan.

| Component | Status | Location |
|---|---|---|
| `ork::dataflow` runtime (graphs, modules, plugs, sorter) | implemented | `ork.core/{inc,src}/ork/dataflow/` |
| `ork::lev2::particle` family on dataflow | implemented | `ork.lev2/{inc,src}/ork/lev2/gfx/particle/` |
| Python imperative graph construction | implemented | `ork.core/pyext/pyext_dataflow.cpp` |
| **`ork.hypergraph` Python namespace + asset taxonomy** | **implemented** | `obt.project/scripts/ork/hypergraph/` — see "Namespace layout" section below |
| **Author-helper utilities** (`hsv()` / `wavelength()` / `colortemp()`, `colors` palette, `axis_angle()`, dict-form transforms, nested-form material lobes + TypedDicts) | **implemented** | `obt.project/scripts/ork/hypergraph/colors.py`, `obt.project/scripts/ork/hypergraph/ecs/scene/__init__.py`, `obt.project/scripts/ork/hypergraph/ecs/scene/assets.py` |
| Runtime hardening (cycle detect, fanout, type check, required, codec-generic setter, MaterializationPhase) | **planned (M0)** | `ork.core/src/dataflow/dataflow_sorter.cpp`, `graph_data.cpp`, `pyext_dataflow.cpp`, `ork.core/inc/ork/dataflow/module.h` |
| `dflow.dsl` tracing core (trace context, DslNode, Expr, bindings) | **partially implemented (M1.A)** | `obt.project/scripts/ork/hypergraph/dflow/_trace.py`, `_expr.py`, `_bindings.py`, `_lower.py` — core trace + DslNode + Expr + bindings staging all in place; the `dsl/` introspection package (registry, OpInfo, examples API) is M1.B and not yet built |
| `particles` DSL vocab (test bed) | **partially implemented (M1.A)** | `obt.project/scripts/ork/hypergraph/dflow/particles/` — `ParticleSystem` family base + 22 `chain_op`-based ops covering emitters/forces/attractors/renderers/colliders. **Pending M1.B**: `@op` decorator, registry, `OpInfo`/`PlugSpec`/`ExampleInfo`, source provenance, multi-output op support, examples-per-family directory + indexing |
| `dflow.validate` subprocess harness | **planned (M2)** — genuinely unbuilt; NOT the same-named hypermesh meshvet (`_ork.hypermesh.validate.py`) which is an unrelated per-asset trimesh check | `obt.project/scripts/ork/hypergraph/dflow/validate/`, zmq worker pool |
| `ptex2d` family + FXV2 codegen | **planned (M3)** | `ork.lev2/{inc,src}/ork/lev2/gfx/ptex2d/`, `obt.project/scripts/ork/hypergraph/dflow/ptex2d/` |
| `ptex3d` family (surface DSL + fxv2 codegen) | **shipped** — expression DSL → forward-PBR `.fxv2` (GEOV2); no C++ family runtime (pure Python codegen); TOP-LEVEL package, NOT `dflow.ptex3d` | `obt.project/scripts/ork/hypergraph/ptex3d/` (8 files; `materialize_ptex3d`); recipes in `assets/materials/` |
| `hypermesh` family + mesh/SDF ops | **shipped** (core in daily use; honest gaps in `ork.dox/hyper/HYPERMESH.md` §10) | `ork.lev2/{inc,src}/ork/lev2/gfx/hypermesh/`, `obt.project/scripts/ork/hypergraph/dflow/hypermesh/`; doc `ork.dox/hyper/HYPERMESH.md` |
| `sdf` family + SDF brick / CSG / marching-tetrahedra | **shipped (E.7 track: M0–M2 LANDED; M3 NanoVDB reserved/unbuilt)** | `ork.lev2/src/gfx/sdf/`, `obt.project/scripts/ork/hypergraph/dflow/sdf/` |
| `terrain` family + heightfield bake | **shipped** (expression-first DSL, compute bake, manifest scale contract, `TerrainChunkDrawableData` renderer) | `ork.lev2/{inc,src}/ork/lev2/gfx/terrain/`, `obt.project/scripts/ork/hypergraph/dflow/terrain/`; recipes `assets/terrain/`; doc `ork.dox/hyper/HYPERTERRAIN.md` |
| Hypermesh procedural rigging + skinning | **planned (feasibility-gated; bigger than M3.5 implies)** — gated on the `XfNodeGraph` interchange plug, writable `XgmSkeleton` bindings + a `XfNodeGraph→XgmSkeleton` flattener, AND a NEW GPU-skinning render path (no skinned hypermesh VS exists). Sized L+, not a thin extension. | `ork.lev2/.../hypermesh/`; see `UNIFIED_SUBSTRATE.md` §15–§16 |
| **Structural spine** (`XfNodeGraph` interchange plug + sweep + rewrite/grammar engine) | **foundation LANDED (2026-06-25)** — the `XfNodeGraph` interchange currency (G0a), the `LSweep` skinner (G0b), and the L-system slice (G1) are in code; the grammar generalization + creature/city milestones are forward | `ork.lev2/.../gfx/dflow/interchange.h` (`XfNode*`), `hmdflow_module_lsystem/lsweep.cpp`; spec in `UNIFIED_SUBSTRATE.md` §11–§16 |
| New generator families (`lsystem` + space-colonization / phyllotaxis / tensor-field / straight-skeleton) | **`lsystem` LANDED (M1); the rest forward** — each = a rule vocabulary + interpret kernel + sink topology assertion over `XfNodeGraph` | `lsystem` shipped (`hmdflow_module_lsystem.cpp`, `scn_lsystem.py`); rest gated on the spine; spec in `UNIFIED_SUBSTRATE.md` §12–§14 |
| Hypergraph coordinator + timeline | **future (M4)** | TBD |
| `hyperprim` family (parametric props) | **forward** | catalog layer over hypermesh; `xgmmodel_ptr_t` + named PrimSlot dict |
| `hyperarch` family (CGA-grammar architecture) | **forward** | grammar-rule mesh+slot generation; composes ptex3d for facades, hyperprim for openings |
| `hypercity` family (urban layout) | **forward** | street graph → parcels → per-parcel hyperarch; tile-partition for streaming |
| `hyperanim` family (animation behavior) | **forward** | wraps `XgmAnim`/`XgmAnimInst`/`XgmBlendPoseInfo`; FSM + blend tree + IK overlay; procedural clip synthesis primary path |
| `hyperlight` family (lighting/IBL/fog/sky) | **forward** | composes with terrain + hypercity + style context |
| `hypershot` family (per-shot camera authoring) | **forward** | cameras + dolly + focus pull; per-shot only — sequencing belongs to future `sequence` family |
| `behavior` family (stateful FSM authoring) | **forward** | first non-dataflow HyperSyn family; wraps `[[orkcore-fsm]]`; drives pose graphs / particles / materials via hypergraph composition |
| `StyleContext` sub-system | **forward** | cross-family palette/era/fog context; deterministic hash feeds materializer cache |
| `singularity` DSL family (hypersound) | **shipped v1** — sound DSL (`SoundPatch`/`S.*`/`materialize_sound_patch`), caps validation, ECS emitters | `obt.project/scripts/ork/hypergraph/sound/{dsl,emitter,caps}.py`; tests `ork.lev2/pyext/tests/singularity/test_hypersound_{basic,arith}.py`; scene `ork.data/scenes/scn_spatial_audio_showcase.py` |
| `sequence` DSL family | **future (M5)** | TBD |
| Node-graph editor canvas | **future (M6)** | builds on `lev2.ui.PrimCanvas`; uses `DgModuleData::mgvpos` |
| ECS components (`HyperSynGraphComponent` per family) | **future** | depends on M3+ + cache layer |

**Status legend**: *implemented* = exists in code; *planned* = on the M0–M3 roadmap with committed API contract; *forward* = designed with rough spec in SKILL.md, API surface stable enough for hypergraph/ECS planning, no implementation timeline; *future* = vague pointer at the right area, no API committed.

Editor UI uses the current `lev2.ui.PropertySheet` + `ReflectionPropertySheetModel` + composite-widget pattern (see `ork.editor.ecsedit`, `ork.hdri.studio`, `ork.lev2/pyext/tests/ui/widget_pack.py`). Do not extend the legacy `ged` system.

## Namespace layout

The HyperSyn Python authoring stack lives under one top-level package: `ork.hypergraph` (`obt.project/scripts/ork/hypergraph/`). Pre-existing `ork.ecs.*` / `ork.dflow.*` import paths were migrated under this namespace.

```
hypergraph/                    # (regenerated from the tree 2026-08-01)
├── colors.py                  # universal: hsv(), colors palette, _Hsv
├── exprir.py                  # canonical ExprIR tree (author-intent round-trip)
├── registry.py                # SHIPPED minimal op registry: register_op / @op / get_op / list_ops
├── units.py                   # natural-units helpers
├── asset_core/                # constructors, converters, loaders (infrastructure)
│   ├── material/  sdf/  drawable/  particle/  probe/
├── assets/                    # concrete named asset wrappers + real recipes
│   ├── hypermesh/  materials/  mesh/  sdf/  terrain/
├── ecs/                       # ECS-coupled Scene DSL + runtime
│   ├── runtime.py             #   EcsRuntime
│   ├── audio_manifest.py      #   hypersound scene wiring
│   └── scene/                 #   Scene, Transform, SceneGraphHandle, axis_angle, assets
├── ptex3d/                    # SHIPPED surface DSL (TOP-LEVEL; fxv2 codegen — NOT under dflow/)
├── sound/                     # SHIPPED hypersound DSL v1 (dsl / emitter / caps)
└── dflow/                     # dataflow DSLs + trace machinery (_trace/_expr/_bindings/_lower/
    ├── hypermesh/  lsystem/   #   _parampack, document/graphdata_document/hypermesh_document,
    ├── particles/  roads/     #   testbench)
    └── sdf/  terrain/
```

The authoritative per-family current-state docs are `ork.dox/hyper/{HYPERMESH,HYPERTERRAIN,OGEO,HYPERECS}.md` and `ork.dox/core/dataflow.md` — consult those rather than this sketch when they disagree.

**Asset taxonomy rule.** `asset_core/<category>/` houses *infrastructure* — generic constructors / converters / loaders (`ImplicitSdf` takes arbitrary GLSL; `MeshToSdf` is a converter; `HdriToXir` loads). `assets/<category>/<name>.py` houses *concrete named things* — recognizable primitives or user-authored content (`SphereSdf`, `ThickSaddleSdf`, future `pillar.py`, `bridge.py`, etc.). Both directories share the same per-output-type subdir taxonomy so the parallel is obvious: "anything SDF-related" lives in two predictable places.

**One class per file.** Each `assets/<category>/<name>.py` defines one wrapper class. Authors writing new content drop new files into `assets/<category>/`. File name → class name maps directly (snake_case → CamelCase).

**Backward-compat (M0 transitional).** During the namespace transition, the per-class files under `asset_core/<category>/` and `assets/<category>/` are *re-export shims* — the class definitions still physically live in `hypergraph/ecs/scene/assets.py`. Import paths from author code are correct on day one; the physical per-file split is a pure code-move follow-up with no public-surface change.

**Convenience re-exports.** `ork.hypergraph.__init__` exposes the most common imports so casual authors can write `from ork.hypergraph import Scene, Transform, axis_angle, hsv, colors`. Granular paths (`from ork.hypergraph.assets.sdf.sphere import SphereSdf`) work for code that wants explicit attribution.

## Author-helper utilities (landed before M1)

These are the ergonomic primitives sitting between the orkengine bindings and the HyperSyn DSL surface. They aren't tied to any specific family but every DSL author / family uses them.

### Color (`ork.hypergraph.colors`)

All four constructors below return a `_LazyColor` subclass (`_Hsv`, `_Wavelength`, `_ColorTemp`, or the palette-backing form) that auto-coerces to vec3 or vec4 at the consumption slot. The PbrMaterial dispatcher does it automatically per the field-name registry; other code calls `.to_vec3()` / `.to_vec4()` explicitly. Alpha != 1 fed to a vec3 slot raises `ValueError`.

- **`hsv(h, s, v, a=1.0)`** — HSV-constructed color. Hue in degrees [0, 360], s/v/a in [0, 1].
- **`wavelength(nm, a=1.0)`** — visible-spectrum monochromatic color via Dan Bruton's piecewise approximation + edge intensity falloff + gamma 0.8. Useful reference points: 405 nm violet laser, 450 nm blue, 532 nm green laser, 555 nm peak photopic, 589 nm sodium-D (streetlamp yellow), 650 nm red laser. Outside ~380-780 nm returns black.
- **`colortemp(k, a=1.0)`** — black-body color temperature via Tanner Helland's algorithm. Clamped to [1000, 40000] K. Reference points: 1900 K candle flame, 3200 K tungsten/halogen, 5500 K noon daylight, 6500 K D65 monitor white, 10000 K cool overcast.
- **`colors`** — 64-name palette as `_Hsv` instances: primaries/secondaries (12), neutrals (8), skin tones (skin1..skin6, light→dark), wood (oak1/oak2/mahogany/birch), stone (granite/marble/slate/jade/ruby/sapphire), plants (leaf1/leaf2/grass/moss/sage/autumn), sky/water (skyblue1/skyblue2/sunset/dawn/fog/ocean/lagoon/lake), metals (gold/silver/copper/brass/iron/rust), fire/warm (ember/flame/terracotta/coral), misc (chocolate/wine/lavender/salmon). Used as `colors.oak1`, `colors.skyblue1`, etc.
- **`_LazyColor`** — common base class. PbrMaterial dispatcher uses `isinstance(value, _LazyColor)` so all four producers are picked up uniformly.
- **`_PBR_VEC3_FIELDS` / `_PBR_VEC4_FIELDS`** — registry used by `PbrMaterial._coerce_hsv` for slot→type dispatch. Updated when new reflected color fields land on materials.

### Transform (`ork.hypergraph.ecs.scene`)

- **`Transform(**kwargs)`** — kwargs-style constructor for `lev2.Transform` (existing).
- **`axis_angle(axis, angle)`** — orientation DSL helper, returns a quat via `quat.createFromAxisAngle`. Reads cleanly in `transform={"orientation": axis_angle(vec3(0,1,0), math.pi/4)}`.
- **`_coerce_transform`** — accepts `Transform | dict | None`, returns `Transform | None`. Wired into `Scene.spawner` (every entity/spawner path routes through). Author can pass either form interchangeably:
  ```python
  self.entity("statue",
      transform={"translation": vec3(-5.5, 0, 0),
                 "orientation": axis_angle(vec3(0, 1, 0), math.pi/4),
                 "scale": 1.5},
      components=[...])
  ```

### PbrMaterial — dual-form authoring (`ork.hypergraph.asset_core.material.pbr`)

PbrMaterial accepts two equivalent forms — flat (LLM-friendly, glTF-aligned) and nested (human-friendly, mirrors glTF JSON structure). Mixing forms across different lobes is allowed; mixing within one lobe is rejected.

```python
# Flat — preferred for LLM-authored code (long names match glTF spec)
PbrMaterial("jade", base_color=colors.marble,
            subsurface_color=colors.jade, subsurface_factor=1.0,
            transmission_factor=0.15, transmission_roughness=0.3, ior=1.55)

# Nested — preferred for human authoring (visual grouping)
PbrMaterial("jade",
    base = {"color": colors.marble, "roughness": 0.85},
    subsurface = {"color": colors.jade, "factor": 1.0, "radius": vec3(0.15, 0.55, 0.05)},
    transmission = {"factor": 0.15, "roughness": 0.3},
    ior = 1.55,
    volume = {"attenuation_color": colors.jade, "attenuation_distance": 1.0})
```

**TypedDicts per lobe** (`BaseLobe`, `TransmissionLobe`, `VolumeLobe`, `DiffuseTransmissionLobe`, `SpecularLobe`, `ClearcoatLobe`, `SheenLobe`, `IridescenceLobe`, `SubsurfaceLobe`) declare the closed inner-key vocabulary so IDEs / type checkers can complete and validate inner keys. `_NESTED_LOBES` mapping table drives the flat→nested translation with explicit `remap` rules for fields that don't take a lobe-name prefix (volume's `attenuation_*` fields, base's `color`→`base_color`).

**hsv() coercion at the boundary.** `PbrMaterial._coerce_hsv` walks the kwargs after `_flatten_nested` and converts any `_Hsv` value to vec3 or vec4 per `_PBR_VEC3_FIELDS` / `_PBR_VEC4_FIELDS`. Other code paths (e.g., `PostFxNodeSSSS.subsurface_tint = colors.skin2`) call `.to_vec3()` / `.to_vec4()` manually.

### Implications for forthcoming DSL work

- **M1 `dflow.dsl` should treat `_Hsv`-style "lazy color" + "lazy transform" as a general DSL-node pattern** — any DSL value that defers its concrete type until consumption follows the same shape: an object with `.to_<typeX>()` methods, optionally with assertion on conversion mismatch. The hsv/dict-transform implementations are the reference for how a future `dsl.Vec3Node`, `dsl.QuatNode`, `dsl.FloatNode` should behave at consumption boundaries.
- **TypedDict-per-section** is the recommended discoverability + LLM-correctness pattern when a DSL surface has multiple named sub-blocks (lobes, families' sink declarations, op output bundles). Use this for P3.F dataflow-codegen materializer output declarations.

## Milestone roadmap

| Milestone | Goal | Gating prerequisite |
|---|---|---|
| **M0** | Runtime hardening — make `ork::dataflow` safe for DSL-authored and LLM-authored graphs | — (start here) |
| **M1.A** | Trace core + particles DSL vocab — proves the trace→graphdata round-trip against the existing particles runtime | M0 |
| **M1.B** | Op registry + introspection (`@op` decorator, `OpInfo`/`PlugSpec`/`ExampleInfo`, source provenance, multi-output ops, examples-per-family) — exposes the DSL surface to editor/LLM tooling | M1.A |
| **M2** | `dflow.validate` subprocess harness — zmq REQ/REP worker pool, persistent across editor session and into live-mutable runtimes | M1 |
| **M3** | HyperSyn DSL-vocab + materializer layer per family. NOTE: for particles / ptex3d / hypermesh / terrain / sdf (and hypersound) the DSL vocab **and** materializers already SHIPPED (see the Status table — ptex3d’s full surface DSL + fxv2 codegen included); M3’s real remaining scope is `ptex2d` (the only genuinely unbuilt family) plus the M1.B registry/introspection wrappers where missing | M0, M1, M2 |
| **M3.5** | Hypermesh rigging + skinning extension — procedural skeleton/weight ops yielding `xgmmodel_ptr_t` with skinning bound; prerequisite for hyperanim | M3 |
| **M4** | Hypergraph coordinator + timeline — cross-family graph composition + phased materialization scheduling | M3 |
| **M5** | Singularity + sequence DSL families — audio-reactive cross-family use cases | M3, M4 (and needs cycle-permitting subgraphs for audio feedback) |
| **M6** | Node-graph editor canvas — `DgGraphCanvas` widget on `PrimCanvas` consuming `DgModuleData::mgvpos`; round-trips with file format | M3 (and benefits from validator harness from M2) |
| **Forward** | hyperprim, hyperarch, hypercity, hyperanim, hyperlight, hypershot, behavior, StyleContext — each rough-specced in SKILL.md; implementation order TBD when M3 stabilizes. `behavior` is the first **stateful** family (FSM runtime, not dataflow); see "Family kinds" in SKILL.md | M3 (hyperanim additionally needs M3.5; behavior needs the validator harness from M2 + FSM-specific check rules) |

## Structural-spine milestones (forests-adjacent; feasibility-gated)

> The holistic substrate review (`UNIFIED_SUBSTRATE.md` §11–§16) adds a *structural / topology* spine
> (one `XfNodeGraph` currency + one rewrite engine + shared ops) on top of the expression-IR substrate.
> A 9-verdict feasibility pass found the naive G0–G3 order infeasible-as-written and reshaped it. The
> milestones below are the feasibility-adjusted order. **G0a/G0b/G1 LANDED (2026-06-25)** — the foundational
> `XfNodeGraph` plug + `LSweep` skinner + L-system slice are in code; **G2a/G3 remain forward** and await an
> owner sequencing call vs the LOCKED racer-first gate (A3 #5). Each is GREEN only when it renders (or passes
> an OBJ/SSBO readback oracle where nothing skins yet).

| Milestone | Goal | Feasibility note |
|---|---|---|
| **G0a** ✅ LANDED | `XfNodeGraph` interchange plug (reflected, JSON round-trips, registers in BOTH grammar + both hmdflow dgctx blocks) + Python host-fill authoring | Mechanical `SdfGrid`-recipe copy; but it is a `GpuMesh`-SHAPED type (CSR edges + `_slots` + dual-version dirty), NOT a byte-for-byte `InstanceSet` copy. |
| **G0b** ✅ LANDED | `sweep`/`LSweep` module (`XfNodeGraph`→GpuMesh) — renders a swept tube | Transported profile frame is NEW shader math; reuses only the extrude ring-field buffers + `cs_face_seg` stitching. **DROP `BONEIDX`/`BONEWT` (no consumer); rename `skin/sweep`→`sweep`.** |
| **G1** ✅ LANDED (v1) | L-system family (M1): `LSystemModule` produces an `XfNodeGraph` skeleton (v1 = hardcoded bracketed parametric grammar, CPU-side at activate), `LSweep` skins it. Forward: reflected-`LRuleSet` grammar (rules as JSON, NOT the GLSL SelExpr ABI) + GPU rewrite; terrain-slope `Field` bias. | Landed as the vertical slice; the general rewrite engine + branch-capable/data-dependent topology remain the forward part. |
| **G2a** | `radial_repeat` (mirror's rotational sibling) + Python `LegChain` over existing `extrude_faces` → static 8-legged GpuMesh (vocab+interpreter, GENERATION half only) | Feasible M. The skeleton/skin/DRIVE half is deferred behind: writable `XgmSkeleton` bindings → SKELETON sink+plug → a NEW GPU-skin render path → `auto_skin` → `bake_skeleton`. Sized L, NOT 'no new engine'. |
| **G3** | Building: (U1) NEW mesh-merge/join module first → (U2) footprint→walls→roof authored in the imperative DSL (roof = extrude+inset cap, NOT straight-skeleton) → (U3) optional thin split/repeat layer lowering onto U1+U2 | G3-as-written infeasible: no scope IR, no mesh-merge (every module single-input), no multi-sink materializer, straight-skeleton is a separate multi-week algorithm. **Decouple from G2** — a building grammar needs no creature vocabulary. |

**First commitment G0a + G0b + G1 — LANDED (2026-06-25).** Next: reflected-`LRuleSet` grammar generalization
+ GPU L-system, then G2a/G3. The forests-vs-racer tension is an owner sequencing call (the racer gate is
unchanged either way).

## M0 — Runtime hardening checklist

**Scoped down after evaluation.** Step 1 (cycle detection) was a real bug fix — the old code infinite-looped on cyclic graphs. Steps 2-7 below were originally framed as M0 prerequisites but on examination are speculative pre-work for consumers (validator, M3 families, M4 hypergraph) that don't exist yet. Particles run fine with the existing runtime. Each deferred item should be revisited when its actual consumer is being built — at that point the requirements will be concrete and the design won't be guesswork.

### M0 (done)

1. **Cycle detection — two-site fix.** ✅ Landed. `DgSorter::generateTopology` (`ork.core/src/dataflow/dataflow_sorter.cpp:337-351`) used to infinite-loop if `_pending` never drained, and `DgSorter` ctor's call to `pmod->computeMinDepth()/computeMaxDepth()` (`dataflow_sorter.cpp:53-76`) infinite-recursed on cycles. Fixed: added `dgmoduleset_t` DFS path tracker in `_computeMinDepth/MaxDepth`, plus pending-drain check in `generateTopology` that returns nullptr with offender list logged. Negative test at `ork.core/pyext/tests/dataflow_cycle.py` (3-node A→B→C→A cycle, completes <1s with `None` return; would hang forever before this fix). No regression on existing `dataflow.py` or particles.

### Deferred until their consumer needs them

2. **Per-plug `_required` annotation.** ~~Add `bool _required = false` to InPlugData~~. Defer to M2 validator. In orkid's dataflow model every input has a `_value` default; "required" doesn't map to anything broken until a validator actually exists to check it, and the shape of that field might want to live elsewhere (per-trait, on OpInfo, in a separate registry) once M2 is concrete.
3. **Codec-generic Python scalar setter.** ~~Replace hardcoded float/int/fvec3/fquat switch with a registry~~. Defer to M3. The hardcoded switch works for everything particles uses; new plug-value types (Image/Mesh/SDFGrid/Shaderfile) will arrive with M3 family work, and the registry design will be concrete then. Refactoring now without knowing the M3 types is speculative and one attempt already cost an hour of debugging on cross-DSO type_info / dispatch-table issues.
4. **Fanout enforcement.** ~~Add cap > 0 check in `safeConnect`~~. Defer until something actually overcommits a fanout. The `max_fanout = 0 = unlimited` convention is already established by existing traits; enforcement only matters if a future graph violates it.
5. **Real type check in `canConnect`.** ~~Add `effectiveOutputTypeId()` virtual~~. Defer until M3 needs cross-family connections that actually require type checking. Today's `canConnect` returns true and nothing depends on it being strict.
6. **`MaterializationPhase` enum on `DgModuleData`.** ~~Add `_phase = LIVE` field~~. Defer to M4 hypergraph coordinator — that's the only consumer that schedules by phase. Adding the field now means committing to its shape before the scheduler exists.
7. **Optional `_graph_id` slot in `ConnectionsProperty` serialization.** ~~Add 5th field per connection~~. Defer to M4 hypergraph file format. The optionality is for forward-compat; backfilling when the hypergraph layer lands is the same diff either way.

**Net M0 outcome:** one real bug fixed (cycle detection). The hardening list was reorganized into "consumer-driven" rather than "speculative." When each downstream consumer (M2 validator, M3 families, M4 hypergraph) lands, its prerequisites get added then with concrete requirements — no premature design.

## Still-open items (compressed 2026-08-01; the review retrospectives live in git history)

The team-review and tooling-leverage findings that used to be narrated here are now either folded into SKILL.md (subprocess worker-pool validator design, multi-sink contract, introspection/`Diagnostic`/examples/`dry_run` surfaces, three runtime modes) or SHIPPED and documented in the authoritative current-state docs — the unified substrate in `UNIFIED_SUBSTRATE.md` + `ork.dox/hyper/HYPERTERRAIN.md`, and the terrain scale contract as `TerrainManifest` (`dflow/terrain/manifest.py`; NATURAL UNITS: heights are true meters, `HeightField.EXTENT_M` is the only scale attr — the vertical scale constant and the per-op erosion exaggeration plug were deleted, and erosion exaggeration is now authored via remap nodes around `erode_thermal`/`erox`/`pha`). What remains genuinely open:

- **Trace-time control flow trap** (M1): Python `if`/`for`/`while` over a `DslNode` silently misbehaves — need `dsl.where(cond, a, b)` + `DslNode.__bool__` raising `TraceTimeBranchError` in the family-neutral DSL core (the lsystem DSL solved this locally with serializable `choose`/`fork`/`when` combinators).
- **`dsl.tap(node, name=...)`** (M1): intermediate-value inspection side-channel for debugging.
- **`@dsl.fragment` sub-expression reuse** (M1): partially answered in practice by plain-Python function libraries (`ptex3d/functions.py`); a formal decorator remains unbuilt.
- **M3 codegen deferrals**: `fxv2_prelude` sampler/uniform contributions on op declarations; conditional GLSL fragments (multi-template or ternary-lowering `cond_fragment`).
- **Cycle-permitting subgraphs** (M5): audio feedback needs `allowsCycles()` or an explicit `delay()` break-cycle node; until then M0's cycle detector applies uniformly.
- **Neural ops** (bake-phase only): `neural_mesh` / `neural_albedo` / `neural_displace` / `neural_ibl` / `neural_clip` / `style_pass` — INSTALL/SCENE_LOAD phase ONLY; heavy inference is a bake step producing ordinary cached artifacts, nothing neural in the frame loop.
- **HRTF/binaural spatializer tier**: the per-voice world-space spatialization path + panner tier SHIPPED (`ork.lev2/inc/ork/lev2/aud/spatializer.h`, the hypersound ECS emitters); the HRTF tier and an `AcousticVolume` plug type (room acoustics as a graph input) are still open commitments.
- **`UserPose` plug type** (head/hands/gaze as a typed graph input) for consumer-extensible behavior predicates — the `hypersense` family proposal was folded into `behavior` extensibility; this plug is the surviving open commitment.
- **Per-family style-lint + deterministic seed-derivation policy**: StyleContext itself is designed, unbuilt (forward — see the Status table); the companion lint/seed proposals revisit with the editor/orchestration work.
- **Asset-library bridge** (`assetdb.query(kind, tag, rig)`): explicitly out of scope for HyperSyn (SKILL.md "Out of scope"); belongs to an orkcore-catalog integration plan or an application-side composition layer. Tracked here so it isn't lost.
- **Process-graph concepts** (adopted concepts-only, no implementation yet): per-node cook status surface (editor UX, M6), `@wedge` parameter sweeps (M4+). Tile-partition caching shipped in terrain/cook-cache form.

## Performance budget tracking

Agent 2's runtime estimates (cite the agent's full report for derivations):
- 10-node particle graph, 60k particles: ~0.1–0.5ms/frame per system; 1–2 systems fit in 120fps budget
- Per-entity `graphinst.compute()`: ~1–5µs amortized → realistic ceiling ~1k–5k entities with per-frame graphs before exhausting frame time
- Above ~256 entities: prefer shared graphinst + per-instance plug-value SSBOs + batched compute (existing instanced-drawable PBR pattern)
- Cold `import orkengine.{core,lev2}`: 0.3–3s (Apple silicon, warm cache vs cold) → mandates persistent worker pool, hidden by "Initializing validator..." progress bar at editor startup
- FXV2 codegen + compile: pay the shader-compile cost once per unique `(graph_hash, override_set_hash)`; cache via `DataBlockCache` (`ork.core/inc/ork/kernel/datacache.h`)

## Cross-references

- `SKILL.md` — the contract: how to author, validate, materialize; family list; DSL pattern; runtime modes
- Memory: `~/.claude/projects/-Users-michael-projects-orkid/memory/project_dataflow_three_families.md` — durable project context
- Engine-only framing rule: LLM/sandbox/application tooling stays in a separate private layer, not in orkid
- Memory: `~/.claude/projects/-Users-michael-projects-orkid/memory/feedback_ged_outdated.md` — UI direction

(The retired VR-strategy review distillate that used to sit here was compressed into "Still-open items" above on 2026-08-01 — its still-live commitments are the neural-ops, HRTF/binaural, `UserPose`, style-lint/seed-policy, and asset-library-bridge bullets. Git history preserves the full narrative.)
