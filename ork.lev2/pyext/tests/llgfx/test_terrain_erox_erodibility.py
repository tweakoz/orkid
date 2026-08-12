#!/usr/bin/env ork.python
###############################################################################
# EROX PER-CELL ERODIBILITY gate (Erox Milestone B).
#
# The hydraulic solver's erosion strength is now a per-cell scalar: the OPTIONAL
# "Erodibility" image input multiplies erosion_rate_per_s cell-by-cell (1.0 = the
# uniform rate, 0 = armored). This gate proves the two claims that matter:
#
#   e1  DIFFERENTIAL CARVING. One graph, a hardness field that is 0.1 over the left
#       half and 1.0 over the right: the soft half must lose measurably more material
#       than the hard half. Measured as mean(base - eroded) in METERS per half, read
#       from the RAW R32F capture (EXR scalar captures are unnormalized true units).
#   e2  ARMORING IS THE CAUSE, not terrain asymmetry. The same graph with the plug
#       UNWIRED is the control: its two halves erode alike, and wiring the field must
#       cut the HARD half's loss while leaving the SOFT half's loss essentially
#       unchanged (erodibility 1.0 there). Note the hard half does NOT fall to 10% of
#       the control — only the carving term scales; deposition and creep stay uniform.
#   e3  DEFAULT-ABSENT SEMANTICS. UNWIRED bakes BYTE-IDENTICAL to erodibility=T.const(1.0)
#       — the equivalence that proves an absent plug means exactly the uniform solver
#       (the shader's presence gate is dispatch-uniform, and x*1.0 is exact in IEEE).
#   e4  A non-field erodibility argument fails LOUD in the DSL (never a silent uniform).
#
# All-analytic corpus (fbm -> erox) with the cook cache wiped, so the bakes are
# deterministic; capDate (a per-bake timestamp) is masked before hashing, the sibling
# battery's convention. 512-dim / 512 m (1 m cells), ~1 s per bake.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import Path as _Path

import sys, shutil, hashlib
import numpy as np

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow.terrain.pywriter import to_python
from ork.hypergraph.dflow.terrain.resolve import load_dsl_class

DIM    = 512
EXTENT = 512.0          # 1 m cells — gullies resolve, and a bake is ~1 s
HARD   = 0.1            # erodibility of the left half (10x more resistant than nominal)

# thin concentrating flow (the erox.py tuning), short sim: enough carving to measure.
EROX = dict(sim_time_s            = 12.0,
            rain_mps              = 0.006,
            evaporation_per_s     = 0.08,
            flow_speed_max_mps    = 10.0,
            capacity_Kc           = 0.5,
            erosion_rate_per_s    = 0.7,
            deposition_rate_per_s = 1.0,
            creep_m2ps            = 3.0)

_CAPDATE = b"capDate\x00string\x00\x13\x00\x00\x00"


class _Erod(HeightField):
  """fbm relief -> erox, capturing the input AND the eroded height so the per-cell loss
  is a difference of two RAW fields. `mode` selects the erodibility wiring."""
  EXTENT_M = EXTENT

  def __init__(self, mode):
    super().__init__()
    base = (T.Fbm(frequency=6.0, octaves=7) * 0.5 + 0.5) * 150.0     # meters
    u    = T.gradient(dir_x=1.0, dir_y=0.0)                          # 0..1 across X
    soft = T.smoothstep(u, 0.49, 0.51)                               # 0 = left half, 1 = right half
    erod = {"unwired": None,
            "const1":  T.const(1.0),
            "split":   T.mix(HARD, 1.0, soft)}[mode]
    h = T.erox(base, **EROX, erodibility=erod)
    self.capture(base, "base", cache=True)
    self.capture(h, "height", cache=True)


def _wipe():
  shutil.rmtree(str(_Path.expandPathString("<staging>/dflowcache")), ignore_errors=True)


def _field(path):
  """The RAW R32F capture as a (h,w) float array (EXR scalar captures are true units)."""
  img = lev2.Image.createFromFile(path)
  a   = np.frombuffer(img.data.bytes, dtype=np.float32)
  return a.reshape(img.height, img.width, img.numcomponents)[:, :, 0]


def _masked(path):
  """Raw EXR bytes with the embedded capDate (a per-bake timestamp) zeroed, so two
  identical bakes hash equal (the battery's byte-identity convention)."""
  with open(path, "rb") as f:
    b = bytearray(f.read())
  i = b.find(_CAPDATE)
  if i >= 0:
    b[i + len(_CAPDATE):i + len(_CAPDATE) + 19] = b"\x00" * 19
  return hashlib.sha256(bytes(b)).hexdigest()


def _bake(mode, ctx, outdir):
  _wipe()
  g = _Erod(mode).generatedflow()
  os.makedirs(outdir, exist_ok=True)
  for cap in lev2.terrain.capture_modules(g):
    cap.path = os.path.join(outdir, "{channel}.exr")
  lev2.terrain.bake_heightfield(g, ctx, DIM, EXTENT)
  hpath = os.path.join(outdir, "height.exr")
  assert os.path.exists(hpath), f"no height capture in {outdir}"
  return hpath, _field(os.path.join(outdir, "base.exr")), _field(hpath)


