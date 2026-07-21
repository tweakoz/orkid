#!/usr/bin/env ork.python

################################################################################
# test_dflowedit_bypass — DETERMINISTIC render A/B gate for the particles family's
# real BYPASS semantics in ork.dflow.edit.py (JUL13_DFLOW E7 follow-up).
#
# Proves that bypassing a particle module via the editor MODEL API (the exact canvas
# path: node_model.set_bypassed + host rebake) genuinely reshapes the RUNNING sim — not
# merely bumps a rebuild counter. Each capture is a FRESH offscreen process: the C-library
# rand() starts at its default seed and the ONLY per-tick rand() consumer is the emitter
# (the chain forces + the renderer consume none), so two processes that differ ONLY in
# bypass emit the SAME particle stream — the whole-frame pixel MAD isolates the bypass
# EXACTLY, and an empty bypass reproduces the baseline bit-for-bit.
#
# GATES (all headless / offscreen via ork.python):
#   A. single-renderer (fireball, POOL→EMIT→BUOY→DRAG→TURB→CURL→SPRI):
#      A1 baseline vs bypass(TURB,CURL)     MAD ABOVE threshold  (sim genuinely changed)
#      A2 baseline vs un-bypass (rerun)     MAD == 0             (exact restore)
#   B. multi-terminal (two renderers, sprites A + streaks B on one chain):
#      B1 baseline vs bypass(A)             A's pixels gone, B intact (region MAD)
#      B2 baseline vs bypass(A,B)           empty (near-zero lit) render, no crash
#      B3 baseline vs un-bypass             MAD == 0             (exact restore)
#
# Determinism note: whole-frame MAD is an 8-bit mean-abs pixel delta. The baseline-vs-
# rerun MAD is expected EXACTLY 0.0 (same seed + same graph + same tick); a nonzero value
# would flag a hidden nondeterminism and is reported honestly.
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

ADVANCE = 150            # PLAYING ticks (2.5s @ 60Hz) — a fully-developed plume
CHANGED_MAD = 0.2        # bypass(TURB,CURL) must move whole-frame mean-abs pixels by >= this
                         # (measured ~0.41; the fire collapses when the turbulent spread is
                         # wired out — 0.2 clears it with wide margin)
EXACT_MAD = 0.01         # un-bypass rerun must match baseline within this (measured 0.000000)
MULTI_MIN = 0.02         # a single terminal's whole-frame contribution (measured ~0.06 each)
ADD_TOL = 0.02           # additivity residual bound (see gate B): |mad_AB-(mad_A+mad_B)|

# ---- a temp two-renderer particles DSL (sprites A -> streaks B, ONE chain) ----
# The pool "pool" output is fan-out 1, so multiple render terminals CHAIN through each
# other's pool passthrough (SPRI -> STRK). Both register a render lambda (renderers fetch the
# pool globally), so both draw over the SAME particles — bypassing either omits exactly its
# terminal while the other still renders.
_TWO_RENDERER_DSL = '''\
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine.lev2 import particles
from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P

tokens = CrcStringProxy()


class TwoTerminal(ParticleSystem):
  """POOL -> EMIT -> TURB -> SPRI (sprites, terminal A) -> STRK (streaks, terminal B)."""

  def __init__(self):
    super().__init__()
    self.pool = P.PoolData(size=20000, name="POOL")
    self.emit = P.RingEmitter(self.pool, name="EMIT",
                              LifeSpan=1.6, EmissionRate=3000, EmissionVelocity=2.0,
                              EmissionRadius=0.3, DispersionAngle=35.0,
                              Direction=vec3(0, 1, 0), Tangent=vec3(1, 0, 0),
                              Offset=vec3(0, 1, 0))
    self.turb = P.Turbulence(self.emit, name="TURB", Amount=vec3(3.0, 1.0, 3.0))

    self.mat_a = particles.GradientMaterial.createShared()
    self.mat_a.blending = tokens.ADDITIVE
    self.mat_a.depthtest = tokens.OFF
    self.mat_a.colorIntensity = 3.0
    self.mat_a.gradient.setColorStops({0.0: vec4(1, 0.5, 0.15, 1), 1.0: vec4(0, 0, 0, 1)})

    self.mat_b = particles.GradientMaterial.createShared()
    self.mat_b.blending = tokens.ADDITIVE
    self.mat_b.depthtest = tokens.OFF
    self.mat_b.colorIntensity = 3.0
    self.mat_b.gradient.setColorStops({0.0: vec4(0.2, 0.6, 1, 1), 1.0: vec4(0, 0, 0, 1)})

    self.sprites = P.SpriteRenderer(self.turb, name="SPRI", material=self.mat_a, Size=0.5)
    self.streaks = P.StreakRenderer(self.sprites, name="STRK", material=self.mat_b,
                                    Length=0.3, Width=0.04)
    self.render(self.sprites)


__all__ = ["TwoTerminal"]
'''


