###############################################################################
# DroopDemo — progressive gravity droop, as ONE multi-segment extrude. (Originally 16 CHAINED
# extrude_faces ops — one per strand segment, each grabbing the previous cap via a slot chain;
# now a single extrude_faces(segments=N): distance accumulates per ring, `direction` evaluates
# per ring (S.t = ring/N) so the strand bends downward progressively, and `scale` reproduces the
# old per-segment inset compounding as an absolute taper curve pow(1-TAPER, S.t*LEVELS).)
#
# EVERY face of the sphere (except the +Y pole cap) grows a drooping strand. The droop strength
# is modulated by a sinewave over the strand's ORIGINAL sphere UV (inherited up each strand),
# with a phase advanced by time -> a traveling "wind" wave across the sphere.
#
#   ./ork.hypermesh.viewer.py droop_demo
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh, S, isolate, group, POLY, param, sl_sin, vexpr

###############################################################################

RADIUS = 2.4
LEVELS = 64          # strand segments (rings of the single multi-segment extrude; was: one extrude each)
SEG    = 0.10        # segment length
DROOP  = 0.95        # downward bias added to the heading at the tip (gravity strength)
TAPER  = 0.05        # per-segment cross-section loss (the old inset=) -> strands taper toward a tip

WAVE_FREQ  = 24.0    # sinewave cycles across the U coord
WAVE_AMP   = 0.6     # wave depth (fraction of the droop it adds/removes; 0 = steady gravity)
WAVE_SPEED = 1.2     # phase advance per second (the wave's travel speed)

UP = vec3(0, 1, 0)

###############################################################################

class DroopDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    self._phase = param("phase", 0.0)              # runtime uniform: the wave phase (advanced by time in onUpdate)
    n = self.uvsphere(radius=RADIUS, segments=6, rings=6)
    roots = (S.N.dot(UP) < 0.95)                   # everything except the +Y pole cap
    n = self.select(n, roots, domain=POLY, op=isolate(group(0)))
    # the old chain's per-level ramp DROOP*(i+1)/LEVELS is exactly DROOP*S.t (S.t = ring/N);
    # the wave rides the strand's inherited sphere UV as before.
    droop = DROOP * S.t * (1.0 + WAVE_AMP * sl_sin(S.uv.x * WAVE_FREQ + self._phase))
    n = self.extrude_faces(
        n,
        distance=SEG,                              # per-ring step (accumulates along the bent heading)
        segments=LEVELS,                           # the whole strand in ONE op (was: LEVELS chained extrudes)
        direction=(S.N - vexpr(0, droop, 0)),      # per-ring downward bias; heading preserved
        scale=S.pow(1.0 - TAPER, S.t * LEVELS),    # the compounded per-segment inset as an absolute curve
        slot=0,                                    # the root selection (no per-level slot chain needed)
    )
    #n = self.smooth_normals(n)
    self.output(n)

  def onUpdate(self, updinfo):
    self._phase.set(updinfo.absolutetime * WAVE_SPEED)


__all__ = ["DroopDemo"]
