# HyperSyn — Implementation Plan

This is the implementation roadmap. The contract / API surface lives in `SKILL.md`. When the user is *using* HyperSyn (authoring graphs, asking how families work, looking up materializer outputs), the skill is the source of truth. When you are *building* HyperSyn (deciding what to commit next, tracking which milestones are done, applying review findings), this plan is the source of truth.

## Status

The implementation status of each component is the source of truth; when in doubt, check the code, not this plan.

| Component | Status | Location |
|---|---|---|
| `ork::dataflow` runtime (graphs, modules, plugs, sorter) | implemented | `ork.core/{inc,src}/ork/dataflow/` |
| `ork::lev2::particle` family on dataflow | implemented | `ork.lev2/{inc,src}/ork/lev2/gfx/particle/` |
| Python imperative graph construction | implemented | `ork.core/pyext/pyext_dataflow.cpp` |
| Runtime hardening (cycle detect, fanout, type check, required, codec-generic setter, MaterializationPhase) | **planned (M0)** | `ork.core/src/dataflow/dataflow_sorter.cpp`, `graph_data.cpp`, `pyext_dataflow.cpp`, `ork.core/inc/ork/dataflow/module.h` |
| `dflow.dsl` tracing core | **planned (M1)** | `obt.project/scripts/ork/dflow/dsl/` |
| `particles` DSL vocab (test bed) | **planned (M1)** | `obt.project/scripts/ork/dflow/particles/` |
| `dflow.validate` subprocess harness | **planned (M2)** | `obt.project/scripts/ork/dflow/validate/`, zmq worker pool |
| `ptex2d` family + FXV2 codegen | **planned (M3)** | `ork.lev2/{inc,src}/ork/lev2/gfx/ptex2d/`, `obt.project/scripts/ork/dflow/ptex2d/` |
| `ptex3d` family + PBRMaterial codegen | **planned (M3)** | `ork.lev2/{inc,src}/ork/lev2/gfx/ptex3d/`, `obt.project/scripts/ork/dflow/ptex3d/` |
| `hypermesh` family + mesh/SDF ops | **planned (M3)** | `ork.lev2/{inc,src}/ork/lev2/gfx/hypermesh/`, `obt.project/scripts/ork/dflow/hypermesh/` |
| `terrain` family + heightfield bake | **planned (M3)** | `ork.lev2/{inc,src}/ork/lev2/gfx/terrain/`, `obt.project/scripts/ork/dflow/terrain/` |
| Hypermesh procedural rigging + skinning extension | **planned (M3.5)** | extends `ork.lev2/.../hypermesh/` — `bone`/`bone_chain`/`skeleton`/`auto_skin`/`skinned` ops; output `xgmmodel_ptr_t` with skinning bound |
| Hypergraph coordinator + timeline | **future (M4)** | TBD |
| `hyperprim` family (parametric props) | **forward** | catalog layer over hypermesh; `xgmmodel_ptr_t` + named PrimSlot dict |
| `hyperarch` family (CGA-grammar architecture) | **forward** | grammar-rule mesh+slot generation; composes ptex3d for facades, hyperprim for openings |
| `hypercity` family (urban layout) | **forward** | street graph → parcels → per-parcel hyperarch; tile-partition for streaming |
| `hyperanim` family (animation behavior) | **forward** | wraps `XgmAnim`/`XgmAnimInst`/`XgmBlendPoseInfo`; FSM + blend tree + IK overlay; procedural clip synthesis primary path |
| `hyperlight` family (lighting/IBL/fog/sky) | **forward** | composes with terrain + hypercity + style context |
| `hypershot` family (per-shot camera authoring) | **forward** | cameras + dolly + focus pull; per-shot only — sequencing belongs to future `sequence` family |
| `behavior` family (stateful FSM authoring) | **forward** | first non-dataflow HyperSyn family; wraps `[[orkcore-fsm]]`; drives pose graphs / particles / materials via hypergraph composition |
| `StyleContext` sub-system | **forward** | cross-family palette/era/fog context; deterministic hash feeds materializer cache |
| `singularity` / `sequence` DSL families | **future (M5)** | TBD |
| Node-graph editor canvas | **future (M6)** | builds on `lev2.ui.PrimCanvas`; uses `DgModuleData::mgvpos` |
| ECS components (`HyperSynGraphComponent` per family) | **future** | depends on M3+ + cache layer |

