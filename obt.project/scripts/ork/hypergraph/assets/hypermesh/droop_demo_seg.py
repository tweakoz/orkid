###############################################################################
# DroopDemoSeg — DroopDemoOct rebuilt as ONE multi-segment extrude (instead of 16 chained
# extrude ops). Each selected sphere face is inset to an octagon, then a SINGLE
# extrude_faces(segments=N, ...) grows the whole drooping strand: distance accumulates per ring,
# `direction` is evaluated per ring (S.t = ring/N) so the strand BENDS (gravity droop), and `scale`
# tapers the cross-section toward the tip. The UV/phase traveling wave drives the per-ring droop.
#
#   ./ork.hypermesh.viewer.py droop_demo_seg
###############################################################################
import numpy as np
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh, S, isolate, group, POLY, param, sl_sin, vexpr

RADIUS = 1.4
SEGS   = 16          # rings in the single multi-segment extrude (== old LEVELS)
SEG    = 0.10        # per-ring step length
DROOP  = 0.95        # downward bias strength at the tip
TAPER  = 0.85        # cross-section scale lost from base->tip (absolute)
INSET  = 0.05        # octagon inset in from each face boundary (0..1)

WAVE_FREQ  = 8.0
WAVE_AMP   = 0.6
WAVE_SPEED = 1.2
UP = vec3(0, 1, 0)

def _inst_mtx(tx, ty, tz, s, d0, d1, d2):
  # COLUMN-MAJOR mat4: each ROW below IS a glm COLUMN, so .reshape(-1) yields column-major order.
  # columns 0-2 = scaled basis (+ data in .w); column 3 = translation (.w must stay 1).
  return np.array([[s,  0.0, 0.0, d0],
                   [0.0, s,  0.0, d1],
                   [0.0, 0.0, s,  d2],
                   [tx, ty,  tz,  1.0]], dtype="float32")
mats = []
for i in range(16):
  gx, gz = i % 4, i // 4
  tx, tz = (gx - 1.5) * 16, (gz - 1.5) * 16
  h = i / 16.0                                       # per-instance debug data
  mats.append(_inst_mtx(tx, 0.0, tz, 1.0, h, 1.0 - h, 0.5))

class DroopDemoSeg(Hypermesh):
  VET = dict(allow_self_intersect=True)   # organic strands legitimately interpenetrate (bends + neighbor contact)

  def __init__(self):
    super().__init__()
    self._phase = param("phase", 0.0)
    n = self.uvsphere(radius=RADIUS, segments=36, rings=36)
    roots = (S.N.dot(UP) < 0.95)                        # whole sphere except the +Y pole
    n = self.select(n, roots, domain=POLY, op=isolate(group(0)))
    n = self.inset(n, amount=INSET, sides=9, slot=0, mask_inner=isolate(group(1)))
    # ONE extrude grows the whole strand. droop ramps with S.t (ring/N); the old chain's per-level
    # (i+1)/LEVELS becomes S.t. taper via absolute `scale`; inset's per-segment compounding -> scale curve.
    droop = DROOP * S.t * (1.0 + WAVE_AMP * sl_sin(S.uv.x * WAVE_FREQ + self._phase))
    n = self.extrude_faces(
      n,
      distance=SEG,
      segments=SEGS,
      direction=(S.N - vexpr(0, droop, 0)),             # per-ring downward bias = gravity droop
      scale=S.sin(S.time*3.0+1.0 - TAPER * S.t*10.0)*0.25+0.75,                        # absolute taper toward the tip
      twist = S.sin(S.time+S.t)*10.0*S.t,
      slot=1,                                           # grab the octagon cap (bit 1)
    )
    n = self.subdivide(n,smooth=True, level=1)
    n = self.smooth_normals(n)
    self.output(n)
    self.instances = mats

  def onUpdate(self, updinfo):
    self._phase.set(updinfo.absolutetime * WAVE_SPEED)  # advance the traveling wave


__all__ = ["DroopDemoSeg"]