def _capture_mode(source, bypass_csv, out):
  """Subprocess leaf: boot the shell offscreen, apply the bypass set via the model API,
  advance ADVANCE ticks, capture the viewport RGB to `out` (.npy), exit.

  benches=False: this gate's determinism (fresh-seed identical particle stream, only the
  emitter consumes rand) is a PRE-BENCH property — an asset TESTBENCH would re-instantiate the
  emitter binding + drive it, so the bypass A/B is measured on the static production graph."""
  from orkengine import core       # noqa: F401  (core before lev2)
  from orkengine import lev2        # noqa: F401
  from ork.editor.dflowedit import DflowEditor
  names = [s for s in bypass_csv.split(",") if s]
  app = DflowEditor([source], bypass_ab={"bypass": names, "advance": ADVANCE, "out": out},
                    benches=False)
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()


def _spawn_capture(orkpython, source, bypass_csv, out, env):
  # ONE retry on the SAME machine: an offscreen GPU-context init occasionally flakes on
  # back-to-back spawns (a transient, not a logic failure) — a single re-run clears it.
  cmd = [orkpython, os.path.abspath(__file__), "--capture", source, bypass_csv, out]
  tag = os.path.splitext(os.path.basename(out))[0]
  for attempt in (1, 2):
    r = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=180)
    ok = "BYPASS_AB_RESULT=OK" in (r.stdout or "")
    for ln in (r.stdout or "").splitlines():
      if "[dflowedit bypass-ab]" in ln or "BYPASS_AB_RESULT=FAIL" in ln or "refused" in ln:
        print(f"    [{tag}] {ln.strip()}", flush=True)
    if ok:
      return True
    print(f"    [{tag}] leaf FAILED attempt {attempt} (rc={r.returncode})"
          f"{'' if attempt == 2 else ' — retrying'}\n{(r.stderr or '')[-400:]}", flush=True)
  return False


def _mad(a, b):
  import numpy
  if a is None or b is None or a.shape != b.shape:
    return -1.0
  return float(numpy.abs(a.astype(numpy.float32) - b.astype(numpy.float32)).mean())




