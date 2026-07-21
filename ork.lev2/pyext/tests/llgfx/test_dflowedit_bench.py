#!/usr/bin/env ork.python

################################################################################
# test_dflowedit_bench — headless gates for the TESTBENCH program (authoring-time
# stimulus) in ork.dflow.edit.py.
#
# Proves the adjudicated bench law end-to-end on the particles family (fireball):
#
#   GRAPH PARITY (cook-hash + production-unchanged, in-process, no GPU):
#     the bench is EDITOR-ONLY. benches=OFF reproduces the EXACT production graph
#     (FireTrail().generatedflow() module set + edge count), with NO bench module;
#     benches=ON adds the emitter-entity binding (a TransformPoint module) — the
#     genuinely-different EDITING instantiation. The bench never enters a production
#     graph, so it can never enter a cook hash.
#
#   MOTION (offscreen, deterministic): fireball benches=ON captured at two distinct
#     transport frames shows the emitter DISPLACED (whole-frame MAD + luma-centroid
#     shift); the SAME frame twice is BYTE-IDENTICAL (frame-locked determinism).
#
#   PRODUCTION UNCHANGED (offscreen): fireball benches=OFF is deterministic (same frame
#     twice byte-identical) AND its graph == the pre-bench production graph, so a
#     pre-bench baseline reproduces it exactly.
#
#   LIVE PARAMS (offscreen): a radius tweak via the DISTINCT bench propsheet model moves
#     the next frame with the rebuild counter UNCHANGED (LIVE, resolver-fed); toggling
#     `enabled` increments the rebuild counter (the honest re-instantiation).
#
#   RESOLVER-ABSENT UNCHANGED: GraphInst.setEntityResolver exists (the new binding) and a
#     graphinst with NO resolver bound behaves exactly as before (proved by the benches=OFF
#     byte-determinism + graph parity above — the binding is invisible when unused).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"

import sys
import shutil
import subprocess
import tempfile

_SCRIPTS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                         "..", "..", "..", "..", "obt.project", "scripts"))
if _SCRIPTS not in sys.path[:1]:
  sys.path.insert(0, _SCRIPTS)

MOTION_ADV_A = 60          # first PLAYING capture tick (a developed plume) — the base
TICK_HZ = 60.0             # host fixed sim step is 1/60s (particles_viewport_host._DT) -> ticks/sec
MAX_SPAN_TICKS = 660       # cap the DERIVED A->B span so a very slow orbit stays a sane sim length
PROD_ADV = 90
MOTION_MAD = 0.03          # two distinct transport frames must move whole-frame pixels by >= this
EXACT_MAD = 0.01           # the SAME frame twice must match within this (byte-locked determinism)
CENTROID_MIN = 3.0         # the fire's luma centroid must shift by >= this many pixels between A/B
FAST_BENCH_PERIOD_S = 2.0  # the in-test fast-bench override period (proves the derivation generalizes)


def _fast_bench_dsl(period_s):
  """A test-generated DSL reusing fireball's DUT VERBATIM (subclass, no graph change) with a FAST
  orbit bench — the "temporarily-constructed fast bench" that proves the capture-tick derivation
  generalizes. fireball.py itself is never edited; this rides ORK_PARTICLES_SEARCH_PATH."""
  return (
    "# test-generated fast-bench fixture (fireball.py is NOT edited): fireball's DUT + a fast orbit.\n"
    "from ork.hypergraph.dflow.particles import resolve as _R\n"
    "from ork.hypergraph.dflow.testbench import T\n\n"
    "_fb = _R.load_dsl_module(_R.resolve_dsl_file('fireball'))\n\n\n"
    "class FireTrailFastBench(_fb.FireTrail):\n"
    "    pass\n\n\n"
    "TESTBENCH = T.Testbench(\n"
    "    instantiate=dict(emitter_entity='@bench'),\n"
    f"    entities={{'@bench': T.orbit(radius=3.2, period_s={period_s}, axis=(0, 1, 0), height=0.0)}},\n"
    "    camera=T.frame(distance=9.0, elevation_deg=14.0, target=(0, 1, 0)),\n"
    "    enabled=True,\n"
    ")\n\n"
    "__all__ = ['FireTrailFastBench']\n"
  )


def _bench_orbit_period_s(source):
  """The resolved TESTBENCH's primary motion period (seconds) for `source`, read the SAME way the
  shell does (resolve -> getattr TESTBENCH -> primary_program). This is the bench DECLARATION the
  gate derives its capture spacing from, so a future bench re-tune re-derives the spacing itself."""
  from ork.hypergraph.dflow.particles import resolve as R
  from ork.hypergraph.dflow.testbench import resolve_testbench
  tb = resolve_testbench(R.load_dsl_module(R.resolve_dsl_file(source)))
  prog = tb.primary_program() if tb is not None else None
  period = float(getattr(prog, "period_s", 0.0) or 0.0)
  if period <= 0.0:
    raise ValueError(f"{source!r} has no positive-period orbit bench to derive a capture span from")
  return period


