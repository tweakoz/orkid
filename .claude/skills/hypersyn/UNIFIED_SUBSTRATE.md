# HyperSyn Unified Procedural Substrate — Feasibility & Design

> **Scope note (2026-06-24).** This doc unifies the *expression IR* (terrain ⇄ ptex3d: one `SurfNode`
> tree → fragment GLSL + compute bake). The companion *structural / topology* spine — a shared
> transform-graph currency (`XfNodeGraph`), the one rewrite/grammar engine, the cross-cutting ops that
> let flora / cities / creatures be thin specializations — is specified in §11–§16 below. The two are
> orthogonal: the expression-IR work (§1–§10) is LANDED through Phase 2; the structural spine's
> **foundation LANDED 2026-06-25** — the `XfNodeGraph` interchange currency (G0a), the `sweep`/`LSweep`
> skinner (G0b), and the L-system generator vertical slice (G1, M1). The **advanced spine** (§12–§16 grammar
> generalization + creature/city/skinning milestones) remains **design-accepted but unbuilt**. Keep the two
> tracks distinct when planning.

> One authored procedural expression → fragment GLSL (shading) + compute bake (heightfield
> displacement / feature channels). Source: a 9-agent feasibility workflow + follow-on design.
> Status: design accepted, phased. C++ phases need `ork.build.py`.

## 1. Verdict

**Feasible — the plumbing is a slam-dunk; correctness rides on a few prerequisites, now resolved/planned.**

Slam-dunks:
- The ptex3d IR is a clean backend-agnostic typed DAG; `_Emitter` already supports atom-rebinding via `subst` (`dsl.py:646`) — the exact compute hook. `emit_height` already uses it (`subst={'opos':'coord'}`); a compute lowering is a **4th sibling** of the existing emitters.
- A generic `ExprModule` already exists *in spirit*: `erox`'s `_erox_text` (`erox.cpp:34-54`) wraps an arbitrary GLSL body + injected SSBO decls + a params SSBO into the compute shell and disk-caches it. Generalizing it is extraction, not new infra.
- `CaptureModule` is already the generic sink (`base.py:74`) → `hfbake`/`hfmask` need **no** new sink machinery. The just-built scale contract (`TerrainManifest`, physical `height_m`, texel-center) is the coordinate/units bridge.

Hard / gated:
- **Screen derivatives** (`fwidth`/`dFdx`/`fbm_aa`) are impossible in compute and are the spine of hmview's AA → a **portable-core** gate (hard-reject at trace time) + a portable `ctx.footprint` substitute.
- **Noise divergence** and **capture auto-exposure** are silent corruptors → §4 and §6.

## 2. Shared IR & dual lowering

One `SurfNode` tree, multiple lowering entry points (all already exist): `emit_surface` (`dsl.py:696`, fragment `o.<field>=`), `emit_height` (`:713`, `float ptex_height(vec3 coord,…)`), `emit_cellular` (`:726`). The compute backend is a 4th sibling:

```python
# dsl.py — sibling of emit_height, ZERO IR changes
def emit_compute_field(node, *, coord_expr):
    em = _Emitter(subst={"opos": coord_expr, "wpos": coord_expr})
    final = _coerce(em.expr(node), node._type, "float")   # bake sink = scalar
    return em.lines, final, _emitter_deps(em)              # lines are fragment-free GLSL
```

All fragment coupling lives in the `fxv2_template.py` shell (`:316-468`), **not** in the Op nodes. New code = the wrapper + a `compute_template.py` (std430 SSBO shell + coord prologue) + a C++ `ExprModule` generalizing `_erox_text`.

**Refactor surface:** `dsl.py` (small: `emit_compute_field` + portability visitor), `compute_template.py` (new, small), `terrain/base.py` + `ops.py` (small: sinks + `expr_field`), `hfdflow_module_expr.cpp` (new, **medium, C++**), `pyext_gfx_terrain.cpp` (bind, C++). **Drop CPU** — no interpreter exists, non-viable at 4096²; for offline bakes run the same compute **headlessly** → bit-identical.

## 3. The portable core — `ctx.footprint`, not rejection (DECIDED)

**Decision: anti-aliasing ops are TARGET-POLYMORPHIC, not fragment-only.** The author writes one expression with `fbm_aa`/band-limiting; it works in both targets, the AA degrading on its own in compute. The "portable core" is therefore **"everything except view-dependent atoms"** — a friendly contract, not a hard restriction.

The mechanism is a first-class **`ctx.footprint`** atom the emitter binds per target — the band-limit metric every AA op reads instead of calling `fwidth` directly:
- **Fragment:** `footprint = length(fwidth(p))` (screen-space, as today).
- **Compute / bake:** `footprint = extent_m/dim` (the texel size — a constant; the emitter substitutes it, so `fwidth`/`dFdx` never reach the compute backend). This is the *correct* degrade, **not a NOP**: octaves finer than a texel can't be stored in the baked field and would alias into the geometry, so `fbm_aa` must fade them — same intent as on screen, keyed to the grid. `footprint = 0` is the explicit "bake everything raw" escape.

So `fbm`/`fbm_aa` route through `ctx.footprint` → **portable, never rejected** (see §4: collapse to one footprint-aware `fbm`). `aa_ramp` on a *position-derived* mask likewise ports via `footprint`.

**ACCEPT (the whole vocabulary):** `vec2/3/4`, `sin cos atan atan2 abs floor fract sqrt normalize saturate mix clamp step smoothstep pow mod min max dot length cross`, `noise fbm fbm_aa` (footprint-aware, §4), `voronoi/hexgrid/spherecells` (vecN forms), `func`, `ctx.footprint`. Atoms: `P_object`(opos), `NO`(onrm), `uv`, `Const`, `Param`; `P`/`N`/`NW` (world) once the bake synthesizes them from uv (§5).

**REJECT at trace time, with call-site (the short list):**
- **View-dependent atoms** — `NV`(vnrm), `eye`, `Cd` (no camera / no vertex attrs on a height grid).
- **Bare `fwidth(x)` of an arbitrary, non-footprint derived value** `x` (an analytic edge on a *computed* field) — has no constant analog; would need a grid finite-difference (a neighborhood op, a different mechanism — possible later, out of scope now). Note this is *not* the `fbm_aa`/footprint case, which ports.
- voronoi `.fwedge` (divides by per-fragment `onrm`) — feed a synthetic up-normal `(0,1,0)` or reject.

Add `sys._getframe(1)` provenance to `_Ops` (ptex3d nodes carry none today) so the error fires at the user's line:
```
hfdisplacement(): atom 'ctx.NV' (mymat.py:42) is view-dependent — no bake form.
Bake-rejects only view-dependent atoms (NV, eye, Cd).  fbm_aa/footprint port fine.
```

## 4. Noise — ONE canonical basis (DECIDED)

**Decision: the canonical noise is the ptex3d 3D value-noise `lib_mmnoise::noise(vec3)` (`misctools.i2:66`), used across rasterization AND compute.** hmview already uses it; keeping it means materials don't move and the dual-target expression matches by construction.

The `sampler3D` scare is a **red herring**: that's `octavenoise(sampler3D,…)` (`misctools.i2:11`), a *separate* texture function `P.fbm` does **not** call. `noise(vec3)` (`:66`) is pure ALU (`floor`/`fract`/`dot`/`mix`/`sin`-hash). The two basins genuinely differ today only by **dimension + hash + packaging** — both are smoothstep value noise; the divergence is accidental (terrain inlines a 2D hash `fbm.cpp:41`, ptex3d uses a 3D libblock). No design reason for *two hashes* (the 2D-vs-3D dimension split is legit, but should come from one lineage).

**Plan (incremental → full):**
1. Split the pure-ALU `noise(float/vec2/vec3)` out of `lib_mmnoise` into a **sampler-free libblock** (`lib_pnoise`), leaving `octavenoise`/`sampler3D` behind. (Prereq: prove a compute module can `import`/inherit a libblock — no terrain compute module does today; `pha.cpp:117` inherits `lib_pha`, so the mechanism exists, but noise-into-compute is unproven.)
2. The new `ExprModule` emits `lib_pnoise` → `P.fbm` in an `hfbake`/`hfdisplacement` is **byte-identical** to the fragment. The legacy `T.Fbm`/`FbmModule` keeps its own hash for now (terrain-only generator, never dual-targeted) → **no re-tune of `xxx.py`**.
3. Later (when ready to re-tune `xxx.py`): migrate `FbmModule` to `lib_pnoise` → genuinely one noise in the engine.

