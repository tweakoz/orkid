###############################################################################
# DroopDemoOct — DroopDemo with a ROUND (inset-resampled) cross-section, as ONE multi-segment
# extrude. Each selected sphere face is first INSET to a regular N-gon cap (the collar bridges
# the quad/tri boundary, winding inherited from the face), then a SINGLE
# extrude_faces(segments=N) grows the whole drooping strand from that cap: `direction`
# evaluates per ring (S.t = ring/N) so the strand bends progressively, and `scale` reproduces
# the old per-segment inset compounding as an absolute taper curve. (Originally LEVELS chained
# extrudes walking a frontier-bit chain — slot 1 -> 2 -> ...; now just slot=1, no chain.)
# Same progressive gravity droop + UV/phase traveling wave as DroopDemo.
#
#   ./ork.hypermesh.viewer.py droop_demo_oct
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh, S, isolate, group, POLY, param, sl_sin, vexpr

RADIUS = 1.4
LEVELS = 16          # strand segments (rings of the single multi-segment extrude; was: one extrude each)
SEG    = 0.10        # segment length
DROOP  = 0.95        # downward bias added to the normal each level (gravity strength)
TAPER  = 0.12        # inset per segment -> strands taper toward a tip
INSET  = 0.05        # how far the octagon insets in from each face boundary (0..1)

# UV-driven wave: droop strength modulated by a sinewave over the strand's original 
#   sphere U, phase-advanced by time -> a traveling wave across the sphere.

WAVE_FREQ  = 8.0
WAVE_AMP   = 0.6
WAVE_SPEED = 1.2
UP = vec3(0, 1, 0)

class DroopDemoOct(Hypermesh):
  VET = dict(allow_self_intersect=True)   # organic strands legitimately interpenetrate (bends + neighbor contact)

  def __init__(self):
    super().__init__()
    self._phase = param("phase", 0.0)
    n = self.uvsphere(radius=RADIUS, segments=24, rings=24)
    roots = (S.N.dot(UP) < 0.95) # whole sphere except the +Y pole
    n = self.select(n, roots, domain=POLY, op=isolate(group(0))) # selected faces -> bit 0
    # resample every selected face to a regular OCTAGON cap (bit 1); the collar (bit 0) stays on the surface.
    n = self.inset(n, amount=INSET, sides=24, slot=0, mask_inner=isolate(group(1)))
    # the old chain's per-level ramp DROOP*(i+1)/LEVELS is exactly DROOP*S.t (S.t = ring/N);
    # the wave rides the strand's inherited sphere UV as before.
    droop = DROOP * S.t * (1.0 + WAVE_AMP * sl_sin(S.uv.x * WAVE_FREQ + self._phase))
    n = self.extrude_faces(
        n,
        distance=SEG,                              # per-ring step (accumulates along the bent heading)
        segments=LEVELS,                           # the whole strand in ONE op (was: LEVELS chained extrudes)
        direction=(S.N - vexpr(0, droop, 0)),      # per-ring downward bias; heading preserved
        scale=S.pow(1.0 - TAPER, S.t * LEVELS),    # the compounded per-segment inset as an absolute curve
        slot=1,                                    # the inset cap (no frontier-bit chain needed)
    )
    n = self.smooth_normals(n)
    self.output(n)

  def onUpdate(self, updinfo):
    self._phase.set(updinfo.absolutetime * WAVE_SPEED)   # advance the wave; 
                                                         # drives every segment


__all__ = ["DroopDemoOct"]