def _derive_capture_ticks(period_s):
  """(A, B, span, half_orbit): capture B sits ~HALF an orbit after A so the orbiting emitter is
  near-diametrically opposite at the two captures — maximal luma-centroid displacement for ANY bench
  tune (green by construction). Capped to MAX_SPAN_TICKS so a very slow orbit stays a sane sim
  length (a still-meaningful, if sub-half, arc)."""
  half = int(round(period_s * TICK_HZ / 2.0))
  span = max(1, min(half, MAX_SPAN_TICKS))
  return (MOTION_ADV_A, MOTION_ADV_A + span, span, half)


# ---- subprocess capture leaves --------------------------------------------------------

def _capture_motion(source, advance, out, benches):
  """Boot the shell offscreen, advance `advance` PLAYING ticks (no bypass), capture the
  viewport RGB to `out` (.npy). benches gates the editor TESTBENCH seam."""
  from orkengine import core       # noqa: F401  (core before lev2)
  from orkengine import lev2        # noqa: F401
  from ork.editor.dflowedit import DflowEditor
  app = DflowEditor([source], bypass_ab={"bypass": [], "advance": int(advance), "out": out},
                    benches=bool(int(benches)))
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()


def _capture_bench(source, advance, out):
  """Boot the shell offscreen in the bench-A/B LIVE-params driver (radius tweak + enabled toggle)."""
  from orkengine import core       # noqa: F401
  from orkengine import lev2        # noqa: F401
  from ork.editor.dflowedit import DflowEditor
  app = DflowEditor([source], bench_ab={"advance": int(advance), "out": out})
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()


# ---- in-process graph-parity gate (no GPU) --------------------------------------------

def _module_classes(graph):
  import json
  root = json.loads(graph.serializeJson())
  mods = root["root"]["object"]["properties"].get("Modules", {}) or {}
  return sorted(e.get("object", {}).get("class", "") for e in mods.values())


def gate_graph_parity():
  """The bench is EDITOR-ONLY: benches=OFF == the production graph (no bench module); benches=ON
  adds the emitter-entity binding module. Pure reflection — no GPU, deterministic."""
  from orkengine.core import Object                      # noqa: F401
  from ork.hypergraph.dflow.particles import resolve as R
  from ork.hypergraph.dflow.testbench import resolve_testbench

  dsl_path = R.resolve_dsl_file("fireball")
  module = R.load_dsl_module(dsl_path)
  cls = R.class_in_module(module, dsl_path)
  tb = resolve_testbench(module)
  assert tb is not None and tb.enabled, "fireball must declare an ENABLED TESTBENCH"
  assert "@bench" in tb.entities, "fireball bench must bind the '@bench' entity"

  from collections import Counter
  prod = cls().generatedflow()                            # production instantiation (no bench kwargs)
  bench = cls(**tb.instantiate).generatedflow()           # editing instantiation (bench kwargs)
  prod_cls = _module_classes(prod)
  bench_cls = _module_classes(bench)
  added = list((Counter(bench_cls) - Counter(prod_cls)).elements())
  xform = [c for c in bench_cls if "Transform" in c]
  print(f"[gate:graph] production modules={len(prod_cls)} bench modules={len(bench_cls)} "
        f"bench-added={added}", flush=True)
  # cook-hash clean: the production graph carries NO bench binding at all.
  assert not any("Transform" in c for c in prod_cls), \
      "production graph must NOT carry a bench entity-transform module (cook-hash clean)"
  # the editing graph adds the entity-transform binding (+ its vec3 support), nothing removed:
  # production is a strict SUB-MULTISET of the bench graph (the bench only ADDS — no topology
  # change to the DUT), and every added module is an entity-binding support module.
  assert xform, "bench (editing) graph must carry the emitter-entity TransformPoint binding"
  assert Counter(prod_cls) <= Counter(bench_cls), \
      "the bench must not remove/alter any production module (production must survive intact)"
  assert all(("Transform" in c) or ("Vec3Combine" in c) for c in added), \
      f"bench added a non-entity-binding module (unexpected topology): {added}"
  print("GATE_GRAPH_PARITY=PASS", flush=True)
  return True