This collapses old "FATAL #1 (noise)" + the Phase-0 libblock de-risk into **one task**: *split `lib_mmnoise` pure-ALU `noise()` into a sampler-free `lib_pnoise` and prove import-into-compute.*

**Collapse `fbm` + `fbm_aa` into ONE footprint-aware `fbm` (DECIDED).** Per §3, AA is target-polymorphic via `ctx.footprint`, so there is no reason for two functions: a single `fbm` band-limits against `ctx.footprint` (fragment → screen, compute → texel, `0` → raw). The author writes `P.fbm(...)` and it does the right thing in whatever target it's emitted to — the most seamless surface for humans *and* LLMs. The current `_ptex_fbm_aa(p, oct, aabias)` (which computes its footprint inline via `dFdx`/`dFdy`) becomes `_ptex_fbm(p, oct, aabias, footprint)` with `footprint` supplied per target; the legacy plain `_ptex_fbm` is the `footprint = 0` case. **Alignment note:** because the low octaves (the band undulation) are identical across targets and only sub-pixel/sub-texel octaves differ, a φ that uses `fbm_aa` still yields coincident benches (bake) and bands (fragment) — so the shared strata φ keeps its AA.

## 5. The new sinks + coordinate/units bridge

All thin wrappers; **the expression is the portable unit**.
- **`self.hfbake(expr, channel)`** → `ops.expr_field(expr)` builds an `ExprModule` (generator, no input SSBO) → existing `capture(node, channel)`. A data channel other shaders sample / other hf modules consume. Lowest-risk first.
- **`self.hfmask(expr, channel)`** → same, wrapped `saturate`/remap to `[0,1]`.
- **`self.hfdisplacement(expr, into=h, scale_m=…, mask=None)`** → same `ExprModule`, composed into the height `TerrainNode` *before* `capture("height")` (TerrainNode arithmetic already lowers to Combine/MaskBlend) → geometrically-real terraces.

**Bridge (load-bearing correctness):**
- Compute prologue, manifest convention verbatim: `uv = (vec2(xi,yi)+0.5)/dim` (**texel-CENTER** — current generators use corner, `fbm.cpp:35`), `xz_m = (uv-0.5)*extent_m + origin`, feed `vec3(xz_m.x, h_cur_m, xz_m.y)` as the rebound `opos`/`wpos`.
- **Displacement = NORMALIZED additive delta** `h_norm += expr_m/height_m`, never absolute meters — OR a `pre_exposed` flag on `CaptureRequest` so `hfdflow.cpp:241` skips its `[min,max]→[0,1]` renorm (the silent corruptor). Resolve the `HEIGHT_M (4000)` vs `BakeEnv._height_scale_m (9830.25)` default disagreement.
- Feed the expression the **physical** `height_m`, never erosion `exaggerated_height_m`.
- One frequency convention: drive the expr path off the per-meter `xz_m` basis so material `scale` and bake `frequency` mean the same thing.

## 6. Data-flow direction & projection (the deciding architecture)

There are **two complementary coupling directions**, and the **projection problem decides which to use for what**.

**Direction A — shared sub-expression → multiple sinks.** Author a `SurfNode`-returning function once; feed it to `self.surface(...)` (fragment) AND `self.hfdisplacement(...)` (compute). Do **not** have the HF module "load a Ptex3d and pull things out" — that couples the module to material internals. The HF sink receives a *lowered GLSL body*; the **portable-core gate is the "pull what it needs"** (it rejects fragment-only ops). Mental model: *a fragment function is a value; sinks decide where it lands.*

**Direction B — bake exports FEATURE fields → fragment samples them.** The bake computes quantities that need the heightfield's **global / neighborhood** structure (slope, flow accumulation, flow direction, curvature, terrace/strata id, AO). These are **properties of the heightfield**, naturally defined on the 2D top-down uv, and many are expensive/iterative → compute once, sample cheaply at runtime.

**The projection line (the key rule):**
- ✅ **Bake FEATURE fields, not appearance.** Slope-at-a-point is a scalar on the 2D surface → sampling it top-down is *correct*, no smear. Same for flow/curvature/ids/AO.
- ❌ **Never bake colored APPEARANCE to top-down uv** — it smears on verticals (a cliff is degenerate under `uv=xz/extent`). **Synthesize appearance procedurally IN THE FRAGMENT in 3D/world space** — which is exactly what hmview already does (`p = ctx.P_object`, world-space strata, **no uv texture, no smear**).

**Projecting onto vertical faces (the strata-on-a-riser problem), using slope + flow:**
- **World-Y strata is already projection-free**: hmview's `sin(dot(p,(0,1,0.12)))` makes bands horizontal in *world* space regardless of surface orientation → correct on a cliff with no work. (The `cliff_compress` hack becomes unnecessary.)
- **Triplanar for 2D-pattern detail** — `ptex3d/functions.py:27 triplanar(ctx, fn2d, scale, sharpness)` already blends X/Y/Z projections by the surface normal. On a vertical face the normal is horizontal → it picks the vertical-plane projection → grain runs *along the face*, no smear. Blend **planar↔triplanar by slope**.
- **Flow direction aligns anisotropy** — sample the baked flow-direction field; on a riser, water runs straight down → orient drips/erosion streaks along *real* drainage (hmview fakes this today with `streak`).
- **Slope is the projection SELECTOR** — flat → top-down/planar (sediment, grass); steep → world-Y + triplanar + flow-aligned.

**"Non-physical space" nuance:** the bake runs in its native uv/grid domain (fine), but the exported feature **values must be physical** (real slope angle, real flow — via the scale contract). The material samples by uv. So "non-physical space" = native *domain*, physical *values*.

**The virtuous loop:** `hfdisplacement` carves **real terraces** → the bake measures slope/flow on the *terraced* geometry → the material shades using those features with proper (flow/slope-driven) projection. Displacement and feature-export reinforce each other.

**Feature fields to export:** `slope` ✅ (`ops.py:86`), `curvature` ✅ (`:107`); **flow accumulation + flow direction** (NEW module — also needed by the segmentation work); `terrace_id`/`strata_id` (quantize world-Y, or from the displacement); `ao`/openness (optional). All are projection-safe (heightfield properties).

## 7. Single-source authoring + LLM legibility

- **Unified op library as an additive `@op` layer** over existing `P`/`T` callables (the registry SKILL.md describes **does not exist in code yet** — zero `@op` hits). Don't rewrite `_Ops`. Each portable op carries one canonical math def + per-target emitters; `P.fbm`/`T.Fbm` resolve to the same `OpInfo` → same noise to both targets.
- **Per-op `targets` annotation** + `registry.portable_core()` (enumerable, in LLM context + autocomplete). Almost everything is `{fragment,compute}`; only view-dependent **atoms** (`NV`/`eye`/`Cd`) are `{fragment}`. `fbm`/`fbm_aa` are `{fragment,compute}` (they route through `ctx.footprint`, §3).
- **A `BakeCtx` typed wall** exposing the portable atoms (`P_object`, `uv`, `footprint`, world `P`/`N`) but **not** the view-dependent ones (`NV`/`eye`/`Cd`) so a bake author *cannot reach* them — far more reliable than docs. Note `fbm_aa`/`footprint` are deliberately *present* in `BakeCtx` (they port); the wall only hides what genuinely can't bake.

## 8. Risks (ranked)

1. **Noise import-into-compute unproven** (gating) — no terrain compute module imports a libblock; `lib_mmnoise` carries a `sampler3D`. → split sampler-free `lib_pnoise`, smoke-test first (§4, Phase 0).
2. **Capture auto-exposure** (`hfdflow.cpp:241`) silently rescales meters displacement → normalized delta + `pre_exposed` flag (§5).
3. **Screen derivatives** — *downgraded* (§3 decision): AA is target-polymorphic via `ctx.footprint`, so `fbm`/`fbm_aa`/`aa_ramp` **port** (compute band-limits to texel size). Residual rejects are only view-dependent atoms (`NV`/`eye`/`Cd`) + a bare `fwidth()` of a non-footprint *derived* value (needs a grid finite-difference — deferred). Much smaller than first thought; no two-function split.
4. **Coordinate/frequency/texel-center** mismatch → one physical basis in the prologue (§5).
5. **Cook determinism** — `ExprModule.cookComputeHash` must `accumulateString(_body)` over the exact generated text; assert byte-identical emission across processes (free-threaded-Python set/dict order; `_emitter_deps` sorts, CSE is key()-memoized — assert it).
6. **Serialization** — `_body` round-trips as a reflected `std::string` (precedent `CaptureModuleData::_channel`); idempotent under double `reshapeIOs`.
7. **Cellular structs** — co-emit `ptex_voro_t` typedef into compute or restrict to vecN; reject `.fwedge`.