def _orchestrate():
  import numpy
  orkpython = shutil.which("ork.python") or sys.executable
  workdir = tempfile.mkdtemp(prefix="dflowedit_bypass_")
  ok_all = True

  # -- write the two-renderer DSL + point the particles resolver at its dir --
  dsl_dir = os.path.join(workdir, "ptc")
  os.makedirs(dsl_dir, exist_ok=True)
  with open(os.path.join(dsl_dir, "two_terminal.py"), "w") as f:
    f.write(_TWO_RENDERER_DSL)
  env = dict(os.environ)
  prev = env.get("ORK_PARTICLES_SEARCH_PATH", "")
  env["ORK_PARTICLES_SEARCH_PATH"] = dsl_dir + (os.pathsep + prev if prev else "")

  def cap(source, bypass_csv, tag, env):
    out = os.path.join(workdir, tag + ".npy")
    ok = _spawn_capture(orkpython, source, bypass_csv, out, env)
    return (numpy.load(out) if ok and os.path.isfile(out) else None), ok

  # ==== GATE A : single-renderer fireball A/B ====
  print("[gate:bypassA] fireball single-renderer A/B ...", flush=True)
  base_a, o1 = cap("fireball", "", "A_baseline", os.environ.copy())
  byp_a, o2 = cap("fireball", "TURB,CURL", "A_bypass", os.environ.copy())
  rerun_a, o3 = cap("fireball", "", "A_rerun", os.environ.copy())
  mad_changed = _mad(base_a, byp_a)
  mad_exact = _mad(base_a, rerun_a)
  a1 = mad_changed >= CHANGED_MAD
  a2 = 0.0 <= mad_exact <= EXACT_MAD
  print(f"[gate:bypassA] baseline vs bypass(TURB,CURL) whole-frame MAD={mad_changed:.4f} "
        f"(>= {CHANGED_MAD} changed={a1})", flush=True)
  print(f"[gate:bypassA] baseline vs un-bypass rerun MAD={mad_exact:.4f} "
        f"(<= {EXACT_MAD} exact={a2})", flush=True)
  gateA = bool(o1 and o2 and o3 and a1 and a2)
  print(f"GATE_BYPASS_A={'PASS' if gateA else 'FAIL'}", flush=True)
  ok_all = ok_all and gateA

  # ==== GATE B : multi-terminal (sprites A -> streaks B) ====
  # Additive materials + a fresh-seed identical particle stream => the whole-frame MAD of each
  # removal is a non-negative light contribution, so mean(A)+mean(B)=mean(A+B) EXACTLY iff
  # bypass(A,B) renders NEITHER (an empty terminal set). The additivity residual is thus a
  # rigorous, background-independent empty-render proof (no brightness threshold needed).
  print("[gate:bypassB] two-terminal multi-renderer A/B ...", flush=True)
  base_b, p1 = cap("two_terminal", "", "B_baseline", env)
  byp_A, p2 = cap("two_terminal", "SPRI", "B_bypassA", env)       # terminal A omitted (B renders)
  byp_B, p3 = cap("two_terminal", "STRK", "B_bypassB", env)       # terminal B omitted (A renders)
  byp_AB, p4 = cap("two_terminal", "SPRI,STRK", "B_bypassAB", env)  # BOTH omitted (empty)
  rerun_b, p5 = cap("two_terminal", "", "B_rerun", env)
  mad_A = _mad(base_b, byp_A)                    # A's contribution
  mad_B = _mad(base_b, byp_B)                    # B's contribution
  mad_AB = _mad(base_b, byp_AB)                  # both contributions
  b_intact = _mad(byp_A, byp_AB)                 # B still renders in the A-bypassed frame
  additivity = abs(mad_AB - (mad_A + mad_B)) if min(mad_A, mad_B, mad_AB) >= 0 else 1e9
  mad_exact_b = _mad(base_b, rerun_b)
  b1 = mad_A >= MULTI_MIN and b_intact >= MULTI_MIN           # A gone, B intact
  b2 = (p4 and mad_AB >= mad_A and mad_AB >= mad_B and additivity <= ADD_TOL)  # both empty
  b3 = 0.0 <= mad_exact_b <= EXACT_MAD
  print(f"[gate:bypassB] bypass(A): whole_MAD={mad_A:.4f} B-still-renders_MAD={b_intact:.4f} "
        f"(A gone & B intact={b1})", flush=True)
  print(f"[gate:bypassB] contributions A={mad_A:.4f} B={mad_B:.4f} A+B={mad_A + mad_B:.4f} "
        f"vs both={mad_AB:.4f} additivity_residual={additivity:.4f} "
        f"(<= {ADD_TOL} empty={b2})", flush=True)
  print(f"[gate:bypassB] baseline vs un-bypass rerun MAD={mad_exact_b:.6f} "
        f"(<= {EXACT_MAD} exact={b3})", flush=True)
  gateB = bool(p1 and p2 and p3 and p4 and p5 and b1 and b2 and b3)
  print(f"GATE_BYPASS_B={'PASS' if gateB else 'FAIL'}", flush=True)
  ok_all = ok_all and gateB

  print(f"DFLOWEDIT_BYPASS_RESULT={'PASS' if ok_all else 'FAIL'}", flush=True)
  return 0 if ok_all else 1


def main(argv):
  if argv and argv[0] == "--capture":
    _capture_mode(argv[1], argv[2], argv[3])
    return 0
  return _orchestrate()


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
