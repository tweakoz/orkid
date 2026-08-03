# SKYLIGHT — Shadows Work Complete (2026-07-22)

## ADDENDUM 2026-07-23 — mac integration + first static sun (m5max lane)

### DONE
- Tranche merged into `toz-2026-jul22` (clean; zero file overlap with the W·M /
  kiva / singularity work on that branch).
- **macOS: Metal argument buffers are REQUIRED.** Without AB, Metal caps
  samplers at 16 per fragment stage; the merged sun-cascade sampler pushed
  heavy generated forward fragments (forest terrain: impostor atlas + IBL +
  cookies + sun) over the cap → MSL compile error → pipeline create
  VkResult -3 (the 5090 has no such cap — why that box never saw it).
  AUTHORITATIVE fix = shell env `MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1`
  exported by `obt.project/scripts/init_env.py` (MoltenVK snapshots config at
  dylib load — in-process setenv is provably too late); engine warns loudly
  when the env is missing and `_createPipelineSSBO` now dumps its create-state
  on failure. Historical default-off (SPIRV-Cross SSBO concern) no longer
  reproduces; SSBO-pull graphics pipelines battery-proven green under AB.
- **Sun aim CONJUGATE bug (the big one): the DSL sun pointed BELOW the
  horizon.** An entity-orientation quat reaches `Light::direction()`
  (`worldMatrix().zNormal()`) applying the INVERSE of the intended rotation —
  declared el45/az285 measured travel `(0.966, +0.183, 0.183)` = R(q)ᵀ·z.
  Fixed in `_helpers.elevation_azimuth_quat` (conjugate composition,
  probe-verified exact). Live symptoms this explained in scn_swest: unlit
  terrain (N·L ≤ 0 under up-tilted light), "shadow projected as if the sun is
  below the horizon", flipped-looking shadows. NOTE: every prior HMD verdict
  (scn_sun_test / scn_forest_sun orbits) rendered under the broken pose — the
  orbit scenes' look CHANGES with this fix.
