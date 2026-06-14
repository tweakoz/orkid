###############################################################################
# SdfConform — E.7/M4b flagship: SHRINKWRAP / CONFORM RETOPO. A clean, uniform
# icosphere is wrapped onto an animated implicit "blob": N spheres that each ORBIT
# the origin (steady angular motion, per-sphere rate / phase / plane), smooth-union'd
# into one field. The field is REDISTANCED (M4a) to a true |grad|=1 SDF, the icosphere
# is CONFORMED onto it (M4b), Taubin surface-fairing smooths the ridging, and a
# temporal EMA damps cross-frame jitter.
#
#   N orbiting spheres ─ smooth_union ─ redistance ─┐
#   icosphere (fixed base) ─────────────────────────┴─ conformToSdf ─ temporalSmooth ─ (render)
#
#   ./ork.hypermesh.viewer.py sdf_conform
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh
from ork.hypergraph.assets.materials.terrain.solid import Solid
from orkengine.core import vec3, vec4
import math


def _cross(a, b):
  return (a[1]*b[2] - a[2]*b[1], a[2]*b[0] - a[0]*b[2], a[0]*b[1] - a[1]*b[0])

def _norm(a):
  l = math.sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]) or 1.0
  return (a[0]/l, a[1]/l, a[2]/l)

def _orbit_basis(i, n):
  """An orthonormal (u,v) spanning sphere i's orbit PLANE. The plane normals are spread over the unit
  sphere by the Fibonacci/golden-angle spiral, so N orbits fill 3D instead of lying in one plane."""
  ga  = math.pi * (3.0 - math.sqrt(5.0))           # golden angle
  y   = 1.0 - 2.0 * (i + 0.5) / n                  # plane-normal: even latitudes
  r   = math.sqrt(max(0.0, 1.0 - y*y))
  ph  = i * ga
  nrm = (r*math.cos(ph), y, r*math.sin(ph))        # unit plane normal
  ref = (0.0, 1.0, 0.0) if abs(nrm[1]) < 0.9 else (1.0, 0.0, 0.0)
  u   = _norm(_cross(nrm, ref))
  v   = _cross(nrm, u)                             # unit (nrm,u orthonormal)
  return u, v


class SdfConform(Hypermesh):
  MATERIAL_CLASS = Solid
  MATERIAL_PARAMS = {
      "color": vec4(1, 0, 0, 1),
      "roughness": 0.0,
      "metallic": 1.0,
  }

  N             = 8       # number of orbiting spheres (definable; cost ~ N·dim^3 per frame)
  RES           = 16     # SDF brick / field resolution (finer = tighter wrap, cost ~dim^3)
  RADIUS        = 0.1   # each sphere's SDF radius
  K             = 1.85    # smooth_union blend radius (the metaball merge)
  ORBIT_R       = 1.8     # orbit radius (distance of each sphere's CENTER from the origin)
  OMEGA         = 0.2     # base angular rate (rad/sec)
  OMEGA_DETUNE  = 0.45    # per-sphere rate spread -> orbits drift in/out of phase (organic)
  SUBDIV        = 8       # icosphere base subdivisions
  CONFORM_STEPS = 12
  RELAX_STEPS   = 32       # Taubin surface-fairing passes
  RELAX_LAMBDA  = 0.5
  RELAX_PASSBAND= 15.5    # low-pass cutoff (lower = smooth more)
  TEMPORAL_TAU  = 0.35    # inter-frame EMA time-constant (sec; framerate-independent). 0/None = off.

  def __init__(self):
    super().__init__()
    reach     = self.ORBIT_R + self.RADIUS                  # max distance any sphere surface reaches
    extent    = 3.0 * reach + 3.0                           # brick contains the swept blob + margin
    sdfc      = self.sdf(dim=self.RES, extent=extent)
    self.spheres = [sdfc.sphere(self.RADIUS) for _ in range(self.N)]
    self._orbits = [_orbit_basis(i, self.N) for i in range(self.N)]   # per-sphere orbit-plane basis

    sdf = self.spheres[0]                                   # smooth_union the N spheres into one field
    for s in self.spheres[1:]:
      sdf = sdf.smooth_union(s, k=self.K)
    #sdf = sdf.redistance()                                  # M4a: true |grad|=1 SDF (smooth_union is not)

    ball = self.icosphere(radius=reach + 1.0, subdivisions=self.SUBDIV)  # clean base, encloses the blob
    skin = ball.conformToSdf(sdf,
                             conform_steps=self.CONFORM_STEPS,
                             relax_steps=self.RELAX_STEPS,
                             relax_lambda=self.RELAX_LAMBDA,
                             relax_passband=self.RELAX_PASSBAND,
                             iso_level=0.1)
    if self.TEMPORAL_TAU:
      skin = skin.temporalSmooth(tau=self.TEMPORAL_TAU)     # damp the redistance/JFA cross-frame jitter
    self.output(skin)

  def _angle(self, i, t):
    """Sphere i's orbit angle. STEADY orbit = constant angular velocity. (A future "circular harmonic"
    variant swaps THIS for e.g. A*sin(w t + phi) — pendulum — or a sum of harmonics; everything
    downstream is unchanged.)"""
    phi = 2.0 * math.pi * i / self.N                        # even phase spread
    w   = self.OMEGA * (1.0 + self.OMEGA_DETUNE * i)        # per-sphere detune
    return w * t + phi

  def onUpdate(self, updinfo):
    # poke ONLY each sphere's runtime offset plug; the whole chain re-evaluates on the GPU next frame.
    t = updinfo.absolutetime
    for i, s in enumerate(self.spheres):
      th     = self._angle(i, t)
      u, v   = self._orbits[i]
      ct, st = math.cos(th), math.sin(th)
      c = (self.ORBIT_R * (ct*u[0] + st*v[0]),              # center on the orbit circle in plane (u,v)
           self.ORBIT_R * (ct*u[1] + st*v[1]),
           self.ORBIT_R * (ct*u[2] + st*v[2]))
      s.inputs.offset = vec3(*c)