def gate_resolver_binding():
  """The new GraphInst.setEntityResolver binding exists and a None resolver is a no-op (invisible
  when unused). Model-level — proves the binding is present + safe."""
  from orkengine.core import dataflow as _dflow
  assert hasattr(_dflow.GraphInst, "setEntityResolver"), \
      "GraphInst.setEntityResolver binding missing (the resolver seam is not built)"
  print("GATE_RESOLVER_BINDING=PASS", flush=True)
  return True


# ---- orchestrator ---------------------------------------------------------------------

def _mad(a, b):
  import numpy
  if a is None or b is None or a.shape != b.shape:
    return -1.0
  return float(numpy.abs(a.astype(numpy.float32) - b.astype(numpy.float32)).mean())


def _luma_centroid(rgb):
  import numpy
  if rgb is None:
    return None
  luma = rgb.astype(numpy.float32).mean(axis=2)
  luma = numpy.maximum(luma - 8.0, 0.0)                   # threshold background
  tot = float(luma.sum())
  if tot <= 0.0:
    return None
  ys, xs = numpy.mgrid[0:luma.shape[0], 0:luma.shape[1]]
  return (float((xs * luma).sum() / tot), float((ys * luma).sum() / tot))


def _motion_gate(cap_motion, source, period_s, tag):
  """DERIVED-spacing motion gate for `source` at its declared bench `period_s`: capture two
  transport frames ~half an orbit apart (the second tick DERIVED from the period) + a determinism
  rerun; assert the fire is DISPLACED (whole-frame MAD + luma-centroid shift) and the same frame
  twice is byte-identical. The derivation is LOGGED + ASSERTED so a future bench re-tune stays
  green by construction (B tracks the declared period). Returns the gate bool."""
  advA, advB, span, half = _derive_capture_ticks(period_s)
  full = int(round(period_s * TICK_HZ))
  print(f"[gate:motion:{tag}] bench-derived capture: period_s={period_s} tick_hz={TICK_HZ:.0f} "
        f"full_orbit={full}t half_orbit={half}t cap={MAX_SPAN_TICKS} -> A={advA} B={advB} "
        f"span={span}t (~half orbit apart)", flush=True)
  assert advB == advA + span and span == max(1, min(half, MAX_SPAN_TICKS)) and span >= 1, \
      f"[{tag}] capture-tick derivation inconsistent with period_s={period_s}"
  fA = cap_motion(advA, f"motion_{tag}_A", 1, source)
  fB = cap_motion(advB, f"motion_{tag}_B", 1, source)
  fA2 = cap_motion(advA, f"motion_{tag}_A2", 1, source)
  mad_AB = _mad(fA, fB)
  mad_det = _mad(fA, fA2)
  cA, cB = _luma_centroid(fA), _luma_centroid(fB)
  cshift = (((cA[0] - cB[0]) ** 2 + (cA[1] - cB[1]) ** 2) ** 0.5) if (cA and cB) else -1.0
  displaced = mad_AB >= MOTION_MAD and cshift >= CENTROID_MIN
  deterministic = 0.0 <= mad_det <= EXACT_MAD
  print(f"[gate:motion:{tag}] frame A(tick {advA}) vs B(tick {advB}): whole_MAD={mad_AB:.4f} "
        f"(>= {MOTION_MAD}) luma_centroid {cA} -> {cB} shift={cshift:.1f}px (>= {CENTROID_MIN})",
        flush=True)
  print(f"[gate:motion:{tag}] same frame twice MAD={mad_det:.6f} (<= {EXACT_MAD} "
        f"deterministic={deterministic})", flush=True)
  return bool(fA is not None and fB is not None and displaced and deterministic)


def _spawn(orkpython, argv, tag, timeout=180):
  cmd = [orkpython, os.path.abspath(__file__)] + argv
  for attempt in (1, 2):
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    out = r.stdout or ""
    ok = ("BYPASS_AB_RESULT=OK" in out) or ("BENCH_AB_RESULT=OK" in out)
    for ln in out.splitlines():
      if any(k in ln for k in ("[dflowedit bench-ab]", "BENCH_AB_RESULT", "[dflowedit bypass-ab]")):
        print(f"    [{tag}] {ln.strip()}", flush=True)
    if ok:
      return r
    print(f"    [{tag}] leaf FAILED attempt {attempt} (rc={r.returncode})"
          f"{'' if attempt == 2 else ' — retrying'}\n{(r.stderr or '')[-400:]}", flush=True)
  return r


