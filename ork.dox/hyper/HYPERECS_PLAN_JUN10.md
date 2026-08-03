# HYPERECS — Forward Plan & Locked Contracts

**Architecture, design, and implementation status live in `HYPERECS.md`** (same dir). This doc now
tracks only **open (unimplemented) work** plus the durable **locked contracts** (Appendix A).

The original ordered execution plan (Stages A–E, authored 2026-06-10; architecture-first, no-regressions
per step) has **LANDED**. Its detailed per-item landing log is preserved in git history and distilled into
`HYPERECS.md`'s implementation-status section; it is intentionally no longer restated here.

---

## Status (as of 2026-07-04)

Stages A–E of the original ordered plan are implemented:

- **A — the ratchet:** full hypermesh op-state reflection, `save`/`load` + byte-identical round-trip gate,
  the fail-loud plug-layout/connection skew gates, and the determinism + CPU-vs-GPU equivalence oracles.
- **B — the locked contracts:** typed `canConnect` (typeid-unified, WARN-then-enforce), `GraphInst` env
  unification (`TypeKeyedVars` — mixed-family graphs), the family-neutral `interchange.h`, and the
  param/animation/clock contract (EXPRP plugs, declarative `S.time`, incremental pausable clock).
- **C — the GPU-native engine core:** dirty-gated per-frame live path, parallel MeshScan (~97×),
  `dispatchComputeIndirect`, the subdivide/extrude/inset GPU topology ports (each equivalence-gated), and
  the flagged non-blocking submit.
- **D — the C++ host spine:** the `PbrMaterial`/`HeightField`/`Hypermesh` gendata materializers +
  `AssetSystem::materializeAll` wire step + `HypermeshComponent`, through the pure-C++ HYPERECS host demo.
- **E — the cross-family features:** DisplaceByField, typed InstanceSet edges, gid bucketing + multi-material
  draw, per-view GPU cull + instanced depth-prepass, the **SDF family** (M0–M2 + M4: dense brick,
  `MeshToSdf` voxelize, `Csg` + marching-tetrahedra `SdfToMesh` incl. fully-GPU live counts + weld,
  `Redistance` JFA, `DisplaceBySdf` conform/offset/scalar, cook-hash — only M3/NanoVDB remains), and
  the **L-system structural-spine slice + flora render wave** (see item 4 / A7).