## 9. Phased plan

- **Phase 0 — noise + libblock-into-compute de-risk. ✅ DONE (gate GREEN).** Created sampler-free `lib_pnoise` (`pnoise.i2`); `ork.lev2/pyext/tests/llgfx/test_compute_pnoise.py` confirms `noise(vec3)` compiles + runs in a COMPUTE shader (lifecycle mirrors `test_terrain_bake.py`: `ecs.headless_appinit(use_subsystems=[…])` + `bindGfxToCurrentThread` — the `lev2appinit`/`loadingContext` path segfaults). The substrate is unblocked. **Noise packaging for Phase 1:** default to INLINE the libblock (proven, works regardless); switch to the shared `import "orkshader://pnoise.i2"` only if that variant compiled in the test run.
- **Phase 1 — `hfbake`/`hfmask` (compute-only, unified noise + `ctx.footprint`). ✅ DONE — VERIFIED (built; `test_terrain_hfbake.py` bakes `stripes`+`mottle` and they render).** The unified-substrate spine works: a ptex3d SurfNode → `emit_compute_field` → compute shell → `ExprModule` → channel, with `lib_pnoise` `noise()` running in compute. C++ `hfdflow_module_expr.cpp` (generic `ExprModule`: reflected `_shadertext` w/ `%DIMU%/%EXTENT_M%/%HEIGHT_M%` holes, hashed in `cookComputeHash`) + `hfdflow.h` decl + `lev2_init` register + `pyext_gfx_terrain` bind. Python: `emit_compute_field`/`_bake_literal` + `ctx.footprint` atom (`dsl.py`), `compute_template.py` (shell + texel-center prologue, maps `lib_mmnoise→lib_pnoise`), `ops.expr_field`, `base.hfbake/hfmask`, and the **portable-core gate** (rejects `ctx.NV`/`eye`/`Cd` at trace time with a legible error — verified). Codegen validated pre-build (stripes=pure-ALU, mottle=`lib_pnoise` noise). Acceptance: `pyext/tests/llgfx/test_terrain_hfbake.py` bakes `assets/terrain/strata_bake.py` (stripes + mottle channels). **Deferred sub-item:** the `fbm`/`fbm_aa` collapse to a single footprint-aware `fbm` (the `ctx.footprint` atom + compute-shell `footprint` are in place, but `P.fbm` still emits the `lib_mmnoise`/`_ptex_fbm` form — plain `fbm`/`noise` bake fine; folding `fbm_aa` to read `ctx.footprint` is a small follow-up once a bake expr needs it).
- **+ NormalizeModule (added).** `hfdflow_module_normalize.cpp` — explicit `[min,max]→[out_lo,out_hi]` rescale (GPU atomic min/max reduce + rescale; float plugs `out_lo`/`out_hi`); `T.normalize(node, out_lo, out_hi)`. The controllable counterpart to the flush's unconditional auto-exposure (FATAL #2): put the renorm where you want it; pairs with the future `pre_exposed` capture flag in Phase 2.
- **Phase 1.5 — feature bake.** Add a **flow accumulation + flow-direction** module (shared with segmentation); export `slope`/`curvature`/`flow`/`terrace_id` via `hfbake`. Material samples them to drive projection (triplanar blend by slope, flow-aligned anisotropy). Proves Direction B + the projection architecture (§6).
- **Phase 2 — `hfdisplacement` (geometric terraces, headline). ✅ IMPLEMENTED (pending `ork.build.py` + run).** `ExprModule` is now **multi-input** (`In0..In7`, sorter-tolerant when unconnected; bound contiguously slot 1.. ). New ctx atoms: `ctx.input(k)`, `ctx.height_m`, `ctx.extent_m`. **Convention: input 0 = the current height → the shell sets `ctx.P_object.y = in0·height_m` (physical)**, so the *same* `strata(ctx)` that shades a material also displaces — baked benches coincide with shaded bands (the unification). `base.hfdisplacement(expr, into, *extra, mask=)` wires `into`→In0 (+ extra→In1.. ), optional `into.masked_by(result, mask)`. Demo: `strata_bake.py` terraces an fbm base (`terrace_strata` snaps `ctx.P_object.y` to band elevations); `test_terrain_hfbake.py` checks the `terraced` channel. Pair with `T.normalize` / the future `pre_exposed` capture flag to keep the flush from auto-exposing the displaced height. Codegen validated pre-build (1-input shell: `sif_in0`, `opos.y=in0·height_m`).
- **Phase 3 — seamless authoring + LLM legibility.** `@op` registry + `OpInfo.targets`, `registry.portable_core()`, `BakeCtx` typed wall (hides only view-dependent atoms), `sys._getframe` provenance + `OP_NOT_BAKE_SAFE` diagnostics. (`ctx.footprint` already landed in Phase 1.) Pure Python, no behavioral change.
- **Phase 4 (optional) — ptex2d unification** (same emitter, different IO wrapper). Defer CPU indefinitely (headless compute).

## 9a. Phase 0 — detailed spec (the gate)

**Gate (GREEN):** a *compute* shader computes `noise(vec3)` from a shared, sampler-free noise libblock, and the value matches the *fragment's* `lib_mmnoise.noise(vec3)` for the same input. GREEN unblocks the entire substrate (ExprModule → hfbake → hfdisplacement → strata-terrace).

**Risk: LOW** — the precedents nearly answer it: `pha` already inlines a libblock in a compute interface and runs (`pha.cpp:35` `libblock lib_pha{…}`, `:117` `cs_pha : iface : lib_pha`); `typ_mmnoise` is a bare `struct`, **no sampler uniform** (`misctools.i2:1-7`); `octavenoise`'s `sampler3D` is a function param `P.fbm` never calls; `noise(vec3)` is `floor/fract/dot/mix/sin` — the same primitives terrain's own fbm already compiles. The *only* genuine unknown is whether `import "orkshader://pnoise.i2"` resolves into a **compute** interface — a nicety with a proven inline fallback.

**Step 1 — create `lib_pnoise` (sampler-free).** New `ork.data/platform_lev2/shaders/fxv2/pnoise.i2`:
```
libblock lib_pnoise {
  float hash(float n){ ... }   // verbatim misctools.i2:30
  float hash(vec2 p){ ... }    // :33
  float noise(float x){ ... }  // :37
  float noise(vec2 x){ ... }   // :44
  float noise(vec3 x){ ... }   // :66   <- the one P.fbm calls
}
```
Bodies copied **verbatim** from `lib_mmnoise` → ptex3d output stays byte-identical. No typeblock, no `octavenoise`, no sampler. (`lib_mmnoise` can later `import "orkshader://pnoise.i2"` and keep only `octavenoise`.)

**Step 2 — smoke test (the gate).** Reuse the proven compute path; two variants:
- **2a (guaranteed, inline):** a throwaway/temporary compute that *inlines* `lib_pnoise` text and `: iface : lib_pnoise`, writes `noise(vec3(xz_m, 0))` to a 1-channel SSBO; bake a trivial DSL; confirm it **compiles + dispatches + gives sane `[0,1]` values**. Mirrors `pha` exactly → must pass. Cheapest harness: a pyext test under `ork.lev2/pyext/tests/llgfx/`, or temporarily point the existing `fbm` module's shader at `lib_pnoise.noise(vec3)` and bake.
- **2b (the nicety, import):** same but `import "orkshader://pnoise.i2"` + `: iface : lib_pnoise` instead of inlining. Compiles → shared-`.i2` import into compute works.

**Step 3 — byte-identity.** Same `vec3` input → fragment `lib_mmnoise.noise` == compute `lib_pnoise.noise` (trivially true if verbatim). Confirm by repointing ptex3d `P.noise`/`P.fbm` to `lib_pnoise` (`dsl.py inherits` + add the import to `fxv2_template.py:43`) and checking an existing material (hmview/marble) renders pixel-identical.

**Outcomes → Phase 1 packaging:**
- 2b passes → share via `pnoise.i2` **import** in both fragment and the ExprModule (cleanest).
- 2b fails, 2a passes → **inline** the libblock: `compute_template.py` emits the *same* `lib_pnoise` text the fragment uses (one Python string constant) — still one source, byte-identical.
- both fail (very unlikely given `pha`) → a true dialect problem; investigate, but `noise(vec3)` uses only primitives terrain fbm already compiles.