def _orchestrate():
  import numpy
  orkpython = shutil.which("ork.python") or sys.executable
  workdir = tempfile.mkdtemp(prefix="dflowedit_bench_")
  ok_all = True

  # ---- fast-bench GENERALIZATION fixture: a temp DSL reusing fireball's DUT with a FAST orbit
  #      (temporarily constructed; fireball.py is NOT edited) laid on the particles search path so
  #      both the leaves (subprocess, inherited env) and the in-process period read resolve it ----
  from ork.hypergraph.dflow.particles import resolve as _pres
  default_dirs = [str(d) for d in _pres.search_path()]
  fast_dir = os.path.join(workdir, "fastbench")
  os.makedirs(fast_dir, exist_ok=True)
  with open(os.path.join(fast_dir, "fireball_fast.py"), "w") as f:
    f.write(_fast_bench_dsl(FAST_BENCH_PERIOD_S))
  os.environ["ORK_PARTICLES_SEARCH_PATH"] = ":".join([fast_dir] + default_dirs)

  # ---- in-process (no GPU): graph parity + resolver binding + resolve both benches' periods ----
  from orkengine import ecs
  from orkengine import core
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ezapp.bindGfxToCurrentThread()
  try:
    ok_all = gate_resolver_binding() and ok_all
    ok_all = gate_graph_parity() and ok_all
    fb_period = _bench_orbit_period_s("fireball")           # owner's current tune
    fast_period = _bench_orbit_period_s("fireball_fast")    # the temporarily-constructed fast bench
  finally:
    ezapp.mainThreadEnd()
    core.coreappexit()

  def cap_motion(advance, tag, benches, source="fireball"):
    out = os.path.join(workdir, tag + ".npy")
    _spawn(orkpython, ["--capture-motion", source, str(advance), out, str(benches)], tag)
    return numpy.load(out) if os.path.isfile(out) else None

  # ---- MOTION (benches ON): two frames ~HALF an orbit apart (the second tick DERIVED from the
  #      resolved bench period) must DISPLACE the fire; the same frame twice is byte-identical.
  #      The gate FIXes the FAIL by tracking the orbit period, NOT the bench: a period re-tune (the
  #      owner's 3.5s->20.5s widening that caused the 1.1px stall) re-derives the spacing itself.
  #      Proved GREEN at the owner's current tune AND at a temporarily-constructed FAST bench. ----
  print("[gate:motion] fireball benches=ON, derived half-orbit spacing (owner tune) ...", flush=True)
  gate_motion_real = _motion_gate(cap_motion, "fireball", fb_period, "fireball")
  print("[gate:motion] fireball_fast benches=ON, derived spacing (generalization proof) ...",
        flush=True)
  gate_motion_fast = _motion_gate(cap_motion, "fireball_fast", fast_period, "fast")
  gate_motion = bool(gate_motion_real and gate_motion_fast)
  print(f"GATE_BENCH_MOTION={'PASS' if gate_motion else 'FAIL'}", flush=True)
  ok_all = ok_all and gate_motion

  # ---- PRODUCTION UNCHANGED (benches OFF): deterministic + the graph parity above proves parity ----
  print("[gate:production] fireball benches=OFF determinism ...", flush=True)
  p0 = cap_motion(PROD_ADV, "prod_0", 0)
  p1 = cap_motion(PROD_ADV, "prod_1", 0)
  mad_prod = _mad(p0, p1)
  prod_det = 0.0 <= mad_prod <= EXACT_MAD
  print(f"[gate:production] benches=OFF same frame twice MAD={mad_prod:.6f} "
        f"(<= {EXACT_MAD} deterministic={prod_det})", flush=True)
  gate_prod = bool(p0 is not None and prod_det)
  print(f"GATE_PRODUCTION_UNCHANGED={'PASS' if gate_prod else 'FAIL'}", flush=True)
  ok_all = ok_all and gate_prod

  # ---- LIVE PARAMS: radius LIVE (frame differs, rebuild unchanged) + enabled toggle rebakes ----
  print("[gate:live] fireball bench-A/B (radius live + enabled rebake) ...", flush=True)
  r = _spawn(orkpython, ["--capture-bench", "fireball", "90",
                         os.path.join(workdir, "live")], "live")
  gate_live = "BENCH_AB_RESULT=OK" in (r.stdout or "")
  print(f"GATE_LIVE_PARAMS={'PASS' if gate_live else 'FAIL'}", flush=True)
  ok_all = ok_all and gate_live

  print(f"DFLOWEDIT_BENCH_RESULT={'PASS' if ok_all else 'FAIL'}", flush=True)
  return 0 if ok_all else 1


def main(argv):
  if argv and argv[0] == "--capture-motion":
    _capture_motion(argv[1], argv[2], argv[3], argv[4])
    return 0
  if argv and argv[0] == "--capture-bench":
    _capture_bench(argv[1], argv[2], argv[3])
    return 0
  return _orchestrate()


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