**Status legend**: *implemented* = exists in code; *planned* = on the M0–M3 roadmap with committed API contract; *forward* = designed with rough spec in SKILL.md, API surface stable enough for hypergraph/ECS planning, no implementation timeline; *future* = vague pointer at the right area, no API committed.

Editor UI uses the current `lev2.ui.PropertySheet` + `ReflectionPropertySheetModel` + composite-widget pattern (see `ork.editor.ecsedit`, `ork.hdri.studio`, `ork.lev2/pyext/tests/ui/widget_pack.py`). Do not extend the legacy `ged` system.

## Milestone roadmap

| Milestone | Goal | Gating prerequisite |
|---|---|---|
| **M0** | Runtime hardening — make `ork::dataflow` safe for DSL-authored and LLM-authored graphs | — (start here) |
| **M1** | `dflow.dsl` tracing core + particles family DSL — proves the trace→graphdata round-trip against the existing particles runtime | M0 |
| **M2** | `dflow.validate` subprocess harness — zmq REQ/REP worker pool, persistent across editor session and into live-mutable runtimes | M1 |
| **M3** | Four new families (ptex2d, ptex3d, hypermesh, terrain) — runtime + DSL vocab + materializer for each; uses the codegen + bake patterns | M0, M1, M2 |
| **M3.5** | Hypermesh rigging + skinning extension — procedural skeleton/weight ops yielding `xgmmodel_ptr_t` with skinning bound; prerequisite for hyperanim | M3 |
| **M4** | Hypergraph coordinator + timeline — cross-family graph composition + phased materialization scheduling | M3 |
| **M5** | Singularity + sequence DSL families — audio-reactive cross-family use cases | M3, M4 (and needs cycle-permitting subgraphs for audio feedback) |
| **M6** | Node-graph editor canvas — `DgGraphCanvas` widget on `PrimCanvas` consuming `DgModuleData::mgvpos`; round-trips with file format | M3 (and benefits from validator harness from M2) |
| **Forward** | hyperprim, hyperarch, hypercity, hyperanim, hyperlight, hypershot, behavior, StyleContext — each rough-specced in SKILL.md; implementation order TBD when M3 stabilizes. `behavior` is the first **stateful** family (FSM runtime, not dataflow); see "Family kinds" in SKILL.md | M3 (hyperanim additionally needs M3.5; behavior needs the validator harness from M2 + FSM-specific check rules) |

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

## Team review findings — corrections folded and items deferred

A team of agents reviewed the spec (see ~/projects/orkid/.claude/projects/-Users-michael-projects-orkid/memory/project_dataflow_three_families.md for higher-level context). Key corrections and deferred items:

