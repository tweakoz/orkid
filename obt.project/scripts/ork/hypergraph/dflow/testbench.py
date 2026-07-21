###############################################################################
# ork.hypergraph.dflow.testbench — verilog-style AUTHORING-TIME stimulus for the
# hypr DSL asset families. A Testbench is declared WITH the asset (module-level
# TESTBENCH = T.Testbench(...)) and is a PASSIVE data object: importing the asset
# in production evaluates it with ZERO engine side effects (no graph build, no GPU,
# no scene). ONLY an editing context (ork.dflow.edit.py's family binding) resolves
# it — through getattr(module, "TESTBENCH", None). It NEVER enters an instantiation
# path (player / viewer / compose contributor) and NEVER a cook hash.
#
# The verilog discipline: a bench wires the DUT's PORTS from OUTSIDE; it does not
# edit the DUT. Concretely a bench may
#   (a) choose construction kwargs for the EDITING instantiation (instantiate=...),
#   (b) spawn bench ENTITIES driven by deterministic, transport-locked MOTION
#       PROGRAMS (orbit / line / figure8 — pure functions of transport time; no
#       wall clock), bound to the asset through its own published-entity seam
#       (fireball's emitter_entity="@bench" -> a bench entity the emitter reads),
#   (c) hint the editor camera (camera=T.frame(...)).
# It may NOT mutate the asset graph's TOPOLOGY.
#
# Motion is LIVE by construction: the editor evaluates the motion program on the
# transport tick and feeds the resulting transform through the graphinst's entity
# resolver (GraphInst.setEntityResolver) — so tweaking radius/period re-parameterizes
# the pure motion program with NO rebake and NO graph touch. Toggling `enabled` (or an
# instantiate-kwarg override) changes the EDITING INSTANTIATION, so it rides the shell's
# honest re-instantiation (rebake) path.
###############################################################################

import math

from orkengine.core import vec3


###############################################################################
# Motion programs — pure deterministic functions of TRANSPORT TIME (seconds).
# position_at(t) is the only runtime surface (returns a world-space vec3 the editor
# feeds to the entity resolver). params()/set_param() drive the LIVE propsheet.
###############################################################################

def _as_vec3(v):
  if isinstance(v, vec3):
    return v
  return vec3(float(v[0]), float(v[1]), float(v[2]))


def _perp_basis(axis):
  """Two orthonormal vectors spanning the plane perpendicular to `axis` (unit).
  Deterministic: the reference is the world axis least parallel to `axis`."""
  a = _as_vec3(axis).normalized
  ref = vec3(1, 0, 0) if abs(a.x) < 0.9 else vec3(0, 1, 0)
  u = a.cross(ref).normalized
  v = a.cross(u).normalized
  return u, v


class MotionProgram:
  """Base motion program. Subclasses implement position_at(t) as a pure function of
  transport time; the editor evaluates it per tick (no wall clock -> deterministic)."""

  kind = "motion"

  def position_at(self, t):
    raise NotImplementedError

  # ---- LIVE propsheet surface (motion params re-parameterize with no rebake) ----

  def params(self):
    """[(name, value)] of the editable, LIVE motion scalars."""
    return []

  def set_param(self, name, value):
    if name in dict(self.params()):
      setattr(self, name, float(value))


class Orbit(MotionProgram):
  """Circular orbit in the plane perpendicular to `axis`, radius `radius`, one
  revolution per `period_s`, lifted `height` along the axis from `center`."""

  kind = "orbit"

  def __init__(self, *, radius=2.5, period_s=4.0, axis=(0, 1, 0), height=0.0,
               center=(0, 0, 0), phase_deg=0.0):
    self.radius = float(radius)
    self.period_s = float(period_s)
    self.axis = _as_vec3(axis)
    self.height = float(height)
    self.center = _as_vec3(center)
    self.phase_deg = float(phase_deg)

  def position_at(self, t):
    phase = math.radians(self.phase_deg)
    ang = phase + (2.0 * math.pi * (t / self.period_s) if self.period_s > 0.0 else 0.0)
    u, v = _perp_basis(self.axis)
    a = _as_vec3(self.axis).normalized
    return self.center + a * self.height + (u * math.cos(ang) + v * math.sin(ang)) * self.radius

  def params(self):
    return [("radius", self.radius), ("period_s", self.period_s), ("height", self.height)]


