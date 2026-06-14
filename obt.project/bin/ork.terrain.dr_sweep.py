#!/usr/bin/env ork.python
###############################################################################
# ork.terrain.dr_sweep.py — headless DIAGNOSTIC: bake deproute_dbg at a sweep of
# `outer` values in one process and report drains-to-outflow white_frac per level.
# A correct route-through climbs monotonically to ~1.0 white. Collapse to ~0
# (all black) past some outer = divergence (cycles in basin_route).
#
#   ork.terrain.dr_sweep.py 512 0,1,2,4,8,16
###############################################################################
import sys
import numpy as np

from orkengine import core          # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.ecs.scene.assets import HeightField


def _field(exr_path):
    img = lev2.Image.createFromFile(str(exr_path))
    if not img or img.width == 0:
        raise RuntimeError(f"could not load {exr_path}")
    arr = np.array(img.numpy, dtype=np.float32)
    if arr.ndim == 3:
        arr = arr[..., 0]
    if img.bytesPerChannel == 1:
        arr = arr / 255.0
    elif img.bytesPerChannel == 2 and "F" not in img.format_name:
        arr = arr / 65535.0
    return arr.astype(np.float32)


def main():
    dim = int(sys.argv[1]) if len(sys.argv) > 1 else 512
    outers = [int(x) for x in (sys.argv[2].split(",") if len(sys.argv) > 2
                               else ["0", "1", "2", "4", "8", "16"])]

    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ezapp.mainThreadBegin()
    ctx = ezapp.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    print(f"dr_sweep: deproute_dbg dim={dim} outer={outers}", flush=True)
    print(f"{'outer':>6} {'white_frac':>11} {'black_frac':>11} {'mean':>9} {'min':>7} {'max':>7}", flush=True)
    for o in outers:
        hf = HeightField(dsl_file="deproute_dbg", dimension=dim,
                         extent_m=16384.0, height_scale_m=1000.0, ctx=ctx, outer=o)
        hf.gendata.asset_name = f"dr_dbg_{dim}_{o}"
        art = hf.build(ext="exr")
        a = _field(art["height"])
        white = float((a > 0.5).mean())
        black = float((a <= 0.5).mean())
        print(f"{o:>6} {white:>11.4f} {black:>11.4f} {a.mean():>9.4f} {a.min():>7.2f} {a.max():>7.2f}", flush=True)

    if hasattr(ezapp, "headless_exit"):
        ezapp.headless_exit()


if __name__ == "__main__":
    main()