**Files:** new `pnoise.i2`; one smoke test (pyext test *or* throwaway module edit); optional ptex3d repoint (`dsl.py`, `fxv2_template.py`). **Needs `ork.build.py`** + a run. **Effort ~½ day, mostly confirmation.**

## 10. Key files

Lowering core `ptex3d/dsl.py:635-723`; fragment shell `ptex3d/fxv2_template.py:316-468`; generic-body precedent `hfdflow_module_erox.cpp:34-54`; noise `misctools.i2:66` (canonical) vs `hfdflow_module_fbm.cpp:41` (legacy); libblock-in-compute precedent `hfdflow_module_pha.cpp:117`; import resolver `vulkan_fxi_load.cpp:95-202`; auto-exposure `hfdflow.cpp:241-269`; sinks `terrain/base.py:74`; scale contract `terrain/manifest.py` + `hfdflow.h:104`; triplanar `ptex3d/functions.py:27`; slope/curvature `terrain/ops.py:86,107`; canonical material `assets/materials/hmview.py:106,134,156`.

## 11. The structural spine — one transform-graph currency, shared ops, divergence boundaries

> **STATUS: foundation LANDED (2026-06-25); advanced layer design-accepted, unbuilt.** This section is the
> shared-spine home for the topology layer (flora / cities / creatures). The foundational primitive — the
> `XfNodeGraph` interchange plug (G0a) — HAS landed and renders (via the L-system slice + `LSweep`), so
> `XfNodeGraph`, its shared ops, and the first generator family are real code. What remains unbuilt is the
> grammar generalization + the creature/city/skinning milestones, several of which were rated
> infeasible-as-written by a 9-verdict feasibility pass — see §16 for the feasibility-adjusted scope. Treat
> the *advanced* build plans as target architecture, not committed contract, until each lands and renders.

### 11.1 The one structural decision: SPLIT the type, SHARE the currency and the ops

The substrate already proves the correct unification mechanism three times in `interchange.h`: a thin,
family-neutral, GPU-resource-handle plug whose consumers branch on a representation/semantic
discriminator (`SdfGrid._repr` is the template). The structural spine follows the same cut:

> Add ONE thin **`XfNodeGraph`** interchange plug — structurally `InstanceSet` + an edge buffer —
> `mat4[count]` + parent/edge buffer + `vec4 attrs[count]` + `uint tags[count]`, with **no semantic
> attr-bag**. Domain semantics ride the existing typed-attr `vec4[count]` and the A1 `__tags` 32-bit
> band convention (reused verbatim). Each family is a **distinct producer** that emits a `XfNodeGraph`
> and **asserts its own topology invariant at the sink** (is-tree / acyclic / planar). `Skeleton` is a
> thin `XfNodeGraph → XgmSkeleton` **adapter**, not a separate plug type — promoted out of hypermesh
> exactly as `SdfGrid` was promoted out of terrain.

**Naming + namespace (DECIDED 2026-06-25).** The type is **`ork::hyper::XfNodeGraph`** (the graph) over
**`ork::hyper::XfNode`** elements (each = a transform + tag + attrs + slot-refs). The `Xf` prefix + the
fresh `ork::hyper` namespace deliberately dodge a crowded neighborhood: `ork::TransformNode` (the math
TRS node, `ork/math/TransformNode.h`) and four overloaded `Node`s in `ork::lev2::gfx` (scenegraph /
renderqueue / drawable / tristripper). `ork::hyper` is also the intended future home for the whole
family — `ork::hyper::mesh` (the deferred `ork::lev2::hypermesh →` rename), `ork::hyper::sdf`, … —
defining it now plants that flag without forcing the migration.

**LANDED 2026-06-25 (G0a)** in `ork.lev2/inc/ork/lev2/gfx/dflow/interchange.h`. The CURRENCY type is in
`ork::hyper`; the dataflow PLUG wiring is the 4th handle in `ork::lev2::dflowgfx` (peer of
`GpuComputeImage2D`/`InstanceSet`/`SdfGrid`), registered in both hmdflow dgctx blocks as `"hm_xfng"`.
`XfNodeGraph` is a runtime plug VALUE regenerated by its producer module (like `SdfGridInst`) — **NOT a
reflected `ork::Object`**; the reflected/serialized thing is the MODULE (`LSystemModuleData`, §17).
```cpp
namespace ork::hyper {
struct XfNode { float _xform[16]; uint32_t _parent; float _attrs[4]; uint32_t _tags; }; // attrs/tags domain-specialized; tags=A1 [20:32) gid band
using xfnode_vect = std::vector<XfNode>;
struct XfSlot { uint32_t _node; float _local[16]; uint32_t _tag; };          // named attachment point (leaf/spine/window…)
using xfslot_vect = std::vector<XfSlot>;
struct XfNodeGraphData { bool _reserved = false; };                          // static config (sibling interchange-data pattern)
struct XfNodeGraphInst {                                                     // the runtime value carried on the plug
  FxShaderStorageBuffer *_xform, *_parent, *_attrs, *_tags;                  // GPU SoA (SSBO-backable, lazily allocated)
  xfnode_vect _nodes; xfslot_vect _slots;                                    // CPU mirror — branchy bake-time ops (rewrite, XgmSkeleton flatten)
  int _count; uint64_t _version, _topoVersion; void markChanged(); void markTopoChanged(); // dirty tracking (GpuMesh-style)
};
} // namespace ork::hyper
// dflowgfx::XfNodeGraphPlugTraits + xfng_{in,out}plugdata_t = the plug wiring (registered "hm_xfng")
```

We do NOT add one rich `TransformGraph` with a semantic attr-bag every consumer carries: that forces
defensive re-validation of topology and makes a StreetGraph carry dead bind-pose fields. The thin-plug
cut keeps the shared header POD-thin (matching the three handles already in `interchange.h`).

### 11.2 The shared interchange TYPES (~7), reconciled with reality

| Seed type | Decision | Home / form |
|---|---|---|
| **TransformGraph (linchpin)** | Add as thin **`XfNodeGraph`** handle. NOT a rich semantic type. | NEW in `interchange.h`: `mat4[count]` + `uint parent[count]` (CSR edges optional side-table) + `vec4 attrs[count]` + `uint tags[count]` + `_version`/`_topoVersion`. |
| **Skeleton / WeightMap / BoneRef** | Do NOT promote as plug types. `Skeleton` = `XfNodeGraph` tagged `SKELETON` + a `XfNodeGraph→XgmSkeleton` adapter. `WeightMap` = the reserved `BONEIDX`/`BONEWT` GpuMesh channels. `BoneRef` = a Python-side `int` index. | Adapter in C++; channels in `GpuMesh`; index in DSL. (Corrects the SKILL.md table that listed `Skeleton`/`WeightMap` as hypermesh-local plugs.) |
| **SDFGrid** | Shipped (`SdfGrid`, DENSE behind `_repr`; NANOVDB reserved/unbuilt). | `interchange.h`. |
| **MeshBuffer / GpuMesh** | Do NOT promote to the spine. `GpuMesh` carries COW channel pools, edge domain, adjacency cache, GPU-resident count — a huge family-specific surface. Pass it cross-family as an opaque `gpumesh_ptr_t` the way it already works. | Stays in `hmdflow.h`. |
| **Field / HeightExpr / Mask** | Shipped as `GpuComputeImage2D`. | `interchange.h`. |
| **InstanceSet / ScatterSet / PrimSlot** | `InstanceSet` shipped (E.2). `ScatterSet` = `.ogeo` point geometry → fills `InstanceSet`. `PrimSlot` = the named-attachment-slot currency; rides the `XfNodeGraph` `_slots` side-table, NOT a new plug. | `interchange.h` + `XfNodeGraph._slots`. |
| **Pose / AnimClip / posegraph** | Stay hyperanim-local — NOT shared interchange (per-frame, consumed by ONE runtime). Only the **Skeleton** is shared currency. | hyperanim-local. |
| **gid tag (material partition)** | Shipped (A1 `__tags[20:32)` 12-bit gid). Reused verbatim by `XfNodeGraph.tags`. | A1 contract unchanged. |