Post-2026-06-30 substrate campaign (LOADX, 07-01→07-04 — serves A3 #6 discrete-GPU-first): warm
scn_forest settles in 2.7 s; bake arena 52.5 GB → ~1.5 GB (frontier pool + zero-reader release);
cold eflow 30 min → ~9 min on the 3090 and dropping; strategic cook cache-points (per-fork
resume; forest blobs 34 GB → ~4 GB); blob fp16 wave; mac↔linux bit-parity; exit-crash class fixed.

See `HYPERECS.md` for what each of these provides at the authoring surface.

---

## Open work — what is NOT yet implemented

1. **64-bit `MeshEdges` key widening** — **DROPPED (owner, 2026-07-04)**. The fail-loud 65,536-vert gate +
   wholesale CPU-build fallback (`hmdflow_module_subdivide.cpp:683`, `hmdflow.h:109`) are the accepted
   steady state. Revisit only if a real asset hits the gate in a hot path.

2. **C.5 non-blocking submit — default-flip + N-buffering.** `ORK_HM_NB_SUBMIT=1` enables the depth-1
   overlap; **blocking is still the default** pending soak (`vulkan_compute.cpp:319`). N-buffered param
   buffers + phase-coalescing were deliberately skipped at depth-1 (future if profiling demands).

3. **SDF family — only M3 remains.** *(Corrected 2026-07-04: an earlier revision of this doc listed M4 as
   open — M4 in fact SHIPPED 2026-06-13: M4a `Redistance` (JFA eikonal), M4b `DisplaceBySdf`
   conform/offset/scalar (the cross-family edge, + Taubin fairing + `TemporalSmooth`), M4c
   `SdfComputeInst::cookComputeHash`.)* **M3 = the NanoVDB sparse read/transport blob** — DECLARED but
   UNBUILT (no `nanovdb` code in `ork.lev2/src/gfx/sdf/`; sources vendored at
   `<staging>/builds/openvdb/nanovdb` but headers NOT installed — obt dep change is step 0). Remaining
   post-M3 cross-family/cook surface (brick cook-store, particle GPU collider, ptex3d distance signal):
   **implementation spec `~/JUL04_SDFM3M4.md`.**

4. **Structural spine — advanced milestones.** The **foundation is LANDED** (see A7): G0a the `XfNodeGraph`
   interchange currency, G0b the `LSweep` skinner, G1 the L-system family (v2: four hardcoded archetype
   generators + tropism/jitter channels + parallel-transport frames + CPU sweep). **Also landed (the
   2026-06-26 flora wave, previously unrecorded here): `MergeMesh` (`hmdflow_module_merge.cpp` — the
   join module an earlier revision listed as the missing G3 prerequisite), `LeafScatter` organs, the
   procedural A2C leaf material, VS wind (`VertexDisplace`/`Wind`/`LeafFlutter`, per-instance wind
   groups, `RCFD_TIME` provider clock), per-gid asset materials, E.4 cull on the viewer path, the
   4096-tree @120fps instanced forest, and `scn_forest` (2 species × 8 seeds, terrain-scatter
   instancing, impostor LOD).** Still open (updated 2026-08-01: reflected-`LRuleSet`
   grammar generalization LANDED — see A7; XfSlot/`instance_at_slots` LANDED — grammar SLOT ops emit
   since GR-1, LeafScatter `source=SLOTS` consumes them, c4dbd0031 2026-07-27): GPU L-system
   rewrite; `radial_repeat`;
   `LegChain`/creature vocab; city/building grammar (merge prerequisite now met — buildable via the
   imperative DSL today); `auto_skin` + a GPU-skinning render path (recon 2026-07-04: nothing
   transfers — `FWD_SSBO_CUSTOM*` has no bone channels; only `skintools.i2` math is inheritable;
   `XgmSkeleton` pyext is read-only); straight-skeleton; space-colonization. Full design in
   `UNIFIED_SUBSTRATE.md` §11–§17.
   **SEQUENCING RESOLVED (owner, 2026-07-04): the racer-first gate is retired as a sequencing constraint.**
   Sequencing now optimizes the HOLISTIC outcome: rich, natural, beautiful scenes at VR performance with
   the minimum procedural-DSL code, where the DSL reads clearly. (The racer remains a valid gate ASSET for
   hard-surface/gid acceptance whenever that capability class is touched — it just no longer orders the
   roadmap.) Implementation specs: `~/JUL04_SDFM3M4.md` + `~/JUL04_GRAMMARS.md` (authored 2026-07-04).

5. **Minor / deferred.**
   - **Plug-transformer ALWAYS-APPLY** behavior + suite soak (B.5(e)). The `_transformer` plug is reflected
     (`plug_data.h:273`); the behavioral change (transform whether authored or connected) is the open part.
   - **Dirty-machinery lift into dflow core** (B.5(d) — direction locked; hypermesh has its `_version`/
     `_topoVersion` gate, the general core lift is still partial).
   - **A4 data-model debts:** per-semantic channel stride (everything vec4/stride-16), the hardcoded
     `MeshChannel` enum (~45 call sites — blocks carry-unknown-channels-forward), face-attr scatter through
     topology changes.
   - **Owner-owed verification passes:** live-viewer `S.time` animation under the C++ clock; the C.5
     flag-on viewer-feel soak.

---

# Appendix A — Locked contracts & standing commitments (distilled from the retired docs)

*These are binding design commitments captured in `hypermesh_design_review.md` (Jun 8) and the May
`HYPERECS_PLAN.md` before those docs were retired. They are NOT superseded. The Tier-3 Scene-composite
DESIGN + implementation status live in `HYPERECS.md` (same dir).*

## A1. The tag-space contract (LOCKED) — implemented (B.5(a)), consumed by gid bucketing (E.3)

`__tags` (32 bits per face) = `[0:20)` **free-form** (operator working space; optional 5-bit-bank tooling, bank 0 = volatile op-scratch by convention) · `[20:32)` **persistent gid, LOCKED** (12-bit int 0..4095, single-membership durable identity — the material/semantic-class key; likely subsumes the `material_id` face channel).

- `gid = (tags >> 20) & 0xFFFu` · set: `tags = (tags & 0x000FFFFFu) | (g << 20)` · gid mask `0xFFF00000u`, free mask `0x000FFFFFu`.
- Implications: (1) `MaskOp`/`FaceTagger` writes are MASKED to `0x000FFFFF` so build ops can never clobber the gid (and it never moves); (2) two distinct write verbs — bitfield `MaskOp` (OR/AND/XOR, free region only) vs **`assign_gid(n)`** (int-set into the top 12) — plus the read atom `S.gid`; (3) ONLY the gid is locked; tactical+user bits are one uniform free region (the 5-bit bank is organizational tooling, not a contract).
- Per-face gid = material/semantic CLASS; per-INSTANCE identity/variation rides the instance-attr buffer — don't conflate.

## A2. The serialization principle (owner) — implemented (A.1), asserted by the round-trip gate (A.2)

Serialize **precisely the state that changes what a GraphInst computes** — test every field by that. IN: connections (GraphData edges) · plug AUTHORED values (unconnected-input defaults) · plug input TRANSFORMERS (per-plug scale/bias/curve — easy to forget, silently changes results; the round-trip gate must assert these) · module member params (matrices/GLSL strings/masks/vectors). OUT: plug *structure* (name/type/dir — rebuilt by `reshapeIOs`), transient runtime state (computed values, RNG), imperative `onUpdate`. Reflection method is per-field: `directProperty` for POD, `lambdaProperty` for computed/string/validated (terrain `ExprModule.shadertext` is the string precedent). Trap: any new plug type must keep plug creation idempotent (reshapeIOs runs in createShared AND postDeserialize).

**Animation tiers:** (a) live Python `onUpdate` = app/authoring convenience, NOT serialized; (b) **declarative graph nodes** (Time/oscillator/op modules into param plugs) = the portable asset form; (c) external driving (game/physics) = the HOST pushes plugs. The C++ host's only per-frame job is feeding external inputs — chiefly the clock.

## A3. The ratified decision register (#1–#7 — closed; rationale preserved)

1. **Determinism:** deterministic STORED output is free — the renderer's contiguous CSR forces the ordered scan; raw-buffer compare is therefore VALID for all gates (no canonicalization); same-machine scope, cross-GPU bit-identicality a non-goal. Constrains the Pivot-1 ports to scan-scatter (not atomic-append) for stored output.
2. **City-scale architecture = the hierarchical cross-family hypergraph:** not one giant graph nor thousands of unique ones — a cross-family PIPELINE (`terrain → citygen → hypermesh`), a catalog of TYPES linked by two edge kinds: instance-source (scatter buffer → instancing) and field-input (heightfield → displace/conform). Two interchange flavors: expression-IR (the proven terrain↔ptex3d SurfNode spine) + **GPU-resource handles** (heightfield/ScatterSet/mesh/SDF/`XfNodeGraph` — a mesh is a resource, not an expression). Prove two real links before generalizing (E.1/E.2, both landed).
3. **Portable form = the REAL `dflow::graphdata` serialized via reflection** — persist the trace OUTPUT (GLSL strings, baked matrices, flat vectors), never the recipe (the particles dsl_file/re-run-Python model is the named anti-pattern). Stable names survive round-trip; rebind-by-name is the host's driving mechanism.
4. **Booleans = the SDF domain** (`poly→SDF→CSG min/max→poly`), not polygon CSG. Requires the clean/watertight/manifold poly invariant → the triangulation floor + winding + manifold-weld are PREREQUISITES of the SDF bridge. Sharp edges = feature-preserving extraction (dual-contour w/ Hermite), deferrable. *(SDF family M0–M2 landed this bridge — dense brick + CSG + marching-tetrahedra extraction.)*
5. **Acceptance gate = RACER FIRST** — hard-surface shaping + gid multi-material ahead of generic cross-domain; the racer is the persisted gate asset.
6. **Shipped runtime: discrete GPU first-class** — DEVICE_LOCAL residency shipped; every readback is a PCIe round-trip on discrete, so no-readback work is discrete-critical.
7. **Interactivity: undo is a NON-GOAL** (procedural — the .py/graph is the source of truth; history = version control). Interactive authoring = watch-mode hot-reload + runtime param plugs; cook-cache incremental re-eval is an optional large-graph latency optimization, deferrable.

## A4. Data-model debts (still open — quantified once, don't lose)

- **Per-semantic channel stride:** every channel is forced vec4/stride-16 (UV wastes 50%, scalars 75%; ~96+ B/vert before topology) — a *prerequisite* of any city-scale memory claim.
- **Named vertex-channel map:** the fixed `MeshChannel` enum is hardcoded at ~45 call sites — violates the carry-unknown-channels-forward constraint; the channel-table convention (B.5(b)) for the ports is the interim bridge, not the fix.
- **Face-attr scatter through topology changes:** ops that alias `out->_faces = in->_faces` across count/order changes desync `material_id`/`__tags` (latent correctness bug).

## A5. Acceptance shapes carried from the May milestone plan (M→Stage mapping)

- M0 *Hello-Scene* (a cube renders through the ECS scene composite) → the precursor shape for **D.5**'s pure-C++ host demo (landed).
- M3 *JSON round-trip* (serialize→reload→identical) → the headless round-trip gate (A.2) + the live keystroke gate (D.5) — both landed.
- M4 *fragment composition* (multi-fragment render identical to single-fragment) → Stage E / Tier-3 work.
- *Sugar↔primitive equivalence harness* (the same scene authored via sugar and via raw primitives must lower identically — divergence is a bug) → ships WITH the Tier-3 scene composite.
- Risk register highlights still live: sugar/primitive drift · two-pass lowering order · validator-subprocess library drift.

## A6. Standing Tier-3 design commitments (the live design + status = `HYPERECS.md`)

Still-operative commitments not restated elsewhere: the **three-tier authoring model** (Tier 1 standalone and Tier 3 scene composite coexist PERMANENTLY — Tier 3 never replaces Tier 1) · **stage-then-swap live mutation** (session.stage/revert + auto-revert ring; REALITY CHECK: the swap is 2–3 frames, not the spec'd single frame — barrier-protected, no tearing) · the **time namespace** (`Expr.sim.{time,dt}` / `Expr.entity.{spawn_t,age,pos}` / `Expr.slot.{t,dt}` / `Expr.ptc.{unit_age,random}`) — B.4's clock work landed names compatible with this · **fragment composition** (pass-`self`-to-helper as the composition primitive) · **string-keyed reflection lookup** in every type-resolution path (zero orkid-internal Python imports) · the **phase model** (INSTALL/STARTUP/SCENE_LOAD/PRELOAD/LIVE) is respected by scene-level materialization.

## A7. Structural spine — XfNodeGraph currency + grammar engine (foundation LANDED; grammar generalization forward)

*Originally scoped 2026-06-24 as unbuilt; the foundation landed 2026-06-25.* The `interchange.h`
GPU-resource-handle set (A3 #2's second flavor) grew its **fourth handle**, the family-neutral
**`XfNodeGraph`** (transform-graph currency) — `mat4[count]` + parent buffer + typed `attrs vec4[count]` +
`tags uint[count]` + `_version`/`_topoVersion`, structurally `InstanceSet`+edges but `GpuMesh`-shaped in its
dirty/edge semantics. It carries the structural / topology spine (flora / cities / creatures as thin
specializations) the way `GpuComputeImage2D`/`InstanceSet`/`SdfGrid` carry the expression + scatter + SDF
edges. The full design (one rewrite/grammar engine on the bake-once side of boundary 5; `sweep`/`auto_skin`/
`field_sample`/`mirror`→`XfNodeGraph` shared ops; the `XfNodeGraph`→`XgmSkeleton` adapter linchpin; the
forests-vs-racer sequencing call) lives in `.claude/skills/hypersyn/UNIFIED_SUBSTRATE.md` §11–§17.

**Status (what LANDED vs what is forward):**
- **G0a — `XfNodeGraph` interchange plug: LANDED.** `ork::hyper::{XfNode,XfSlot,XfNodeGraphData,XfNodeGraphInst}`
  in `ork.lev2/inc/ork/lev2/gfx/dflow/interchange.h` (GPU SoA SSBOs + CPU mirror + dual-version dirty), with
  `XfNodeGraphPlugTraits` in `ork::lev2::dflowgfx`, registered `"hm_xfng"` in BOTH `hmdflow` dgctx blocks
  (`hmdflow.cpp:499,619`). Uses A1 `__tags` `[20:32)` gid band and the A2 serialize-the-output rule
  (the emitted graph / rule descriptor serializes, never the Python generator) verbatim.
- **G0b — `LSweep` skinner: LANDED** (`hmdflow_module_lsweep.cpp` — `XfNodeGraph` → swept-tube `GpuMesh`).
- **G1 — the L-system family (M1): LANDED** (`hmdflow_module_lsystem.cpp` producer + `leafscatter.cpp` organ
  placer; Python `lsystem()`/`lsweep()` verbs; `scn_lsystem.py`). v1 = a hardcoded bracketed parametric
  grammar run CPU-side at activate; reflected-`LRuleSet` + GPU rewrite is the next step.
- **LANDED (2026-07-09 → 2026-07-27) — the reflected-`LRuleSet` grammar generalization** (tree labels
  **GR-1/GR-2/GR-B**; full record in `HYPERECS.md` §6 cross-ref): **GR-1** reflected grammar core —
  `LRuleSet` schema + Python DSL 2026-07-09, stock species presets 2026-07-19 (legacy C++ archetype path
  deleted; "species = data"), family-neutral `ork::grammar` core extraction (`ork.core/inc/ork/grammar/`)
  2026-07-27; **GR-2** slot consumption 2026-07-27 (c4dbd0031 — LeafScatter `source=SLOTS` /
  `instance_at_slots`, off-by-default gated); **GR-B** grammar-building library (parameterized building
  generators composed from stock hypermesh verbs) + `scn_hamlet` exemplar 2026-07-19 (41b0bbc53).
- **Forward (UNBUILT):** GPU L-system rewrite; `radial_repeat`; `LegChain`/creature vocab; city-scale
  street/lot grammar beyond the GR-B building library; `auto_skin` +
  `bake_skeleton` + a NEW GPU-skinning render path; straight-skeleton; space-colonization / phyllotaxis /
  tensor-field generators. `GidAssign` (A1, E.3) and `MirrorModule` are mesh-locked (`MeshComputeInst` over
  GpuMesh SoA) — promoting them to operate on any tagged `XfNodeGraph` is post-foundation work.
- Rule predicates are trace-time Python over abstract symbols — they DO NOT serialize as the
  `kPredicateABIVersion` GLSL SelExpr ABI (that ABI is a GPU per-mesh-element predicate; using it for
  production rules is a category error).
- The feasibility-adjusted build order (G0a/G0b/G1 done; G2a creature-vocab; G3 building — its
  mesh-merge prerequisite now landed) is in `UNIFIED_SUBSTRATE.md` §16. **Sequencing resolved by the
  owner 2026-07-04 (holistic-outcome ordering — see item 4); the concrete milestone order lives in
  `~/JUL04_GRAMMARS.md`.**

## A8. Shader-recompile minimization (owner, LOCKED 2026-07-04)

All compute/shader-generating DSL work must distinguish **PARAMETRIC** from **STRUCTURAL** state:

- **PARAMETRIC** — any value a user might tweak from their DSL, *especially* anything they may want
  to tweak in realtime — MUST ride in a UBO / SSBO / runtime plug and bind at runtime. It must NOT
  be inlined as a constant into generated fxv2/GLSL text. Tweak = `bindParam` rebind (the 2.12
  rebind contract makes these live), never a recompile.
- **STRUCTURAL** — things that genuinely change the code's shape (loop bounds that size unrollings,
  topology/plug wiring, expression *structure*, array dimensioning) MAY bake, and version-salt the
  cook/shader identity when they do.

Precedents that already comply (the templates to copy): the VS-wind `WindDir`/`WindParams` uniforms
(value-independent shader text — owner-required), terrain runtime-params SSBO (scales in the SSBO,
only bucket loop bounds bake), ptex3d `ctx.param` tweakables, `SdfEval`'s dim/extent/center/offset
runtime plugs, the `EXPRP` plug channel (`kMaxExprParams`), and named fx-pipeline providers
(`RCFD_TIME`) for engine-fed per-frame values. Secondary benefit: value-independent generated text
maximizes `dslshadercache` hits across assets differing only in parameters (the cache hashes
expanded shader text). Applies to ALL families — hypermesh, sdf, ptex3d, terrain, grammars — and to
both render (fxv2) and compute (shadlang) codegen.