### Folded into SKILL.md
- **Subprocess strategy**: not spawn-per-call — long-lived zmq REQ/REP worker pool, pre-warmed at editor start. Single largest UX risk if mis-implemented.
- **ptex3d FXV2 codegen path confirmed viable** via embedded `FreestyleMaterial` (Agent 2's initial "this won't work" was overstated — `material_pbr.cpp:147` delegates to freestyle which supports `gpuInitFromShaderText`). Spec's FXV2 stitching approach stands; small wrapper may be needed to expose `gpuInitFromShaderText` directly on PBRMaterial.
- **Per-entity vs shared graphinst is a use-site decision**, not a spec mandate. Materializer caching by graph-hash makes sharing automatic when graphs/overrides match.
- **`max_fanout = 0 = unlimited` is the existing convention** (not ambiguous as one agent claimed); just add the contract comment and the `cap > 0` check.
- **Terrain bakes heightfield as materializer side-output** (CPU-accessible + bullet-physics-feedable); no per-op DSL dual-emission required.
- **Three runtime modes** (authoring / runtime-static / runtime-live-mutable), not two — validator subprocess persists into live-mutable runtimes for experiences that allow content modification while playing.

### Folded from HYPERVBOX leverage review (impcore/vibe_sandbox compatibility)
- **Op registry introspection API** — `ork.dflow.dsl.registry.list_families/list_ops/list_examples/get_op` + `OpInfo`/`PlugSpec`/`ExampleInfo` dataclasses + `op.to_json_schema()` classmethod.
- **Structured `Diagnostic` dataclass** — replaces raw tuples in `ValidationResult.errors/warnings`; stable `code` field, optional `hint` + `source`.
- **Examples per family** at `ork.lev2/pyext/tests/hypersyn/<family>/` — runnable visual apps, indexed by `registry.list_examples(family)`, double as CI integration tests; min counts per family.
- **Source provenance** — `@op` records `(file, line)` of decorated class; `DslNode` records `(file, line)` of call site via `sys._getframe`; both feed `Diagnostic.source` and materializer GLSL `// <file>:<line>` comments.
- **Multi-output ops** — `outputs` dict + `build()` may return `dict[str, DslNode]`; callers receive `DslNodeBundle` with attribute access (`n.value`, `n.gradient`).
- **`dsl.dry_run(graph)` in-process variant** — same `ValidationResult` shape as `validate()`, no subprocess, no crash isolation; for CI lint + batch authoring + headless callers.

### Deferred to M1 (DSL design)
- **Trace-time control flow trap**: Python `if`/`for`/`while` over a `DslNode` silently misbehaves. Need `dsl.where(cond, a, b)` op and `DslNode.__bool__` raises `TraceTimeBranchError`. JAX-style fix.
- **Sub-DSL function reuse**: `@dsl.fragment` decorator for shareable sub-expressions. Critical for non-trivial shaders (200+ line PBR with anisotropy/clearcoat/multilayer).
- **`dsl.tap(node, name=...)` op** for intermediate-value inspection (debug-mode side-channel writes to RT for codegen families, or log for execution families). Source-line provenance and GLSL comments are now in SKILL.md (folded above); `dsl.tap` is the runtime-side debuggability complement.

### Deferred to M3 (per-family codegen)
- **Sampler/texture plug-type prelude**: `fxv2_prelude` channel on op declarations for `sampler_set` / uniform block contributions. Without this, texture-sampling ops can't be expressed.
- **Conditional GLSL fragments**: either multi-template ops keyed by static flags OR `cond_fragment(expr, then, else)` op lowering to GLSL ternary. Pick at M3.
- **PBRMaterial libblock substitution decision**: SKILL.md currently says "stitch fragments → free-form FXV2 → gpuInitFromShaderText". An alternative is to substitute into a fixed PBR template's `ps_user_surface(out vec3 albedo, ...)` libblock and reuse existing 23-technique PBR infrastructure. Decide at M3 implementation time; spec accommodates either.

### Deferred to M5 (audio/sequence)
- **Cycle-permitting subgraphs**: audio feedback (delays, reverbs) requires either `DgModuleData::allowsCycles()` virtual returning true on Singularity subgraphs, OR Faust-style explicit `delay()` break-cycle node. Pick when M5 starts. Until then, M0's cycle detector applies uniformly.

### Borrowed from PDG (concepts only; no IP)
- **Per-node cook status** (`unscheduled / waiting / scheduled / cooking / cooked / error`) — `ValidationResult` extended to per-module. Editor UX win at M6.
- **Wedge** (parameter sweep producing N variants per work item) — `@wedge(seed=range(16))` decorator at M4+.
- **Tile partition** (group work items by spatial attribute, materialize per tile, cache per tile) — terrain at world scale.
- Not adopting: file-system-as-IPC, USD coupling, network scheduler protocols (HQueue/Tractor/Deadline).

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
- Memory: `~/.claude/projects/-Users-michael-projects-orkid/memory/feedback_no_vibe_in_orkid.md` — engine-only framing rule
- Memory: `~/.claude/projects/-Users-michael-projects-orkid/memory/feedback_ged_outdated.md` — UI direction
