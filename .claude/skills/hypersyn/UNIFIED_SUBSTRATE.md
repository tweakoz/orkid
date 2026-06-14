# HyperSyn Unified Procedural Substrate — Feasibility & Design

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