def _halves(base, eroded):
  """(left, right) mean material LOSS in meters — left is the hard half in 'split'."""
  loss = base - eroded
  w    = loss.shape[1] // 2
  return float(loss[:, :w].mean()), float(loss[:, w:].mean())


def _raises_typeerror(fn):
  try:
    fn()
    return False
  except TypeError:
    return True


def run(ez, ctx):
  base = "/tmp/erox_erodibility_gate"
  shutil.rmtree(base, ignore_errors=True); os.makedirs(base, exist_ok=True)
  results = {}

  h_un, b_un, e_un = _bake("unwired", ctx, os.path.join(base, "unwired"))
  h_c1, _,    _    = _bake("const1",  ctx, os.path.join(base, "const1"))
  h_sp, b_sp, e_sp = _bake("split",   ctx, os.path.join(base, "split"))

  uL, uR = _halves(b_un, e_un)     # control: uniform coefficient
  sL, sR = _halves(b_sp, e_sp)     # left = erodibility 0.1, right = 1.0

  # ---- e1: the soft half loses measurably more than the hard half --------------------
  ratio = sR / max(sL, 1e-9)
  e1 = (ratio > 2.0)
  print(f"[erox-erod] e1 split: mean loss hard(x{HARD:g})={sL:.4f} m  soft(x1.0)={sR:.4f} m  "
        f"soft/hard={ratio:.3f} -> {'DIFFERENTIAL' if e1 else 'FLAT'}", flush=True)
  results["e1_soft_loses_more_than_hard"] = e1

  # ---- e2: the control's halves match; wiring armors the hard half only ---------------
  ctrl  = uR / max(uL, 1e-9)
  armor = sL / max(uL, 1e-9)       # hard half vs the same half unarmored
  soft_parity = sR / max(uR, 1e-9) # soft half vs the same half unarmored (erodibility 1.0)
  e2 = (0.85 < ctrl < 1.18) and (armor < 0.5) and (0.9 < soft_parity < 1.1)
  print(f"[erox-erod] e2 control(uniform): left={uL:.4f} m right={uR:.4f} m right/left={ctrl:.3f} | "
        f"armored hard half = {armor:.3f}x control, soft half = {soft_parity:.3f}x control", flush=True)
  results["e2_armoring_is_the_cause"] = e2

  # ---- e3: UNWIRED == erodibility=const(1.0), byte-identical --------------------------
  sha_un, sha_c1 = _masked(h_un), _masked(h_c1)
  e3 = (sha_un == sha_c1)
  print(f"[erox-erod] e3 unwired vs erodibility=T.const(1.0): "
        f"{'IDENTICAL' if e3 else 'DIVERGED'} ({sha_un[:12]} / {sha_c1[:12]})", flush=True)
  # control: the split field DOES change the bytes (proves e3 is not comparing no-ops).
  split_differs = (_masked(h_sp) != sha_un)
  print(f"[erox-erod] e3 control: a wired NON-unit field changes the bake: {split_differs}", flush=True)
  results["e3_absent_equals_unit_field"] = e3 and split_differs

  # ---- e4: a non-field erodibility fails LOUD (inside a trace, where the verb runs) ----
  class _BadErod(HeightField):
    def __init__(self):
      super().__init__()
      T.erox(T.const(1.0), erodibility=0.5)   # a scalar is NOT a per-cell field

  e4 = _raises_typeerror(_BadErod)
  print(f"[erox-erod] e4 erodibility=<scalar> raises TypeError: {e4}", flush=True)
  results["e4_non_field_fails_loud"] = e4

  # ---- e5: the wired field survives the document -> .py -> bake round trip -------------
  inst = _Erod("split"); inst.generatedflow()
  src  = to_python(inst.document(), class_name="ErodRound", extent_m=EXTENT,
                   source_note="erox erodibility gate")
  srcdir = os.path.join(base, "src"); os.makedirs(srcdir, exist_ok=True)
  srcpath = os.path.join(srcdir, "erod_round.py")
  with open(srcpath, "w") as f:
    f.write(src)
  emits = "erodibility=" in src
  rt_inst = load_dsl_class(srcpath)()
  _wipe()
  g = rt_inst.generatedflow()
  rtdir = os.path.join(base, "roundtrip"); os.makedirs(rtdir, exist_ok=True)
  for cap in lev2.terrain.capture_modules(g):
    cap.path = os.path.join(rtdir, "{channel}.exr")
  lev2.terrain.bake_heightfield(g, ctx, DIM, EXTENT)
  same = (_masked(os.path.join(rtdir, "height.exr")) == _masked(h_sp))
  e5 = emits and same
  print(f"[erox-erod] e5 emitted .py carries erodibility=: {emits}; re-baked identical: {same}",
        flush=True)
  results["e5_dsl_roundtrip"] = e5

  return results


def main():
  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    results = run(ez, ctx)
    ok = bool(results) and all(results.values())
    print(f"\n=== EROX per-cell erodibility gate {'PASSED' if ok else 'FAILED'} ===", flush=True)
    for k, v in results.items():
      print(f"    {k}: {'ok' if v else 'FAIL'}", flush=True)
  except BaseException:
    import traceback; traceback.print_exc()
    ok = False
  ez.mainThreadEnd()
  ecs.headless_exit()
  sys.exit(0 if ok else 1)


if __name__ == "__main__":
  main()