`XfNodeGraph` is CPU-or-GPU but defined GPU-SoA: the rewrite engine and `bake_skeleton` are
trace-time/CPU (branchy, parent-chain-accumulating, random access), while `sweep`/`skin`/
`instance_at_slots` are GPU. So `XfNodeGraph` is an `InstanceSet`-shaped SoA handle (SSBO-backable) with
a CPU-resident `std::vector<node>` mirror the trace-time ops build; `_version`/`_topoVersion` signal
dirty exactly as `GpuMesh` does. (Feasibility note: the richer fields — CSR edges, `_slots`,
dual-version dirty — actually mirror `GpuMesh`, NOT `InstanceSet`; do not claim a byte-for-byte
`InstanceSet` copy. See §16.)

### 11.3 The shared OPS (~9), as a write-once family-neutral op library

| Op | Decision | Reuse / home |
|---|---|---|
| **sweep** (`XfNodeGraph` → GpuMesh) | **LANDED 2026-06-25 as `LSweep`** (`hmdflow_module_lsweep.cpp`: CPU v1, sides/cap params, radius from `_attrs[0]`, stable-vs-topo rebuild split; GPU-compute port = owner-ratified follow-up). `skin`/bone-channel writing remains a SEPARATE later unit, gated on a skinned consumer — see §16. | CPU build from the XfNodeGraph mirror; the GPU port will reuse the extrude ring-field/stitch machinery. |
| **auto_skin** (GpuMesh + Skeleton → weights) | NEW; proximity-first GPU compute (nearest-k bone, capsule distance). Self-defends (Maya-grade) or fails loudly. Heat-diffusion deferred (BasinFill multigrid is the prior art). **Gated on a skinned render consumer (none exists today).** | `DisplaceBySdf` per-vertex-vs-resource pattern is the template; verify via SSBO/OBJ readback oracle, not render. |
| **instance_at_slots** (`XfNodeGraph._slots` → InstanceSet) | Fill `InstanceSetInst` from the slots side-table. | `interchange.h` (E.2 machinery). |
| **field_sample** | A genuine NEW family-neutral position→{value,gradient} query op over `GpuComputeImage2D`/`SdfGrid`. **It does NOT exist today** (the `DisplaceByField`/`DisplaceBySdf` sampling is inline kernel text, two divergent blocks). First extract a shared shadlang `lib_fieldsample`, then wrap it. There is NO separate `scene_query` op to merge — that name is fictional. | extract from `displace.cpp` (bilinear 2D) + `displace_sdf.cpp` (trilinear 3D phi+grad). |
| **sdf ops** (union/subtract/blend/conform/shrinkwrap) | SHIPPED. `Csg`, `MeshToSdf`, `SdfToMesh`; `DisplaceBySdf(conform)` IS shrinkwrap. Grounds booleans-as-SDF (A3 #4). | `ork.lev2/src/gfx/sdf/` + `hmdflow_module_displace_sdf.cpp`. |
| **mirror** | SHIPPED for face/vertex (`MirrorModule`). Generalizing to `XfNodeGraph` is a SEPARATE inst, not 'one op, two overloads' — `MirrorInst` is `MeshComputeInst`-locked to GpuMesh SoA SSBOs. Deferred until `XfNodeGraph` exists. | `hmdflow_module_mirror.cpp` (mesh form); node form is net-new. |
| **radial_repeat / phyllotaxis** | NEW; a rewrite rule emitting N rotated child frames; the mirror module's rotational sibling. | new, beside mirror. |
| **time_evolve** (clock → param) | SHIPPED contract: `MeshEnv._abstime/_dt`, `IPrePhaseParams::writeParams`, `S.time` bridge. Lives on the **live** side. | B.4 clock contract. |
| **multi-sink reduce** | SHIPPED pattern in terrain (bake); hypermesh `materialize()` returns ONE GpuMesh today. Named multi-sink on the live mesh path is unbuilt. | terrain `CaptureModule` is the precedent; hypermesh multi-sink is forward work. |

Two further dedups: **`gid` generalizes to all node-tagged domains** (`GidAssign` promoted to any tagged
interchange plug, A1 unchanged — but `GidAssign` is mesh-locked today; this awaits `XfNodeGraph`); and
the **field_sample extraction** above (one op, not a merge of two existing ops).

### 11.4 Divergence boundaries — four correct cuts plus a fifth

1. **gen/bake vs pose-eval-per-frame** — strongest cut. Shared currency = `XfNodeGraph` + `XgmSkeleton`
   adapter. Enforced by the dataflow-vs-stateful family-kinds split + the bake (`_abstime=0`) vs live
   split. hyperanim = thin dataflow over `XgmAnim`/`XgmLocalPose`; behavior = thin stateful over
   `FsmInstance`. Do NOT invent a new split.
2. **garment GEOMETRY shares skin+sdf; cloth SIMULATION is a separate stateful runtime.** Geometry is
   buildable today: shrinkwrap = `DisplaceBySdf(conform)` over a body SDF (the shipped `sdf_conform.py`
   flagship is exactly this workflow). Cloth SIM = a stateful family; prior art = `TemporalSmoothModule`'s
   persistent inter-frame buffer + `DisplaceBySdf`'s relax-loop Jacobi dispatch. Cloth must NOT be a
   per-frame dataflow `compute()`.
3. **anatomy/muscle/fat/skin-fold is creature-specific** — a composition of existing shared ops
   (SDF-blend CSG + ptex3d displacement / FS hooks), NOT a new shared type. Opt-in; a tree's
   sweep+rigid path never invokes it.
4. **city planar-graph ops (block-cycle, straight-skeleton, navmesh) are city-specific** — they require
   the planar-cyclic invariant a tree can never satisfy. Keep them as separate ops over a `XfNodeGraph`
   that asserts planarity at its sink.
5. **NEW — bake-once topology generation vs live-mutable parameter evolution.** The grammar/rewrite
   engine sits entirely on the **bake-once** side; `time_evolve` (clock→param) sits on the **live**
   side. Maps onto the existing `_topoVersion` (topology) vs `_version` (params) dirty split. Forcing a
   rewrite to be live would re-pool GPU buffers per rule application.

### 11.5 The Skeleton linchpin — one currency, three consumers

1. **GEN** — a generator emits a `XfNodeGraph` tagged `SKELETON`. Bind-pose hardness resolves at the
   *bake* seam: `bake_skeleton` reads a per-node `skel.bind_local` typed attr (defaulting to the
   node's `_xform`), accumulates down the parent chain to world bind matrices, inverts for
   inverse-bind, and emits a flat `XgmSkeleton`. An L-system scaffold never calls `bake_skeleton` and
   pays zero skeleton cost.
2. **SKIN** — `sweep`/`auto_skin` add `BONEIDX`/`BONEWT` to the GpuMesh against the SAME skeleton.
3. **DRIVE** — hyperanim consumes the SAME `XgmSkeleton` as its `evaluate(time)→xgmlocalpose_ptr_t`
   binding target.

The plug is `XfNodeGraph`; `XgmSkeleton` is the **baked sink output**, never a plug type.

**Feasibility correction (load-bearing — see §16).** `XgmJointProperties` today is only
`{_numVerticesInfluenced, _children}` — it has NO joint-limit/orient fields, so `bake_skeleton` cannot
'lift limits/orient' into it without extending the type + its XGM serialization (a separate rig-props
unit). Skeletons serialize via the **XGM chunkfile**, NOT the dflow GraphData JSON round-trip. The
`XgmSkeleton` Python binding is currently READ-ONLY (no constructor/addJoint/setters). And the
inverse-bind currency can only be validated by skinning a real mesh and rendering it — 'validate with
no mesh work' proves nothing. The buildable, M-sized core is a C++ `flat joint table → XgmSkeleton`
flattener mirroring `meshutil_import_assimp_skeleton.cpp`; everything around it is foundation work.

## 12. The grammar / rewrite engine, scoped honestly

> **STATUS: the L-system slice (G1, v1 hardcoded grammar) LANDED; the general rewrite/grammar engine is
> design-accepted, unbuilt (rated infeasible-as-written by feasibility — re-scoped in §16).**

L-systems (plants), CGA shape grammars (buildings), and segmented-creature topology are all
predicate-match-then-substitute rewriting over a node-graph. The multi-segment extrude
`cs_field_seg`/`cs_face_seg` chain accumulator is *already a degenerate, GPU-unrolled, one-rule linear
rewrite*. The grammar engine is its branching generalization. Scoping:

> **The shared unit is the REWRITE STEP as a TRACE-TIME mechanism, not a monolithic runtime engine,
> and NOT a per-frame `DgModuleData`.** Rewriting is structural → it sits on the bake-once side
> (boundary 5). Build it as a trace-time Python module beside `_trace.py`/`_lower.py` that expands a
> rule-set into a `XfNodeGraph`. Each family supplies ONLY its rule vocabulary + a small interpret
> kernel. **Ship parametric + stochastic first; DEFER context-sensitive and environment-sensitive to
> the second family that needs them.** Do NOT build a universal CGA+L-system+creature rule AST up front.

| Axis | Mechanism (reuse) | Phase |
|---|---|---|
| **parametric** | symbol `params vec4[P]` carried through rewrite; the `kMaxExprParams=8` EXPRP plug contract (B.4) is the runtime-pokeable channel. | first |
| **stochastic** | the stateless counter-hash RNG terrain scatter already ships (`hash(seed, symbol_id, depth)`); deterministic by construction (A3 #1). | first |
| **context-sensitive** | a rule matches a symbol + neighbor window: a gather over the rewrite runtime's OWN production adjacency (which the engine defines). | deferred |
| **environment-sensitive** | a rule predicate samples a `Field`/`SdfGrid` at a position via the NEW `field_sample` op (§11.3) — reusing the shipped E.1 cross-family edge + ENV-OWNERSHIP pattern. | deferred |

**Serialization (A2):** the rule-set serializes the trace OUTPUT (the emitted `XfNodeGraph` / a plain
Python/JSON rule descriptor), never the Python generator.

**Feasibility correction (category error — see §16).** Rule predicates do NOT serialize as the
`SelectModule`/`kPredicateABIVersion` GLSL ABI. That ABI is a GPU PER-MESH-ELEMENT predicate (traced
GLSL reading `fP`/`fN`/`fArea`/`_eid` over a live GpuMesh `__tags` channel). L-system rules are
trace-time Python decisions over abstract symbols at a recursion depth — there is no mesh/face/GLSL
context at rewrite time. Rules serialize as plain data, a NEW serialization. Likewise, the
`_adj_off/_adj_face` adjacency-CSR is the WRONG reuse model for production adjacency (it is post-hoc
vertex→face connectivity over an already-materialized static GpuMesh). And data-dependent topology
generation (N branches where N depends on a sample) is UNPROVEN on this substrate: every hypermesh
generator bakes topology from baked int plugs; `_gpuResidentCount` (runtime GPU-resident face count)
has zero producers today. Proving runtime-data-dependent topology in isolation is a prerequisite.

**The three interpreters (symbol stream → `XfNodeGraph`):**
- **(a) L-systems — turtle.** `F`/`+`/`−`/`[`/`]`/`L`. = multiseg-extrude per-ring frame accumulation
  + a push/pop stack (the one honest engine extension).
- **(b) CGA shape grammars — split/repeat/extrude scopes.** A scope is a `XfNodeGraph` node
  (`_xform` = oriented box frame, typed-attr carries extent). The terminal geometry emit (footprint →
  faces) is a per-family interpret kernel, NOT a turtle.
- **(c) Creatures — segment/limb emission.** Spider = `Body → radial_repeat(8, LegChain)`;
  vertebrate = `Spine(chain) → branch(limbs)`. The emitted `XfNodeGraph` tagged `SKELETON` IS the
  creature skeleton.

**Fenced as NOT rewrites** (city-specific, consume the engine's output): straight-skeleton parcel/roof
and planar-graph block-cycle extraction require the planar-cyclic invariant a tree can never satisfy.

## 13. New families / generators (all emit the shared `XfNodeGraph`)

`lsystem` (turtle plants) + sibling generators: space-colonization (tree crowns), phyllotaxis
(rosettes / spider-legs / radial_repeat), tensor-field (streets), straight-skeleton (parcels/roofs).
Each is **only** a rule vocabulary + an interpret kernel + a topology-invariant assertion at its sink —
the thin-specialization thesis. Creatures are reachable from hypermesh (procedural rig/skin) + the
grammar engine + hyperanim. See SKILL.md for the per-family plug-type registry entries.

## 14. Use-case → reuse matrix

| Use case | Generator (new) | Shared currency | Shared ops | Family-specific |
|---|---|---|---|---|
| Tree / plant | lsystem(turtle) | XfNodeGraph(LSYS) | sweep, field_sample (slope bias), instance_at_slots (leaves), mirror | — |
| Spider / quadruped | grammar(radial_repeat/limb) | XfNodeGraph(SKELETON) → XgmSkeleton | sweep, auto_skin, mirror, sdf-blend | anatomy/fat/fold layer (opt-in) |
| Clothed human | grammar + garment | XfNodeGraph(SKELETON), SdfGrid(body), GpuMesh(garment) | auto_skin, DisplaceBySdf(shrinkwrap), sdf ops | cloth SIM (separate stateful runtime) |
| Building | grammar(CGA scopes) | XfNodeGraph(FRAME) | gid (storey class), multi-sink (lod/collider/nav) | straight-skeleton roof; mesh-merge join (separate op) |
| City | tensor-field streets | XfNodeGraph(STREET) | gid (road class), instance_at_slots (props), field_sample | block-cycle/parcel/navmesh (separate ops) |
| Terrain (shipped) | terrain compute | GpuComputeImage2D, ScatterSet | field_sample, multi-sink | — |

## 15. Sequencing — RESOLVED by the owner (2026-07-04)

*(Historical framing: the holistic seed said forests-first; the docs locked racer-first as the
ratified acceptance gate (A3 #5); these drive different shared primitives and never conflicted at
the engine layer.)* **Owner resolution 2026-07-04: the racer-first gate is retired as a sequencing
constraint. Sequencing optimizes the holistic outcome — rich, natural, beautiful scenes at VR
performance, minimum procedural-DSL code, DSL that reads clearly. The racer remains a valid gate
ASSET for hard-surface/gid work.** The concrete milestone order under this directive lives in
`~/JUL04_GRAMMARS.md` (grammar-as-data → buildings → organs/slots → creature gen → skinning).

## 16. Feasibility-adjusted build order (the honest scope)

> A 9-verdict adversarial feasibility pass against the real substrate found the original G0–G3 plan
> mis-scoped: it leans on a `XfNodeGraph` type, a skinned render path, a `field_sample` op, a
> `FamilyBase` class, and a branch-capable mesh primitive — **none of which exist**. The reshaped order
> below is what the substrate actually supports. Each milestone is GREEN only when it renders on screen
> (or is verified by an OBJ/SSBO readback oracle where nothing skins yet).

**G0 — `XfNodeGraph` interchange plug (split into G0a + G0b; DROP the dead skin half).**
- **G0a — the plug.** Add `XfNodeGraphData/Inst/PlugTraits` to `interchange.h`
  (`mat4[count]` + parent/CSR-edge SSBO + `attrs vec4[count]` + `tags uint[count]` +
  `_version/_topoVersion` + `markChanged/markTopoChanged`); copy the `SdfGrid`
  `describeX/createInstance/ImplementTemplateReflectionX` boilerplate; a Python host-fill authoring
  entry; **`createRegisters` in BOTH the new grammar dgctx AND both `hmdflow` dgctx blocks** (else the
  type can't flow in hypermesh graphs and `sweep` can't feed the hypermesh render path); extend the
  A.2 JSON round-trip gate. **Do NOT claim a byte-for-byte `InstanceSet` copy — it is a `GpuMesh`-shaped
  type** (CSR edges + `_slots` + dual-version dirty are net-new design).
- **G0b — the `sweep` module.** `XfNodeGraph → GpuMesh`, consume the chain via
  `typedInputNamed<XfNodeGraphPlugTraits>` (DisplaceBySdf pattern); generate a parallel-transported
  profile frame per ring (NEW shader; reuses only the FA/FB/FC ring-field buffers + `cs_face_seg`
  stitching from extrude); emit POSITION/NORMAL/UV/COLOR. Acceptance: a hand-built 3-node graph
  renders a swept tube.
- **DROP `BONEIDX`/`BONEWT` from G0** — they have no consumer (no skinning VS, no joint SSBO,
  no XgmSkeleton bridge in hypermesh), so the 'skin' half is untestable. Defer skinning to its own
  unit gated on a skinned consumer landing first. Rename the unit `skin/sweep` → `sweep`.

**G1 — rewrite runtime, proved GEOMETRY-FREE first.** (G1-as-written is infeasible.)
- (A) Build the trace-time L-system rewrite as a pure-Python module beside `_trace.py`: iterate N
  steps, run the stateless counter-hash RNG, push/pop stack, emit an abstract segment list
  (positions, parent links, depth, radius). Validate as DATA (segment count, depth, branch angles) —
  zero blockers, de-risks the novel part.
- (B) Serialize rules as plain Python/JSON data — **NOT** the GLSL SelExpr predicate ABI.
- (C) For first pixels WITHOUT G0: lower the turtle output to ALREADY-SHIPPED ops — each internode a
  `cone()`/`box()` `transform()`'d into place (or `instance_source` over a unit segment). A jointed-stick
  tree renders today with no new C++.
- (D) Wire the terrain-slope `Field` bias only AFTER geometry exists (the easy, well-precedented part).
- Note: there is NO `FamilyBase` class today (`_trace.py` is free functions; each family copies
  boilerplate); the `@op` registry is the ptex3d surface-op table, not an inheritable base. Either
  accept per-family boilerplate or build the base explicitly.

**G2 — second vocabulary (split: vocab now, skin/drive deferred).**
- **G2a (feasible, M):** `radial_repeat` as the mirror module's rotational sibling
  (`hmdflow_module_radial_repeat.cpp` + one DSL method) + a pure-Python `LegChain` interpreter built
  on existing `extrude_faces(segments=,twist=,scale=, per-face SelExpr)`. Target: `Body →
  radial_repeat(8, LegChain)` → a static 8-legged welded GpuMesh through the existing
  ComputeDrawable/ptex3d path. Proves '2nd family = vocab+interpreter' for the GENERATION half.
- **Defer the skeleton/skin/drive linchpin** behind, in order: (1) bind `XgmSkeleton` authoring
  (addBone/bind matrices) in pyext; (2) add a SKELETON artifact + sink + the `XfNodeGraph` interchange
  plug; (3) build ONE skinning render strategy (a GPU-skinning compute pass over BONEIDX/BONEWT +
  bone-matrix SSBO feeding a new FWD_SSBO_CUSTOM skinned variant — keeps the hypermesh SSBO path);
  (4) `auto_skin`; (5) then `bake_skeleton → auto_skin → XgmPoser` drive. This is decidedly NOT
  'no new engine' — size it L, not a free byproduct of vocab.

**G3 — CGA building (decouple from G2; build the missing join first).** (G3-as-written is infeasible.)
- (U1) ~~Build a **mesh-merge / multi-input join** module first~~ **LANDED 2026-06-26**
  (`hmdflow_module_merge.cpp` + `H.merge(a,b,gid_a=,gid_b=)` — binary concat, per-source gid restamp,
  vidx/CSR rebase in `onTopologyReady`, eval-1 passthrough). N-ary = chained merges. U2 is
  therefore unblocked today.
- (U2) A footprint→walls→roof building authored DIRECTLY in the imperative DSL (extrude storeys; place
  window face-sets via select+gid; merge via U1; flat/hipped roof as extrude+inset cap — **NOT**
  straight-skeleton). Proves an architectural asset renders end-to-end with multi-material gid.
- (U3) ONLY THEN, if a declarative grammar is wanted, a thin split/repeat layer that LOWERS onto
  U1+U2 (the lower-to-existing-modules pattern). Treat straight-skeleton roofs and a real multi-sink
  collider/nav materializer as separate, explicitly-budgeted L items, each landing after its consumer
  is concrete. Do NOT gate G3 on G2 (a building grammar needs no creature vocabulary).

**First commitment: G0a + G0b + G1(A/B/C) — LANDED 2026-06-25** (`XfNodeGraph` plug, `LSweep`, the L-system
M1 slice). The next commitment is the reflected-`LRuleSet` grammar generalization + GPU rewrite; everything
past that is gated on the real prerequisites above.

## 17. First family build spec — `lsystem` (the cane-cholla vertical slice)

*Added 2026-06-24.* This is the concrete, M-sized instantiation of §11–§16: the first `lsystem` archetype
(a New Mexico **cane cholla**) authored in the DSL, traced to reflection JSON, baked in C++, skinned on the
GPU, and instanced. Owner intent: the L-system runtime is **C++/fxv2** (Python is authoring only); the
Python DSL emits the reflected `LRuleSet` JSON the C++ engine runs — no Python at runtime. Per §2's
compute analysis, this bakes once per archetype and is instanced; the grammar never appears per-frame.

### 17.1 DSL shorthands (consistent with the other HYPER\* DSLs)

| Shorthand | Form | Mirrors |
|---|---|---|
| Aliased import, no star | `from ork.hypergraph.dflow import lsystem as L` | every HYPER DSL |
| **Symbol-state arg `S`** | `@L.rule(Shoot)` → `def grow(S): S.len, S.gen, S.env.moisture, S.rnd(a,b)` | hypermesh `S.` per-element namespace (`S.N`/`S.t`/`S.uv`) |
| **Expr operators** | `S.len * 0.96` builds an expr node, not a float | sdf `\| & -`, ptex3d arithmetic |
| **Weighted choice** | `L.choose((0.50, a), (0.34, b), (0.16, c))` | ptex3d `blend([(mat,mask)])`, gradient-stop dicts |
| **Branch helper** | `L.fork(2, 3, lean=(22,42), then=…)` | new, high-level |
| **Serializable guard** | `@L.rule(Shoot, until=S.gen >= 11)` / `L.when(pred, a, b)` | replaces non-serializable Python `if` |
| Sequence | a production returns a `[list]` of ops | hypermesh threaded-node style |
| **Turtle-string flat form** | `rules={"A": "F[&A]/[&A]FA"}` + legend (dual-form) | PbrMaterial flat-vs-nested duality |
| Sinks | `self.mesh()/instances()/collider()/lod()` | multi-sink (all families) |
| Skin reuse | `H.sweep(skel, profile=H.ncircle(7), by_gid=…)` | hypermesh ops |

**LOAD-BEARING RULE:** a production body must be a **serializable combinator tree** (turtle ops + expr
nodes + `choose`/`fork`/`when`), NOT arbitrary Python control flow — it serializes to JSON and runs in C++
without Python. `S.len*0.96` traces to an expr AST; runtime conditions (`gen`, `env`) become guards, not
`if`. Only trace-time-constant `for`/`if` may unroll.

### 17.2 The cholla (combinator form) + dual string form

```python
class CaneCholla(Lsystem):
    seed = "nm-cholla-07"
    def grammar(self):
        Shoot = self.symbol("Shoot", len=0.16, rad=0.020, gen=0)
        @L.rule(Shoot, until=Shoot.gen >= 11)
        def grow(S):
            return [
                L.segment(len=S.len, rad=S.rad, gid="cane"),     # one tuberculate joint (XfNodeGraph edge)
                L.spines(every=0.035, gid="spine"),              # areole slots down the joint
                L.choose(
                    (0.50, Shoot(len=S.len*0.96, rad=S.rad*0.92, gen=S.gen+1)),     # keep growing
                    (0.34, L.fork(2, 3, lean=(22, 42),                              # 2–3 child canes
                                  then=Shoot(len=S.len*0.80, rad=S.rad*0.80, gen=S.gen+1))),
                    (0.16, L.bloom(gid="flower")),               # bloom & stop
                ),
            ]
        @L.rule(Shoot, when=Shoot.gen >= 11)
        def cap(S): return [L.segment(len=S.len, rad=S.rad, gid="cane"), L.bloom(gid="flower")]
        self.axiom(Shoot()); self.iterate(depth=11, segment_budget=700)   # budget caps bake cost
    def build(self):
        skel = self.derive()                                     # CPU rewrite+interpret → XfNodeGraph
        self.mesh(H.sweep(skel, profile=H.ncircle(7), radius_attr="rad", by_gid=True, smooth_joints=True),
                  gid_materials={"cane": Ptex3dCane()})
        self.instances(self.slots("spine"),  mesh=SpineCluster(), align="surface", jitter=0.15)
        self.instances(self.slots("flower"), mesh=ChollaFlower(color=hsv(320,0.8,1.0)), prob=0.40)
        self.collider(H.capsule_chain(skel), kind="compound", static=True)
        self.lod([0.45, 0.12], impostor_at=35.0)
```

Dual-form (same machinery, string-compact for non-parametric grammars):
```python
class DesertBroom(Lsystem):
    axiom = "A";  rules = {"A": "F[+(25)A][-(25)A]F!(0.8)A"}        # F=segment  + -=yaw  [ ]=branch  !=taper
    legend = {"F": L.segment(len=0.12, gid="wood"), "!": L.taper};  iterate = dict(depth=6, segment_budget=200)
```

### 17.3 The reflected `LRuleSet` schema + module placement (what the Python emits as C++ JSON)

**The grammar is NOT a dataflow graph** (corrected from an earlier draft). A `dflow::GraphData` is an acyclic
feed-forward compute DAG; a grammar is a *recursive, self-referential* rewrite system (cyclic by design —
`Shoot` produces `Shoot`) whose evaluation is an iterative fixpoint, not a topological pass. So `LRuleSet` is
plain reflected DATA (`ork::Object`), and **the L-system is a MODULE in the hypermesh geometry graph** — a
`XfNodeGraph`-producing source, peer to `Box`/`SdfToMesh`. The grammar rides as the module's reflected payload;
the rewrite is the module's CPU bake-time compute (like `SdfToMesh`'s CPU allocMesh + re-eval). The thing that
*is* the dataflow graph is the geometry pipeline (§17.5), not the grammar.

```cpp
struct LExpr { enum Kind { CONST, PARAM, ENV, RNG, BINOP, CMP } _kind;  // PARAM="len"/"gen"; ENV="moisture"
  float _const; std::string _ref; uint8_t _op; std::vector<lexpr_ptr_t> _args; };   // BINOP:+ - * / ; CMP:< >= ...
struct LSymbolDef { std::string _name; std::map<std::string,float> _defaults; };    // alphabet
struct LTurtleOp { enum Kind { SEGMENT, PITCH, ROLL, YAW, TAPER, SLOT, FORK, CHOOSE, WHEN, CALL } _kind;
  std::map<std::string, lexpr_ptr_t> _params;  uint32_t _gid;          // gid packs into XfNodeGraph tags[20:32)
  std::string _symbol;                                                  // CALL → non-terminal
  std::vector<lturtleop_ptr_t> _children; std::vector<float> _weights;  // FORK/CHOOSE/WHEN bodies + weights
  lexpr_ptr_t _guard; };
struct LRuleDef { std::string _lhs; lexpr_ptr_t _guard; float _weight=1.0f; std::vector<lturtleop_ptr_t> _rhs; };

struct LRuleSet : public ork::Object {          // reflected grammar DATA — NOT a GraphData (recursive, not a DAG)
  DeclareConcreteX(LRuleSet, ork::Object);
  std::vector<lsymboldef_ptr_t> _symbols; std::vector<lturtleop_ptr_t> _axiom;
  std::vector<lruledef_ptr_t> _rules; uint32_t _depth, _segmentBudget; uint64_t _seed; };

struct LSystemModuleData : public dflow::DgModuleData {     // a hypermesh-family SOURCE module
  DeclareConcreteX(LSystemModuleData, dflow::DgModuleData);
  lruleset_ptr_t _grammar;     // the LRuleSet as a reflected payload (directObjectProperty)
  // in plugs:  seed (int) ; field (GpuComputeImage2D, env-sensitive — DEFERRED) ; time (growth — DEFERRED)
  // out plug:  out (XfNodeGraph)   ── consumed by SweepModule (XfNodeGraph→GpuMesh) downstream
};
```
So the L-system slots into the geometry dataflow graph as `LSystem → Sweep → tbn → render` (+ `slots`→InstanceSet
leaves, `XfNodeGraph`→capsule_chain collider). Plug datatypes on the wire: **`XfNodeGraph`** (NEW, G0a — LSystem out
/ Sweep in), `GpuMesh` (Sweep out, the rest of the graph), `InstanceSet` (leaves), `GpuComputeImage2D` (terrain
field, deferred), scalar/int (seed/params). The skin sinks serialize as ordinary hypermesh / InstanceSet sink
graphdata referencing the derived `XfNodeGraph` — multi-sink, exactly as terrain/hypermesh already do.

### 17.4 DSL op list (`@op` signatures)

```python
# turtle / structure (emit into the XfNodeGraph, move the frame)
segment(len:Expr, rad:Expr, gid:str)->TurtleOp ; pitch/roll/yaw(deg:Expr) ; taper(factor:Expr)
slot(tag:str, gid:str, every:Expr=None) ; spines(...)/bloom(...)  # sugar over slot
# control (serializable combinators)
choose(branches:[(float,TurtleOp)]) ; fork(nmin:int,nmax:int,lean:(float,float),then:TurtleOp) ; when(pred:Expr,a,b=None)
# grammar declarations (family base, not @op)
self.symbol(name,**defaults)->Symbol ; @L.rule(Symbol, when=/until=Expr) ; self.axiom(...) ; self.iterate(depth=,segment_budget=)
# bake seam + skin (skin ops live in hypermesh)
self.derive()->XfNodeGraph ; H.sweep(ng, profile=, radius_attr="rad", by_gid=bool, smooth_joints=bool)->GpuMesh  # NEW skinner
H.capsule_chain(ng)->shapedata ; self.slots(tag)->InstanceSet source
```

### 17.5 The `derive() → XfNodeGraph → H.sweep` seam (bake-time, CPU rewrite → fxv2 skin)

```
LRuleSet(JSON) ─C++ materializer, bake-time─►
  derive():  axiom → apply rules depth× (seeded counter-hash RNG, LExpr param eval, guards; stop at
             segment_budget / no non-terminals) → terminal turtle string → interpret (turtle walk with a
             frame + bracket stack) → emit ↓
  XfNodeGraph (the §11 shared currency, SoA):
     mat4 node_xform[count]   // turtle frame at each joint
     uint parent[count]       // branch topology (fork → children share a parent)
     vec4 attrs[count]        // .x = rad   [extensible: .y age, .z wind-stiffness]
     uint tags[count]         // gid in [20:32) (A1 contract verbatim)
     slots[] = (node_id, local_xform, slot_tag)   // spine/flower attach points
  H.sweep(XfNodeGraph, profile=ncircle(7), radius_attr="rad", by_gid):   // fxv2 compute
     per node: place profile ring oriented by node_xform, scaled by attrs.rad; connect ring(parent)→ring(child)
     into tube quads; fork = fan-connect children to the parent ring (smooth_joints = blend overlapping rings);
     carry tags.gid onto faces → GpuMesh (joins the standard triangulator / gid-BucketDraw / cook / render paths)
  multi-sink: mesh→xgmmodel ; instances(slots)→InstanceSet ; collider→capsule_chain ; lod→decimate+impostor
```
The entire left column is bake-time (INSTALL/SCENE_LOAD); the scene then instances the baked archetype.

### 17.6 The M-sized milestone (build / defer / gate)

**Build now (cholla vertical slice):** (1) **`XfNodeGraph` interchange plug** (= §16 G0a, the prereq);
(2) `LRuleSet` + `LExpr` reflected types + the C++ expr evaluator; (3) **`derive()`** in C++ —
**parametric + stochastic** rewrite (seeded counter-hash RNG reused from terrain scatter) + turtle interpret
→ `XfNodeGraph` (brackets via frame stack; `fork`/`choose`/`when`/`segment_budget`); (4) **`H.sweep`** skinner
(= §16 G0b — XfNodeGraph→GpuMesh tube, n-gon profile, `radius_attr`, `by_gid`; fork = fan-connect,
smooth_joints deferred); (5) Python front-end (`L.*` ops, `S` symbol-state + expr overloading,
`choose`/`fork`/`when`, `@L.rule`; string form optional in M); (6) reuse E.2 InstanceSet for
`instances(slots)` and gid-BucketDraw for `by_gid`.

**Gate:** `CaneCholla` bakes → `XfNodeGraph` → mesh, **round-trips JSON byte-identical** (the hypermesh A.2/A.4
oracle pattern), renders, and scatters as instances; deterministic given seed.

**Deferred:** env-sensitive guards (`S.env.*`, needs the terrain field bridge — §16 G1 env axis);
`phyllotaxis`/`radial` ops (ocotillo/yucca); the SDF-capsule skin path (`H.sdf_capsules`); smooth
branch-junction blending; auto-LOD/impostor; the string-form parser if not done in M.

This is the forests-first head of the §16 order (G0a → G1 rewrite → G0b skin) — the v1 slice **LANDED
2026-06-25** (and the v2 archetype/jitter/wind/organ/forest wave 2026-06-26). Sequencing beyond it was
**resolved by the owner 2026-07-04** (holistic-outcome ordering, racer gate retired as a constraint —
see §15); the reflected-`LRuleSet` milestone is specified in `~/JUL04_GRAMMARS.md` §GR-1.