- scn_swest gains the repo's first STATIC sun (`self.sun(elevation=45,
  azimuth=285, 4K cascades)` — owner-tuned from the initial 6:30 PM/18°).
- Full battery green on m5max/MoltenVK: 3 committed gates + sun_test +
  forest_sun players + swest perceptual vet (shadow coherence, no acne /
  peter-panning / seams in stills). scipy added to the venv (masked-dpp gate
  dep; its absence masqueraded as a crash).

- Sun shadow FILTER CHAIN (07-23 late, owner: "better, not 100%"): manual
  bilinear-weighted comparisons (shadlang has no comparison samplers) + fixed
  tent 3x3 (rotation removed — grain on a smooth signal) + normal-offset from
  the DERIVATIVE geometric normal (shading normal warbles with normal maps) +
  derivative-matched radius (minification, cap 12 texels) + PCSS contact
  hardening (sparse 3x3 blocker search w/ fully-lit early-out; penumbra =
  separation x 2·tan(sun angular radius), physical constant inlined — the
  ARTISTIC softness knob wants a DirectionalLightData slot when lane B grows
  ublk_sun). Player devkeys viewer-look defaults: saturation 0.8, exposure
  0.75 (owner-tuned).

### PERF DATAPOINTS 2026-07-23 (m5max gauge: scn_swest in-village
### SWEST_SPAWN=822,150,530, offscreen-forever, post-O2 remesh)
- no sun: **265 FPS** (the swest O2 perf bar of 250+ is MET ex-sun)
- sun light, shadows off (`SWEST_SUNSHADOW=0`): 259 FPS — sun shading ≈0.1 ms
- sun + 3×4K cascades + PCSS filter: 187 FPS — **shadow stack ≈1.5 ms/frame**
- levers for the shadow-perf slice (all measured against this gauge): A4
  round-robin far-cascade amortization (queued in lane A), per-cascade caster
  cull (the cull-reuse correctness fix doubles as perf — depth passes
  currently draw EYE-culled args), cascade count/map-size per-scene knobs,
  PCSS search/filter tap budget. Scene A/B toggles committed in scn_swest
  (`SWEST_NOSUN` / `SWEST_SUNSHADOW`).

### AMD RADV COVERAGE 2026-07-23 (third-vendor battery @ 3d314f3e3: 6/6 PASS)
Build + all three sun/shadow gates + sun_test/forest_sun smokes green on
Ubuntu 26.04 / RADV; [FWD:SUN] aim probes correct both scenes (conjugate fix
cross-platform-verified). TWO RADV-only visual anomalies flagged (vet triage
in flight; timing/asset-not-ready vs deterministic discrimination):
- prologue-gate ground magenta cast + specular sparkle — only structural
  delta in that gate = the reflection-probe absorbed sub-pass (IBL/probe
  capture on RADV suspected; lane-C-adjacent).
- scn_sun_test sky solid black while forest_sun sky is correct — envmap /
  radiance-map-async suspected (desert4k logs as loaded).
Fail-loud gaps surfaced by the batteries (bundle as one "oracles fail loud"
slice): player --snapshot writes dead-black + exits rc=0 when settle-capped
cold (seen 3x; should drain radiancemaps + exit nonzero); OIIO
ImageOutput::create silently no-ops on extension-less paths; instanced gates
must assert instance multiplicity (cluster count), not just nonblack.

### DONE 07-23 evening (coordinator session)

Three slices landed on `toz-2026-jul23`, each: implementer lane → line review →
gate battery → ff-merge → rebuild → post-merge re-gate:
- **Cascade shadow cull** `098644ca6` (+ `08bcced02` canary producers committed) —
  NEXT-1, battery 5/5 + re-gate 6/6.
- **compose() conjugate fix** `56a55796a` — NEXT-2, owner rename-method, re-gate 8/8.
- **Headless DMVR per-eye capture** `991f310be` — known-issue 6 closed; DMVR
  observables machine-gateable from here on.
  ⚠️ LANDING INCOMPLETE (owner paused for the night): merged ff to
  `toz-2026-jul23` and lane-verified (implementer battery: build green, new
  gate PASS + negative-detection proven, llgfx 4/4, canaries EXACT), but the
  POST-MERGE rebuild was killed mid-run on owner instruction — staging is in a
  PARTIAL build state (cmake cache wiped + reconfigure interrupted at
  `~/.staging-jul03/builds/orkid/.build`) and the post-merge re-gate has NOT
  run. FIRST ACTIONS NEXT SESSION: `ork.build.py` from the main tree (grep log
  for " error:"), then the full battery (4 gates + test_dmvr_capture_gate +
  canaries, warm) before any new lane.
Recon docs produced (all in `~/`): `JUL23_compose_fix.md` (now historical record),
`JUL23_coldjit_recon.md`, `JUL23_teardown_recon.md` (thread-affinity root-cause —
would flip all gates to required-rc0), `JUL23_dmvr_capture_recon.md`,
`JUL23_laneB_prep.md` (lane B dispatch-ready). Fleet unused (owner directive).
OWNER GAUGE OUTSTANDING: HMD head-pose neutrality (compose fix removed the openxr
conjugate) + swest/forest look; A3 re-triage (NEXT-3) now truly unblocked.

### NEXT (priority order)
1. **Cascade cull-reuse bug — ✅ LANDED 07-23** (commit `098644ca6`, battery 5/5
   incl. byte-identity canaries EXACT — eye path proven untouched; new committed
   gate `test_shadow_cull_gate.py`, detection revert-proven). Filed remainders:
   terrain keeps eye-culled cascades (culls via `_passes`, separate slice);
   python `setupMeshRender` shadow wiring (safe eye-set degrade); spot shadows
   still eye-culled; `updateWithMicroMesh` boundRadius()==0. Original finding
   below for the record. Recon
   reshaped the finding: ONLY the GPU-culled ComputeDrawable family is
   affected (hypermesh scatter/forest, terrain, InstancedRigidPrimitiveDrawable
   — the cull STREAM-COMPACTS instances into a single persistent per-drawable
   buffer set; sole onPreRender call site `scenegraph.cpp:243`). ModelDrawable
   re-culls CPU-side per pass (fine); InstancedModelDrawable never culls at
   all (separate quirk, left as-is). Adjudicated fix: ONE extra shadow buffer
   set per drawable + ONE union-frustum sun cull dispatch per frame in the
   prologue, CPD-keyed consumption; spot path stays eye-culled (filed). New
   gate: off-view caster shadow-persistence camera-pair oracle.
2. **Conjugate convention — ✅ LANDED 07-23** (commit `56a55796a`, owner
   rename-method; spec+audit `~/JUL23_compose_fix.md`). compose() coefficients
   corrected to the active rotation (was R(conj q)); atomic bundle removed all
   three grown compensations (VR `_poseConjugate` machinery, openxr
   `xrPoseToFmtx4` conjugate, bullet quat converters' inverses) + reverted the
   DSL `elevation_azimuth_quat` conjugate (net-zero for declared suns,
   probe-proven ≤1.3e-5). Unit tests de-blinded (were transpose-blind) + new
   L-A/L-C law tests, fail-before/pass-after proven. Post-merge re-gate 8/8
   (canaries byte-EXACT — neither routes a quat through composed()).
   OWNER GAUGE OUTSTANDING: VR head-pose neutrality in HMD; swest/forest look.
   FILED: pyext `mtx4.xNormal`/`zNormal` bindings SWAPPED
   (pyext_math_la.inl:546-548, live-confirmed); non-uniform-scale
   compose==compose2 untested (uniform proven); skeleton.cpp:108-121
   decompose→compose round-trip is now a true identity (dead code anyway).
3. **A3 visual-issues triage RESCOPED:** re-triage AFTER the conjugate fix —
   the below-horizon sun likely caused or masked much of what was noted;
   re-verify sun_test/forest_sun look (HMD gauge invalidated).
4. scn_swest sun finals: elevation decision (18° golden hour vs 45°), and the
   skybox-vs-DSL-sun bearing mismatch (desert4k's baked glow ≠ shadow
   direction; lane B's sky-source is the real fix, interim = aim the DSL sun
   at the baked glow bearing).
   DATA 07-23 (vet, CPU decode of desert4k.xir, convention round-trip-verified
   vs ps_forward_skybox_mono): baked sun = DSL **elevation ≈10°, azimuth ≈88°**
   (az ±3°, el ±4°; broad 13°×6° LDR glow, no HDR disc — asset is 8-bit
   RGBA8). Declared el45/az285 disagrees ~34°/el, ~160-200°/az. While the
   baked sky is in use, coherence dictates ≈el10/az88 — the 45° look needs
   lane B's procedural sky. Interim one-liner NOT applied (owner look call).
   Also landed 07-23: lane B brief-prep with re-verified anchors
   (`~/JUL23_laneB_prep.md`).
4b. **Ready-to-brief small slices** (recon complete, implementation-ready, in
   recommended order — each has a full root-cause doc):
   - **Teardown thread-affinity fix** (`~/JUL23_teardown_recon.md`): mirror the
     wave_main/wave_parallel split from `Subsystem::shutdownChildren`
     (subsystem.cpp:255-347) into `_shutdownSubsystemsInWaves`
     (application.cpp:878-942) + `StandardSceneGraphComponent._onGpuExit` +
     the `application.py:756` missing-parens bug. Gate: offscreen scene exits
     rc=0; then migrate ALL gates from tolerated-139 to required-rc0.
   - **pyext mtx4.xNormal/zNormal swapped bindings**
     (pyext_math_la.inl:546-548; live-confirmed twice). Small fix + audit the
     few Python callers that may have compensated.
   - **Cold-JIT duplicate-name slice** (`~/JUL23_coldjit_recon.md`): it/it2
     typo fix (dead assert), rich warning + ONE instrumented cold run
     (ORKID_DISABLE_SHADER_CACHE=1) to capture the colliding name, import-path
     canonicalization, cold absence-of-error gate.
   - Cull-fix remainders: terrain shadow-cull coverage (culls via `_passes`,
     not `_perViewCompute`); python `setupMeshRender` shadow wiring; player
     `--movie` instrumented run (settle machine vs DMVR async).
   - UNEXPLAINED: scn_forest_sun fps spread (implementer ~204 vs gate-runner
     ~6.6 via same viewer offscreen; swest runs ~143) — check before perf work.
5. Shadow-quality remainder (owner: filter chain "better, not 100%"):
   candidates = denser blocker search (thin-caster penumbra flicker), per-tap
   receiver-plane depth bias, penumbra continuity across cascade splits,
   EVSM/moment maps as the structural successor (prefiltered + mippable);
   artistic sun-softness knob on DirectionalLightData (needs ublk_sun slot).
6. Carried from the JUL22 list: spot 256² quality pass, DppZBias owner call,
   cold-first-JIT lighting corruption, offscreen teardown segfault.
7. Lanes B (Hillaire atmosphere) / C (SH probes) queued as spec'd.

### Operational notes for session resumption

- **Tree/branch**: main tree `<orkid>` on `toz-2026-jul23`; owner may
  rename/switch branches between sessions — check `git status -sb` first. Lane
  worktrees live at `~/projects/orkid-lanes/<slice>` (branch `lane-<slice>`), retired
  (removed + branch deleted) after ff-merge. Coordinator commits; implementers never do.
- **BUILD-CACHE GOTCHA**: the real orkid cmake cache is
  `~/.staging-jul03/builds/orkid/.build` (NOT `<tree>/.build`). A lane build repoints
  its source dir; when switching back to the main tree, `rm` that dir's
  `CMakeCache.txt` + `CMakeFiles` and rebuild. `ork.build.py` rc can lie — grep the
  log for `" error:"`.
- **Gate-run env laws**: run from the tree under test; PREPEND
  `$OBT_STAGE/lib` to the ambient `LD_LIBRARY_PATH` (replacing it outright breaks the
  interpreter/Vulkan loader); UNSET `ORKID_VR_DRIVER` (ambient shell exports openxr);
  identity captures run WARM (twice, score second). All gates emit `TESTVERDICT=`
  BEFORE teardown; rc=139 after the verdict is defect D1, tolerated
  (`PASS_WITH_TEARDOWN_BUG`) until the teardown fix lands. [RESOLVED 2026-07-24] The
  `hashlib`-before-`orkengine` libcrypto clash is FIXED at the root (ork.build
  `6f051bf`: Linux CPython `_hashlib`/`_ssl` now link the OBT-staged openssl with
  `--with-openssl-rpath`, one OpenSSL deployment per process) — the
  `LD_PRELOAD=$OBT_STAGE/lib/libcrypto.so.3` workaround is OBSOLETE; import order no
  longer matters. Requires the staged python dep rebuilt (`obt.dep.build.py python
  --force`, ~34s, pyvenv survives) + orkid rebuild.
- **Byte-identity canaries**: producers are COMMITTED
  (`ork.lev2/pyext/tests/llgfx/canary_{spot_shadow,probe}.py`); blessed shas above;
  blessed artifact copies + original scratch scripts preserved at
  `~/projects/orkid-baselines/`.
- **Fleet**: owner directive 2026-07-23 — do NOT use obtnet; everything runs on this
  machine, serialized behind whichever lane owns it.
- **C++ DB**: repopulate before briefing lanes (`ork.cpp.db.build.py --all`; DB at
  `$OBT_STAGE/cpp_db_v2_orkid.db`).
- **Home-dir docs**: `~/JUL22_skylight.md` (milestone spec),
  `~/JUL23_{compose_fix,coldjit_recon,teardown_recon,dmvr_capture_recon,laneB_prep}.md`.

Summary of the sun/shadow tranche of the SKYLIGHT milestone (`~/JUL22_skylight.md`),
landed 2026-07-22 on branch `toz-2026-jul22-atmos`. Five slices, five commits, all
gated by committed tests; DMVR behavior owner-verified in HMD.
Repo copy: `ork.dox/gfx/SKYLIGHT_SHADOWS_JUL22.md`.

## Landed commits (oldest first)

| Commit | Slice | What |
|---|---|---|
| `243f62f0b` | S0 | Frame prologue — view-independent render work (light enum, shadow maps, probe captures) runs ONCE per composited frame instead of once per eye (DMVR) / per viewport. `renderPrologue` virtual on RenderCompositingNode called from `NodeCompositingTechnique::assemble`; GetTargetFrame-keyed dedup (multi-viewport legitimate, adjudicated); loud staleness assert in the color pass; `Scene::onFramePrologue` hook chain for future system-level frame work (sky time-of-day, probe scheduling). |
| `1aba92c7f` | Lane A core | Dynamic directional sun end-to-end: directional bucket in light enumeration (first shadow-caster = THE sun); dedicated `ublk_sun` real UBO (4 reserved cascade mat4s + splits + params); cascades rendered in the prologue — practical splits, rotation-stable bounding-sphere fit, MANDATORY texel snapping, runtime count 2/3 (storage 4), dedicated 2048² Z32F cascade TextureArray; shader sun GGX/Smith/Fresnel gated on `has_sun` (sunless scenes byte-identical, proven); cascade selection vs shared mono fit basis (identical both DMVR eyes); 3×3 PCF + slope-scaled bias + per-fragment dither; all knobs reflected on DirectionalLightData (A8); pyext DynamicDirectionalLight. |
| `78bde6078` | DSL sun | `self.sun(...)` scene-DSL declaration (SunMixin) — zero C++ needed (LightData-as-drawable through the generic SceneGraphComponent path; first DSL scenes to exercise it); `_sun_orbit.py` PythonComponent (orbiting sun); `scn_sun_test` (walkable scatterhills terrain + walker + dropped boulder occluders + 60s orbit sun); `scn_forest_sun` (forest + sun, 5-line subclass). |
| `e72909221` | S0.1 | Restored spot shadow rendering (S0 regression D4: prologue CPD had no layer set → depth passes enqueued zero renderables; masked by an unlit receiver in the blessed capture) + spot PCF NDC v-flip (D5: latently y-mirrored). Prologue gate upgraded to lit-receiver crop-ratio oracle with EMPIRICALLY PROVEN detection (revert test). Baselines re-blessed. |
| `b79aae656` | A3 | Masked (alpha-tested) depth prepass, both material paths: stock PBRMaterial MASKED technique permutations (dedicated ublk/sampler blocks — shared PBR blocks corrupt frame state in depth passes) + ptex3d codegen emitting an opacity-subgraph-only depth fragment (LeafProc `alpha_cutout=0.5`, 19 SSA lines). ALSO fixed two pre-existing Vulkan blockers: pipeline layout-bits registry (pipelines leaked across incompatible attachment layouts) and rendering-info sourced from impl-attached buffers (texture-array-slice RTGs — i.e. the cascades — previously built pipelines with `depthAttachmentFormat=UNDEFINED`; NO depth-pass fragment shader ever executed before this). |

## Owner verdicts

- Lane A cascades in VR (`scn_sun_test --vr`, orbiting sun over walkable terrain):
  **"shadows look good in VR"** — moving-sun stability, cascade transitions, eye
  parity all passed the HMD gauge.
- A3 foliage shadows (`scn_forest_sun --vr`): **works; visual issues noted** —
  owner called A3 done with known visual issues outstanding (not yet
  characterized/triaged; candidates: dapple aliasing, cutoff tuning, bias/dither
  knobs, cascade-split visibility over foliage). Follow-up triage belongs to a
  vet/tech-artist pass when the owner wants it.

## Committed gates (permanent coverage)

- `test_fwd_prologue_gate.py` — prologue contract + spot shadow (lit receiver,
  crop-ratio oracle, revert-proven detection) + probe.
- `test_sun_cascades_gate.py` — sun shadow ratio oracle at cascade counts 3 AND 2
  (live switch in one process).
- `test_masked_dpp_gate.py` — shadow-isolating live-toggle oracle,
  enclosed-hole detection (stub-proven).
- Gate protocol law: identity canaries run WARM (twice, score second) —
  cold-JIT changes fixed-frame captures.

## Blessed baselines (current)

- spot_shadow: sha256 `38f01f4e052d71debe4de162086ed4ff9cfad95c84243af467c00ba61060e83d`
- probe: sha256 `3f3845cc8365441ebe353f0636948283e1e315c4feddc9ec83e2d588cd20c9a8`
- Producers COMMITTED 2026-07-23 (were session-scratch): `ork.lev2/pyext/tests/llgfx/
  canary_spot_shadow.py` / `canary_probe.py` (warm protocol; blessed artifact copies +
  originals preserved at `~/projects/orkid-baselines/`).

## Known issues / filed (spec §5b + A3 findings; owner adjudication where noted)

1. A3 visual issues (owner-observed in VR, uncharacterized) — triage pending.
2. `DppZBias` has NEVER biased depth — plain DPP declares `gl_FragDepth` as a
   location-0 color output; depth is fixed-function only. Fix changes all
   blessed pixels (full re-bless). OWNER CALL.
3. `_createFxPipeline` failure path derefs null `_shader` for un-gpuInit-ed
   materials (crash, fail-loudly violation) — small fix slice.
4. Cold-first-JIT transient lighting corruption ("duplicate named object") —
   dodged by warm gate protocol; will bite first-boot users.
5. D1: offscreen ComponentizedApplication teardown segfault (pre-existing,
   reproduces on unmodified upstream) — lifecycle slice.
6. ✅ CLOSED 2026-07-23: headless DMVR per-eye capture landed (see NEXT list) —
   `DualMonoVrOutputNode::downsampledEyeRtGroup(left)` + committed
   `test_dmvr_capture_gate.py` (stereo-delta oracle, detection proven). DMVR
   observables are now machine-gateable. Remaining sub-items from the recon
   (`~/JUL23_dmvr_capture_recon.md`): player offscreen `--movie` 0-frames
   (suspected settle-state machine vs DMVR async work, needs one instrumented
   run); the old "SGVP never composites" leg is D2's onDraw footgun; and the
   prologue gate's "stalls after one frame" claim was a MISDIAGNOSIS
   (comment truth-passed in the gate file).
7. PythonComponentData has no param channel (ScriptFile only) — orbit period is
   a module constant; future ECS slice (sky time-of-day scripting wants it too).
8. Spot shadows: single 256² default via cookie path (untouched this tranche) —
   quality pass deferred.

## Remaining in lane A (queued)

- A4: far-cascade round-robin update amortization (perf, lag-tolerant per L3).
- Texel-snap orbit-pair stability gate (G2) — owner HMD gauge stands in for now.
- PROFILER=ON build for GPU-timer perf evidence (deferred; needed for the
  milestone perf budget).

## Next major lanes

- Lane B — Hillaire atmosphere (transmittance/multi-scatter/sky-view LUTs +
  aerial-perspective froxels), layered animated cloud shells (NO raymarch),
  per-scene sky-source option, IBL via equirect snapshot + budget-sliced
  refilter. The sun gains its actual disc + correct color/transmittance.
- Lane C — SH probe pillar (L2 projection compute→SSBO after cubemap-in-compute
  smoke, multi-probe blend, SH replaces flat ambient in procedural mode).
- GI addendum (spec §6b): probe-grid GI (C2) + multi-lightmap baked option.
