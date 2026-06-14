#!/usr/bin/env python3
###############################################################################
# E.2-A gate — the C++ SCATTER PLACER vs the numpy reference, pinned together by
# the stateless counter-based RNG spec (hash(seed, cell, stream)):
#   1. DENSITY mode, align="normal": bake a small HF + 2-type scatter; place with
#      BOTH implementations against the SAME channels; counts / type_id /
#      variant_seed EXACT, P / xform within tolerance.
#   2. COUNT mode (subsample): exercises the priority-cap path (keep smallest
#      SS_PRIO, cell-order output) in both.
#   3. DETERMINISM: the C++ placer run twice -> byte-identical .ogeo.
#   4. The asset-wrapper path (_run_scatters) places via the C++ placer and the
#      C++ materializer ALSO placed the same artifact at materialize (idempotent).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import numpy as np
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

DSL = '''
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

class ParityHF(HeightField):
  EXTENT_M = 256.0
  HEIGHT_M = 40.0
  def __init__(self):
    super().__init__()
    h = T.fbm(frequency=3.0, octaves=4) * 0.5 + 0.5
    self.capture(h, "height")
    alt = T.normalize(h)
    self.scatter("props",
        density = 0.02,
        seed    = 11,
        align   = "normal",
        yaw     = (0.0, 6.2831853),
        scale   = (0.7, 1.4),
        cutoff  = 0.05,
        jitter  = 0.9,
        lift    = 0.25,
        types   = {"low": 1.0 - alt, "high": alt})
    # NOTE: a CONSTANT weight dies at capture (auto-exposure normalizes a flat field
    # to 0) — use a varying mask biased high so most cells keep, then subsample to 64.
    self.scatter("sparse",
        count   = 64,
        seed    = 23,
        align   = "up",
        yaw     = (0.0, 1.0),
        scale   = (1.0, 1.0),
        cutoff  = 0.01,
        jitter  = 1.0,
        types   = {"any": T.normalize(h, out_lo=0.5, out_hi=1.0)})
'''


def compare(name, geo_py, n_py, ogeo_path, n_cpp):
  from orkengine.lev2 import Geometry
  gc = Geometry.read(str(ogeo_path))
  assert n_py == n_cpp, "%s: count diverged py=%d cpp=%d" % (name, n_py, n_cpp)
  assert n_py > 0, "%s: ZERO points placed — the case exercises nothing" % name
  t_py = np.array(geo_py.point["type_id"]);      t_c = np.array(gc.point["type_id"])
  s_py = np.array(geo_py.point["variant_seed"]); s_c = np.array(gc.point["variant_seed"])
  assert np.array_equal(t_py, t_c), "%s: type_id diverged" % name
  assert np.array_equal(s_py, s_c), "%s: variant_seed diverged" % name
  p_py = np.array(geo_py.point["P"], dtype=np.float64); p_c = np.array(gc.point["P"], dtype=np.float64)
  x_py = np.array(geo_py.point["xform"], dtype=np.float64); x_c = np.array(gc.point["xform"], dtype=np.float64)
  perr = np.abs(p_py - p_c).max() if n_py else 0.0
  xerr = np.abs(x_py - x_c).max() if n_py else 0.0
  assert perr < 1e-4, "%s: P diverged (max err %g)" % (name, perr)
  assert xerr < 1e-4, "%s: xform diverged (max err %g)" % (name, xerr)
  print("PARITY %s PASS (n=%d, Perr=%.2e, Xerr=%.2e)" % (name, n_py, perr, xerr), flush=True)


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    from ork.hypergraph.ecs.scene.assets import HeightField
    from ork.hypergraph.dflow.terrain import scatter as pyscatter
    from ork.hypergraph.dflow.terrain.base import ScatterSpec

    tmpdir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))), ".tmp")
    os.makedirs(tmpdir, exist_ok=True)
    dsl_path = os.path.join(tmpdir, "parity_hf.py")
    open(dsl_path, "w").write(DSL)

    hf = HeightField(dsl_file=dsl_path, dimension=256, extent_m=256.0, height_scale_m=40.0, ctx=ctx)
    hf.gendata.asset_name = "parity_hf"
    art = hf.build()
    assert "scatters" in art and "props" in art["scatters"] and "sparse" in art["scatters"], \
        "wrapper did not place the scatters (C++ placer path dead)"

    extent_m = hf.gendata.extent_m
    height_m = hf.gendata.height_scale_m
    sinks = {s.name: s for s in hf.gendata.scatters}

    # numpy REFERENCE placements against the SAME baked channels
    for name in ("props", "sparse"):
      sink = sinks[name]
      spec = ScatterSpec(
          name=sink.name, types=tuple(zip(sink.type_names, sink.type_channels)),
          density=(None if sink.density == 0.0 else sink.density),
          count=(None if sink.count == 0 else sink.count),
          seed=sink.seed, align=sink.align, yaw=(sink.yaw_lo, sink.yaw_hi),
          scale=(sink.scale_lo, sink.scale_hi), cutoff=sink.cutoff, jitter=sink.jitter,
          max_points=sink.max_points, lift=sink.lift)
      chans = {"height": art["height"]}
      for ch in sink.type_channels:
        chans[ch] = art[ch]
      geo_py, n_py = pyscatter.place(spec, chans, extent_m=extent_m, height_m=height_m)
      compare(name, geo_py, n_py, art["scatters"][name]["path"], art["scatters"][name]["count"])

    # count-mode actually subsampled? (sparse declared count=64 over a dense const mask)
    assert art["scatters"]["sparse"]["count"] == 64, \
        "count-mode subsample expected exactly 64 points, got %d" % art["scatters"]["sparse"]["count"]
    print("PARITY count-mode subsample PASS (priority cap = 64)", flush=True)

    # C++ determinism: place 'props' twice via the direct binding -> byte-identical files
    sink = sinks["props"]
    chans = {"height": art["height"]}
    for ch in sink.type_channels:
      chans[ch] = art[ch]
    o1 = os.path.join(tmpdir, "parity_det_a.ogeo")
    o2 = os.path.join(tmpdir, "parity_det_b.ogeo")
    n1 = lev2.terrain.scatter_place_ogeo(sink, chans, extent_m, height_m, o1)
    n2 = lev2.terrain.scatter_place_ogeo(sink, chans, extent_m, height_m, o2)
    assert n1 == n2 and open(o1, "rb").read() == open(o2, "rb").read(), \
        "C++ placer is not deterministic"
    print("PARITY determinism PASS (two C++ runs byte-identical, n=%d)" % n1, flush=True)
    ok = True
  except Exception:
    import traceback
    traceback.print_exc()
  finally:
    ezapp.mainThreadEnd()
    print("=== terrain scatter parity gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