class Line(MotionProgram):
  """Ping-pong sweep between `start` and `end`, one round trip per `period_s` (the
  bench stays in frame — a one-way sweep would drift off)."""

  kind = "line"

  def __init__(self, *, start=(-2.0, 0.0, 0.0), end=(2.0, 0.0, 0.0), period_s=4.0):
    self.start = _as_vec3(start)
    self.end = _as_vec3(end)
    self.period_s = float(period_s)

  def position_at(self, t):
    if self.period_s <= 0.0:
      return self.start
    phase = (t / self.period_s) % 1.0
    tri = 1.0 - abs(2.0 * phase - 1.0)              # 0->1->0 triangle wave (ping-pong)
    return self.start + (self.end - self.start) * tri

  def params(self):
    return [("period_s", self.period_s)]


class Figure8(MotionProgram):
  """Lemniscate (figure-8) in the plane perpendicular to `axis`: one lobe of width
  `width`, the crossing lobe of height `height`, one full trace per `period_s`."""

  kind = "figure8"

  def __init__(self, *, width=2.5, height=1.5, period_s=6.0, axis=(0, 1, 0),
               center=(0, 0, 0)):
    self.width = float(width)
    self.height = float(height)
    self.period_s = float(period_s)
    self.axis = _as_vec3(axis)
    self.center = _as_vec3(center)

  def position_at(self, t):
    ang = 2.0 * math.pi * (t / self.period_s) if self.period_s > 0.0 else 0.0
    u, v = _perp_basis(self.axis)
    x = math.sin(ang) * self.width
    y = math.sin(ang) * math.cos(ang) * self.height
    return self.center + u * x + v * y

  def params(self):
    return [("width", self.width), ("height", self.height), ("period_s", self.period_s)]


###############################################################################
# Camera hint (optional) — a passive suggestion the editor host MAY consume to frame
# the DUT. Never authoritative (the user's camera always wins on interaction).
###############################################################################

class CameraHint:
  __slots__ = ("distance", "elevation_deg", "azimuth_deg", "target")

  def __init__(self, *, distance=8.0, elevation_deg=15.0, azimuth_deg=0.0,
               target=(0, 0, 0)):
    self.distance = float(distance)
    self.elevation_deg = float(elevation_deg)
    self.azimuth_deg = float(azimuth_deg)
    self.target = _as_vec3(target)


###############################################################################
# The passive TESTBENCH declaration.
###############################################################################

class Testbench:
  """PASSIVE bench declaration. Holds construction kwargs for the editing instantiation,
  named motion-driven bench entities, an optional camera hint, and the standard `enabled`
  bool present on EVERY bench. Constructing one has ZERO engine side effects."""

  def __init__(self, *, instantiate=None, entities=None, camera=None, enabled=True):
    self.instantiate = dict(instantiate or {})
    self.entities = dict(entities or {})     # {published_name: MotionProgram}
    self.camera = camera
    self.enabled = bool(enabled)

  def primary_program(self):
    """The first motion program (the one the v1 propsheet section surfaces), or None."""
    for _name, prog in self.entities.items():
      if isinstance(prog, MotionProgram):
        return prog
    return None


###############################################################################
# Author surface (imported as `T` in an asset file) + the editor-only resolution seam.
###############################################################################

class _TestbenchFactory:
  """The `T` author surface: T.Testbench(...), T.orbit(...), T.line(...),
  T.figure8(...), T.frame(...). Pure constructors — no engine coupling."""

  Testbench = Testbench

  def orbit(self, **kw):
    return Orbit(**kw)

  def line(self, **kw):
    return Line(**kw)

  def figure8(self, **kw):
    return Figure8(**kw)

  def frame(self, **kw):
    return CameraHint(**kw)


T = _TestbenchFactory()


def resolve_testbench(module):
  """EDITOR-ONLY resolution: the Testbench declared in `module` (module-level TESTBENCH),
  or None. Never called by production / instantiate / cook paths. Refuses LOUDLY if
  TESTBENCH is present but is not a Testbench (a bench mis-declaration must not silently
  no-op)."""
  tb = getattr(module, "TESTBENCH", None)
  if tb is None:
    return None
  if not isinstance(tb, Testbench):
    raise TypeError(
        f"module-level TESTBENCH must be a testbench.Testbench, got "
        f"{type(tb).__name__!r} — declare it as TESTBENCH = T.Testbench(...)")
  return tb


__all__ = ["T", "Testbench", "MotionProgram", "Orbit", "Line", "Figure8",
           "CameraHint", "resolve_testbench"]
