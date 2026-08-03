###############################################################################
# _celestial.py — OBSERVER-FRAME EPHEMERIS: where the sun, the moon and the
# star dome are for a given latitude / date / clock. PURE PYTHON MATH — this
# module imports nothing but `math` at module scope, so it evaluates in the ECS
# sim SUBINTERPRETER (see _celestial_orbit.py) and in unit tests that run
# without the engine built. The two quat helpers that DO need engine types
# import them lazily, inside the method.
#
# WORLD FRAME (right-handed, +Y up — the engine's):
#     +X = EAST      +Y = UP      +Z = SOUTH   (so -Z is NORTH)
# That is forced: with +Y up and +X east, a right-handed basis puts +Z south.
#
# AZIMUTH CONVENTIONS — two of them, do not mix:
#   * ASTRONOMICAL (what this module REPORTS as *_azimuth_deg): degrees of the
#     body's compass bearing, 0=N, 90=E, 180=S, 270=W.
#   * DSL (what _helpers.elevation_azimuth_quat consumes): degrees about world
#     +Y, 0 = light TRAVELLING toward +Z.
#   In the world frame above they are exact negatives:  dsl = -astronomical.
#   Snapshot.sun_light_angles() / moon_light_angles() do that conversion; the
#   raw *_azimuth_deg fields never do.
#
# ACCURACY: low-precision ephemeris (Schlyter's classic elements + the leading
# lunar perturbation terms). Visual plausibility is the bar — expect arcminutes
# on the sun, a few arcminutes on the moon. NOT modeled: atmospheric refraction,
# the sun's/moon's finite angular radius, lunar topocentric parallax (up to ~1
# degree of moon altitude), nutation. Every reported angle is GEOMETRIC and
# GEOCENTRIC. What IS exact is the latitude dependence: declination + hour angle
# go through the standard spherical triangle, so the seasonal arc, the noon
# altitude and the day length are right for the observer.
#
# CLOCK: time_of_day is UT at longitude 0 (longitude_deg shifts the observer's
# local sidereal time, not the clock). `at(wall_seconds)` is STATELESS — the
# per-frame component feeds it the sim's absolute time, so the sky never drifts
# and scrubbing backward is exact.
###############################################################################

import math

_DEG2RAD = math.pi / 180.0
_RAD2DEG = 180.0 / math.pi

# Schlyter's element epoch: 1999 Dec 31.0 UT (JD 2451543.5) — all the orbital
# element polynomials below are stated against days from THAT instant.
_EPOCH_JD = 2451543.5

# The linear element rates, deg/day, named once and used BOTH in evaluate_days'
# polynomials and in the placement solver below (which needs to know how fast a
# re-epoched moon moves). Naming them is what keeps the two in step.
_SUN_ANOM_RATE       = 0.9856002585    # Ms
_SUN_PERIHELION_RATE = 4.70935e-5      # ws
_MOON_NODE_RATE      = 0.0529538083    # Nm, REGRESSES (subtracted)
_MOON_PERIGEE_RATE   = 0.1643573223    # wm
_MOON_ANOM_RATE      = 13.0649929509   # Mm

# Mean longitude rates (Lm = Nm + wm + Mm, Ls = ws + Ms) and the synodic rate
# between them: 12.19 deg/day, i.e. the 29.53-day lunation.
_MOON_LON_RATE = _MOON_ANOM_RATE + _MOON_PERIGEE_RATE - _MOON_NODE_RATE
_SUN_LON_RATE  = _SUN_ANOM_RATE + _SUN_PERIHELION_RATE
_SYNODIC_RATE  = _MOON_LON_RATE - _SUN_LON_RATE
_SIDEREAL_MONTH_DAYS = 360.0 / _MOON_LON_RATE

###############################################################################
# MOON PLACEMENT — how a DECLARED moon (phase / initial elevation / orbit rate)
# is made true without touching the declared date, clock or site.
#
# The mechanism is RE-EPOCHING THE LUNAR ELEMENTS: the moon's elements are
# evaluated at their own clock
#     dm = d0 + (d - d0) * orbit_clock_scale + epoch_shift
# while the sun, the sidereal angle and the obliquity stay on the declared one.
# So the sky's DATE never moves — no date search, nothing else in the frame
# changes — and the declared placement composes with the rate, because the two
# terms are independent (at t=0 the rate term is zero by construction).
#
#   epoch_shift (days)   sets WHERE ON ITS ORBIT the moon starts. One day of
#                        shift advances the moon 13.18 deg of ecliptic
#                        longitude with the sun standing still, so it is what
#                        the PHASE solve turns.
#   orbit_clock_scale    sets HOW FAST it goes round. It is derived from the
#                        declared moon_orbit_rate, which is a SYNODIC-period
#                        scale (rate 2 = lunations twice as fast) — the moon's
#                        sidereal rate has to run a little faster than that to
#                        make up the sun's own motion, which is exactly the
#                        conversion _moon_clock_scale() does.
#   node_shift (degrees) turns the ORBIT PLANE about the ecliptic pole, added to
#                        the node and taken back off the perigee so the mean
#                        longitude Lm = Nm + wm + Mm is untouched. That is the
#                        one freedom left once a phase has pinned the moon's
#                        ecliptic longitude, and it is only +-5.15 deg of
#                        ecliptic latitude wide — which is why a phase and an
#                        elevation together are over-constrained outside a
#                        narrow band, and say so instead of bending.
###############################################################################

# The env var carrying scene→sim celestial config. PythonComponentData reflects
# only a script path (the limitation _sun_orbit.py documents for its period), so
# a declared config reaches the sim subinterpreter as JSON in the process
# environment, keyed by ENTITY NAME. Precedent: walk_input_system.py's
# ORK_WALK_SELFTEST. Set by Scene.sun(celestial=...), read by _celestial_orbit.py.
CONFIG_ENV_KEY = "ORK_CELESTIAL_CONFIG"

# The SITE + CLOCK keys — the ephemeris itself. CelestialModel takes exactly
# these as constructor arguments; the rest of the config is for the per-frame
# component, not for the model. The three moon_* keys are the AUTHORED lunar
# placement (see MOON PLACEMENT below): they re-epoch the lunar elements, which
# is ephemeris, so they travel with the site and clock — Scene.moon() copies
# this whole tuple off the sun, and one sky keeps one moon.
MODEL_CONFIG_KEYS = ("latitude_deg", "longitude_deg", "day_of_year",
                     "time_of_day", "time_scale", "year",
                     "moon_phase", "moon_initial_elevation", "moon_orbit_rate")

# Which body a config aims. One config table entry per LIGHT ENTITY, so a sun and
# a moon over the same site are two entries that share every site/clock key and
# differ here (Scene.moon() copies the sun's entry — see _moon.py).
LEGAL_BODIES = ("sun", "moon")

# Every key a celestial= config may carry, with its default. Unknown keys are a
# hard error (a typo'd latitude must not silently become the default).
CONFIG_DEFAULTS = {
  "latitude_deg":  45.0,    # observer latitude, + north
  "longitude_deg":  0.0,    # observer longitude, + east
  "day_of_year":  172.0,    # 1..365 (fractional ok); 172 ~ June solstice
  "time_of_day":   12.0,    # hours UT, 0..24 (fractional ok)
  # sim-seconds of clock per real second. 480 = a full day in 180 wall seconds,
  # PROVISIONAL in the owner's words: "for now (until we trust the sky rendering
  # is complete)". A MITIGATION, not a fix — the sun slows from ~6 to ~2 degrees
  # per wall second, which makes a look judgeable; the publish-chain latency is
  # unchanged and the lag defect stands.
  "time_scale":   480.0,
  "year":          2000,    # calendar year the day_of_year belongs to
  # AUTHORED LUNAR PLACEMENT — None means "whatever the real sky did on the
  # declared date", i.e. the pure ephemeris. See MOON PLACEMENT below for what
  # a declared value moves and what it costs.
  "moon_phase":          None,   # synodic phase at t=0: 0..1, or a PHASE_NAMES name
  "moon_initial_elevation": None,# moon altitude at t=0, degrees
  "moon_orbit_rate":      1.0,   # synodic-period scale, 1.0 = Earth's moon
  "body":         "sun",    # which body aims this light: "sun" | "moon"
  "base_intensity": 4.0,    # the light's DECLARED intensity, which the night
                            # policy scales per frame. The DSL always publishes
                            # the real one; this default only matches Scene.sun().
}


# The four phases that have names. Values are SYNODIC positions, the same 0..1
# CelestialSnapshot.moon_phase reports: 0 new, 0.25 first quarter (waxing),
# 0.5 full, 0.75 last quarter (waning).
PHASE_NAMES = {
  "new":            0.0,
  "first_quarter":  0.25,
  "full":           0.5,
  "last_quarter":   0.75,
}


def phase_fraction(value):
  """A declared phase → its synodic fraction in [0,1). Accepts a PHASE_NAMES
  name or a number; 1.0 wraps to 0.0 (one cycle). Anything else raises."""
  if isinstance(value, str):
    try:
      return PHASE_NAMES[value]
    except KeyError:
      raise ValueError(
        f"celestial config: moon phase {value!r} is not a phase name — legal "
        f"names are {sorted(PHASE_NAMES)}, or pass a 0..1 synodic fraction "
        f"(0 new, 0.25 first quarter, 0.5 full, 0.75 last quarter)") from None
  frac = float(value)
  if not (0.0 <= frac <= 1.0):
    raise ValueError(
      f"celestial config: moon phase {frac} is outside 0..1 — the synodic "
      f"cycle is one turn, not an angle in degrees or a day count")
  return frac % 1.0


def normalize_config(cfg):
  """Validate a celestial= config dict → a full dict with defaults filled in.

  Raises on an unknown key rather than ignoring it: a misspelled knob that
  silently keeps the default is a sky that is wrong for no visible reason."""
  if not isinstance(cfg, dict):
    raise TypeError(
      f"celestial config must be a dict of {sorted(CONFIG_DEFAULTS)}; "
      f"got {type(cfg).__name__}")
  unknown = set(cfg) - set(CONFIG_DEFAULTS)
  if unknown:
    raise KeyError(
      f"celestial config: unknown key(s) {sorted(unknown)}; "
      f"legal keys are {sorted(CONFIG_DEFAULTS)}")
  out = dict(CONFIG_DEFAULTS)
  out.update(cfg)
  out["year"] = int(out["year"])
  for k in ("latitude_deg", "longitude_deg", "day_of_year", "time_of_day",
            "time_scale", "base_intensity"):
    out[k] = float(out[k])
  if out["moon_phase"] is not None:
    out["moon_phase"] = phase_fraction(out["moon_phase"])
  if out["moon_initial_elevation"] is not None:
    out["moon_initial_elevation"] = float(out["moon_initial_elevation"])
    if not (-90.0 <= out["moon_initial_elevation"] <= 90.0):
      raise ValueError(
        f"celestial config: moon_initial_elevation "
        f"{out['moon_initial_elevation']} is not an altitude in -90..90 degrees")
  out["moon_orbit_rate"] = float(out["moon_orbit_rate"])
  if out["moon_orbit_rate"] <= 0.0:
    raise ValueError(
      f"celestial config: moon_orbit_rate {out['moon_orbit_rate']} must be "
      f"positive (it scales the synodic period; 1.0 = Earth's moon)")
  out["body"] = str(out["body"])
  if out["body"] not in LEGAL_BODIES:
    raise ValueError(
      f"celestial config: body {out['body']!r} is not one of "
      f"{list(LEGAL_BODIES)}")
  return out


###############################################################################
# degree-flavored trig — the ephemeris literature is all in degrees, and
# transcribing it in radians is how sign errors get in.
###############################################################################


def _sind(d):
  return math.sin(d * _DEG2RAD)


def _cosd(d):
  return math.cos(d * _DEG2RAD)


def _asind(x):
  return math.asin(max(-1.0, min(1.0, x))) * _RAD2DEG


def _atan2d(y, x):
  return math.atan2(y, x) * _RAD2DEG


def _rev(d):
  """degrees → [0,360)."""
  return d - 360.0 * math.floor(d / 360.0)


def _julian_day_jan1(year):
  """JD at 0h UT on January 1 of `year` (proleptic Gregorian)."""
  y = year + 4799   # (14-M)//12 == 1 for M==1, so y = year + 4800 - 1
  jdn = 1 + (153 * 10 + 2) // 5 + 365 * y + y // 4 - y // 100 + y // 400 - 32045
  return jdn - 0.5


def _ecliptic_to_equatorial(lon, lat, obliquity):
  """(ecliptic lon/lat, obliquity) degrees → (right ascension, declination) degrees."""
  x = _cosd(lat) * _cosd(lon)
  y = _cosd(lat) * _sind(lon)
  z = _sind(lat)
  ye = y * _cosd(obliquity) - z * _sind(obliquity)
  ze = y * _sind(obliquity) + z * _cosd(obliquity)
  return _rev(_atan2d(ye, x)), _atan2d(ze, math.hypot(x, ye))


def _alt_az(ra, dec, local_sidereal, latitude):
  """The spherical triangle: (RA, dec, LST, latitude) degrees → (elevation,
  azimuth) degrees, azimuth ASTRONOMICAL (0=N, 90=E). This is the one place
  latitude enters, and it is why the seasonal arc comes out right."""
  H = _rev(local_sidereal - ra)            # hour angle, west-positive
  elevation = _asind(_sind(latitude) * _sind(dec)
                     + _cosd(latitude) * _cosd(dec) * _cosd(H))
  azimuth = _rev(_atan2d(-_cosd(dec) * _sind(H),
                         _sind(dec) * _cosd(latitude)
                         - _cosd(dec) * _sind(latitude) * _cosd(H)))
  return elevation, azimuth


###############################################################################


class CelestialSnapshot:
  """One evaluation of a CelestialModel — the whole observed sky at one instant.

  Angles in degrees. Azimuths are ASTRONOMICAL (0=N, 90=E, 180=S, 270=W); use
  sun_light_angles() / moon_light_angles() to get the (elevation, azimuth) pair
  the DSL's elevation_azimuth_quat wants.

  moon_phase — the SYNODIC CYCLE position, 0..1 wrapping:
      0.00 = new, 0.25 = first quarter (waxing), 0.50 = full,
      0.75 = last quarter (waning), → 1.0 back to new.
      It is the moon's apparent ecliptic longitude minus the sun's, over 360,
      so it advances monotonically (never reverses) at ~1/29.53 per day.
  moon_phase_angle_deg — the Sun-Moon-Earth angle: 0 at full, 180 at new.
  moon_illumination    — lit fraction of the disc, 0..1 (0 new, 1 full)."""

  def __init__(self, **kwargs):
    for k, v in kwargs.items():
      setattr(self, k, v)

  ###########################################################################
  # light aiming — astronomical azimuth → the DSL's about-+Y azimuth
  ###########################################################################

  def sun_light_angles(self):
    """(elevation, azimuth) degrees in the DSL convention, ready for
    _helpers.elevation_azimuth_quat / an ecssim quat built the same way."""
    return self.sun_elevation_deg, -self.sun_azimuth_deg

  def moon_light_angles(self):
    return self.moon_elevation_deg, -self.moon_azimuth_deg

  def sun_vector(self):
    """World-space unit vector pointing AT the sun (a 3-tuple, engine-free).
    The light's travel direction is its negation."""
    return _sky_vector(self.sun_elevation_deg, self.sun_azimuth_deg)

  def moon_vector(self):
    return _sky_vector(self.moon_elevation_deg, self.moon_azimuth_deg)

  ###########################################################################
  # engine-typed conveniences — lazily imported so the model stays pure math
  ###########################################################################

  def sun_quat(self):
    """Entity orientation aiming a directional sun (light travels along +Z)."""
    from ork.hypergraph.ecs.scene._helpers import elevation_azimuth_quat
    return elevation_azimuth_quat(*self.sun_light_angles())

  def moon_quat(self):
    from ork.hypergraph.ecs.scene._helpers import elevation_azimuth_quat
    return elevation_azimuth_quat(*self.moon_light_angles())

  def star_dome_quat(self):
    """Orientation for a star dome authored with its local +Y at the NORTH
    CELESTIAL POLE and right ascension measured about that axis: tip local +Y
    to the pole (elevation = observer latitude, due north), then spin by the
    sidereal angle. The spin is NEGATIVE because the sky's apparent turn is the
    reverse of the earth's."""
    from orkengine.core import vec3, quat
    tilt = quat.createFromAxisAngle(vec3(1.0, 0.0, 0.0),
                                    (self.pole_elevation_deg - 90.0) * _DEG2RAD)
    spin = quat.createFromAxisAngle(vec3(0.0, 1.0, 0.0),
                                    -self.sidereal_angle_deg * _DEG2RAD)
    return tilt * spin

  def as_dict(self):
    return dict(self.__dict__)

  def __repr__(self):
    return ("CelestialSnapshot(sun el=%.2f az=%.2f, moon el=%.2f az=%.2f, "
            "phase=%.3f)" % (self.sun_elevation_deg, self.sun_azimuth_deg,
                             self.moon_elevation_deg, self.moon_azimuth_deg,
                             self.moon_phase))


def _sky_vector(elevation, azimuth):
  """(elevation, ASTRONOMICAL azimuth) degrees → world unit vector toward the
  point, in the +X east / +Y up / +Z south frame."""
  c = _cosd(elevation)
  return (c * _sind(azimuth), _sind(elevation), -c * _cosd(azimuth))


###############################################################################


def _moon_clock_scale(orbit_rate):
  """A SYNODIC-period scale → the factor the lunar element clock runs at.

  moon_orbit_rate is stated on the lunation (what an observer sees: rate 2 =
  full moons twice as often), so the moon's own sidereal rate must also carry
  the sun's 0.99 deg/day: n_moon' = rate*n_synodic + n_sun. Exactly 1.0 at
  rate 1.0 — the identity is short-circuited so an undeclared rate cannot even
  round the element clock."""
  if orbit_rate == 1.0:
    return 1.0
  return (orbit_rate * _SYNODIC_RATE + _SUN_LON_RATE) / _MOON_LON_RATE


def _phase_error(current, target):
  """current - target on the synodic circle, wrapped to [-0.5, 0.5) — the
  SHORTEST way round, so a solve walks to the nearest cycle rather than
  unwinding a month."""
  return ((current - target + 0.5) % 1.0) - 0.5


class CelestialModel:
  """Observer-frame ephemeris for one site + one clock.

  latitude_deg  — observer latitude, + north.
  longitude_deg — observer longitude, + east (shifts local sidereal time only;
                  time_of_day stays UT).
  day_of_year   — 1..365, fractional allowed.
  time_of_day   — hours UT, 0..24, fractional allowed.
  time_scale    — how fast the clock runs: sim-seconds of clock per real
                  second. 3600 = one clock-hour per wall second;
                  86400/180 = 480 = a full day per 180 wall seconds (the
                  library default; see CONFIG_DEFAULTS for why it is 180).
  year          — the calendar year day_of_year belongs to. Only matters for
                  the moon's phase and the star dome's absolute roll; the sun's
                  seasonal arc is the same every year.

  moon_phase    — DECLARED synodic phase at t=0 (0..1 or a PHASE_NAMES name).
                  None = whatever the real moon did on the declared date.
  moon_initial_elevation — DECLARED moon altitude at t=0, degrees. None = the
                  ephemeris one. Declared TOGETHER with a phase it is
                  over-constrained outside a ~10-degree band and raises.
  moon_orbit_rate — synodic-period scale, 1.0 = Earth's moon; 2.0 = lunations
                  twice as fast. Composes with the two above (they are t=0
                  placement, this is propagation).
  A DECLARED value OVERRIDES the ephemeris initial and nothing else: the moon
  propagates from t=0 physically, at the declared rate. See MOON PLACEMENT.

  Evaluation is STATELESS: at(wall_seconds) is a pure function of its argument,
  so a caller may scrub, pause or replay with no drift and no hysteresis. The
  clock rolls past day 365 into the next year continuously (no wrap seam)."""

  def __init__(self, latitude_deg=45.0, day_of_year=172.0, time_of_day=12.0,
               time_scale=480.0, longitude_deg=0.0, year=2000,
               moon_phase=None, moon_initial_elevation=None,
               moon_orbit_rate=1.0):
    self.latitude_deg  = float(latitude_deg)
    self.longitude_deg = float(longitude_deg)
    self.day_of_year   = float(day_of_year)
    self.time_of_day   = float(time_of_day)
    self.time_scale    = float(time_scale)
    self.year          = int(year)
    self.moon_phase    = (None if moon_phase is None
                          else phase_fraction(moon_phase))
    self.moon_initial_elevation = (None if moon_initial_elevation is None
                                   else float(moon_initial_elevation))
    self.moon_orbit_rate = float(moon_orbit_rate)
    # days from the element epoch to the declared date+clock
    self._epoch_days = (_julian_day_jan1(self.year) - _EPOCH_JD
                        + (self.day_of_year - 1.0)
                        + self.time_of_day / 24.0)
    # the lunar re-epoch (MOON PLACEMENT). All three inert unless declared, and
    # then evaluate_days takes the untouched path — an undeclared moon is the
    # ephemeris one down to the last bit.
    self._moon_clock_scale = _moon_clock_scale(self.moon_orbit_rate)
    self._moon_epoch_shift = 0.0
    self._moon_node_shift  = 0.0
    self._moon_reepoch     = (self._moon_clock_scale != 1.0)
    if self.moon_phase is not None or self.moon_initial_elevation is not None:
      self._moon_reepoch = True
      self._solve_moon_placement()

  @classmethod
  def from_config(cls, cfg):
    """Build from a celestial= config dict (validated by normalize_config).

    Only the site/clock keys reach the model — the config also carries per-light
    policy keys (body, base_intensity) that the ephemeris knows nothing about."""
    full = normalize_config(cfg)
    return cls(**{k: full[k] for k in MODEL_CONFIG_KEYS})

  def config(self):
    return {k: getattr(self, k) for k in MODEL_CONFIG_KEYS}

  def at(self, wall_seconds=0.0):
    """Evaluate the sky `wall_seconds` of REAL time after the declared instant
    (real time × time_scale = clock time). Returns a CelestialSnapshot."""
    return self.evaluate_days(
      self._epoch_days + float(wall_seconds) * self.time_scale / 86400.0)

  def epoch_days_for(self, day_of_year, time_of_day):
    """The epoch-days value for an ARBITRARY date+hour on this model's calendar
    — the same expression __init__ uses for the declared instant. The live clock
    (_sky_time.py) needs it to place a scrubbed date; nothing else about the
    model is stateful, so this is all a date jump requires."""
    return (_julian_day_jan1(self.year) - _EPOCH_JD
            + (float(day_of_year) - 1.0)
            + float(time_of_day) / 24.0)

  ###########################################################################
  # MOON PLACEMENT — the re-epoch and the solves that set it (see the block
  # comment at the top of the file for what each term does).
  ###########################################################################

  def _moon_element_days(self, d):
    """`d` on the MOON's element clock. Bit-identical to d — same object, not
    just the same value — while nothing is declared."""
    if not self._moon_reepoch:
      return d
    return (self._epoch_days
            + (d - self._epoch_days) * self._moon_clock_scale
            + self._moon_epoch_shift)

  def _moon_at_epoch(self, shift, node):
    """Evaluate t=0 with a candidate re-epoch, LEAVING it applied — the solves
    below walk the model to its answer and stop on the accepted one."""
    self._moon_epoch_shift = shift
    self._moon_node_shift  = node
    return self.evaluate_days(self._epoch_days)

  def _solve_moon_placement(self):
    """Set the re-epoch terms so t=0 shows the declared phase and/or elevation.

    Phase alone turns the epoch shift; elevation alone turns the epoch shift
    too (the moon sweeps the whole sky over a sidereal month, so almost any
    altitude is reachable). BOTH declared is the constrained case: the phase
    owns the shift and only the orbit-plane node is left to lift or drop the
    moon, which is why the pair is refused outside its narrow band."""
    if self.moon_phase is not None and self.moon_initial_elevation is None:
      self._solve_phase(0.0)
      return
    if self.moon_phase is None:
      self._solve_elevation_by_shift()
      return

    node = 0.0
    for _ in range(12):
      shift = self._solve_phase(node)
      node  = self._solve_elevation_by_node(shift)
      snap  = self._moon_at_epoch(shift, node)
      if (abs(_phase_error(snap.moon_phase, self.moon_phase)) < 1.0e-10
          and abs(snap.moon_elevation_deg - self.moon_initial_elevation) < 1.0e-7):
        return
    raise RuntimeError(
      f"celestial config: the moon placement solve did not converge on "
      f"moon_phase={self.moon_phase} + moon_initial_elevation="
      f"{self.moon_initial_elevation} at latitude {self.latitude_deg}, day "
      f"{self.day_of_year}, {self.time_of_day} h UT (last residuals: phase "
      f"{_phase_error(snap.moon_phase, self.moon_phase):.3e}, elevation "
      f"{snap.moon_elevation_deg - self.moon_initial_elevation:.3e} deg)")

  def _solve_phase(self, node):
    """Epoch shift (days) putting t=0 on the declared phase, with the orbit
    plane held at `node`. Fixed point: a day of shift is _MOON_LON_RATE degrees
    of elongation and the periodic terms bend that by a few percent, so each
    pass kills ~97% of the error. Starts at 0, so it lands on the NEAREST
    cycle — the declared phase closest to the sky the date really had."""
    target = self.moon_phase
    shift  = 0.0
    for _ in range(64):
      snap = self._moon_at_epoch(shift, node)
      err  = _phase_error(snap.moon_phase, target)
      step = err * 360.0 / _MOON_LON_RATE
      # 1e-11 of a cycle is 4e-9 degrees of elongation — the floor the wrapped
      # double-precision phase can even represent, so a step below it is the
      # iteration chattering on the last bit, not converging further.
      if abs(err) < 1.0e-11 or abs(step) < 1.0e-12:
        return shift
      shift -= step
    raise RuntimeError(
      f"celestial config: moon_phase={target} did not converge (residual "
      f"{err:.3e} of a cycle) — the lunar re-epoch solve is broken, not the "
      f"declaration")

  def _solve_elevation_by_shift(self):
    """Epoch shift putting the moon at the declared altitude at t=0, phase free.

    Altitude is NOT monotonic in the shift (the moon rises and sets twice a
    sidereal month of shift), so the reachable set is scanned first and the
    root nearest shift=0 is taken — the smallest lie about where the moon was.
    A target the moon never reaches at this site on this day RAISES with the
    range it does reach."""
    target = self.moon_initial_elevation
    step   = 0.02                                # 0.02 d = 0.26 deg of motion
    lo     = -0.5 * _SIDEREAL_MONTH_DAYS
    n      = int(_SIDEREAL_MONTH_DAYS / step) + 1
    prev_s = lo
    prev_e = self._moon_at_epoch(prev_s, 0.0).moon_elevation_deg
    lowest = highest = prev_e
    best   = None
    for i in range(1, n + 1):
      s = lo + i * step
      e = self._moon_at_epoch(s, 0.0).moon_elevation_deg
      lowest  = min(lowest, e)
      highest = max(highest, e)
      if (e - target) == 0.0:
        root = s
      elif (prev_e - target) * (e - target) < 0.0:
        root = self._bisect_shift(prev_s, s, target, 0.0)
      else:
        root = None
      if root is not None and (best is None or abs(root) < abs(best)):
        best = root
      prev_s, prev_e = s, e
    if best is None:
      raise ValueError(
        f"celestial config: moon_initial_elevation={target} deg is unreachable "
        f"— at latitude {self.latitude_deg} on day {self.day_of_year} of "
        f"{self.year} at {self.time_of_day} h UT the moon can only be placed "
        f"between {lowest:.2f} and {highest:.2f} degrees of elevation (its "
        f"declination range over one orbit, seen at that hour angle). Declare "
        f"an elevation in that range, or move time_of_day (an hour of clock "
        f"swings the band by ~15 degrees)")
    self._moon_at_epoch(best, 0.0)

  def _solve_elevation_by_node(self, shift):
    """Node offset (degrees) lifting the moon to the declared altitude with the
    phase already pinned by `shift`. The plane turn only moves the moon within
    its +-5.15 deg of ecliptic latitude, so the reachable band is narrow and an
    unreachable pair is the OVER-CONSTRAINED case: it names both declarations,
    both of which are the scene's, and the band that would work."""
    target = self.moon_initial_elevation
    step   = 1.0
    prev_n = 0.0
    prev_e = self._moon_at_epoch(shift, prev_n).moon_elevation_deg
    lowest = highest = prev_e
    best   = None
    for i in range(1, 361):
      nn = i * step
      e  = self._moon_at_epoch(shift, nn).moon_elevation_deg
      lowest  = min(lowest, e)
      highest = max(highest, e)
      if (e - target) == 0.0:
        root = nn
      elif (prev_e - target) * (e - target) < 0.0:
        root = self._bisect_node(shift, prev_n, nn, target)
      else:
        root = None
      if root is not None:
        turn = abs(((root + 180.0) % 360.0) - 180.0)   # shortest turn from 0
        if best is None or turn < best[0]:
          best = (turn, root)
      prev_n, prev_e = nn, e
    if best is None:
      raise ValueError(
        f"celestial config: moon_phase={self.moon_phase} and "
        f"moon_initial_elevation={target} deg cannot both hold. The phase pins "
        f"the moon's ecliptic longitude against the declared clock (day "
        f"{self.day_of_year} of {self.year}, {self.time_of_day} h UT), leaving "
        f"only its 5.15 deg of orbital latitude free: at latitude "
        f"{self.latitude_deg} that puts the moon between {lowest:.2f} and "
        f"{highest:.2f} degrees of elevation. Declare an elevation in that "
        f"range, drop one of the two, or move time_of_day (an hour of clock "
        f"swings the band by ~15 degrees)")
    return best[1]

  def _bisect_shift(self, lo, hi, target, node):
    return self._bisect(lambda s: self._moon_at_epoch(s, node)
                        .moon_elevation_deg - target, lo, hi)

  def _bisect_node(self, shift, lo, hi, target):
    return self._bisect(lambda n: self._moon_at_epoch(shift, n)
                        .moon_elevation_deg - target, lo, hi)

  @staticmethod
  def _bisect(f, lo, hi):
    """Root of a bracketed continuous f, to the last bit of the bracket."""
    flo = f(lo)
    for _ in range(200):
      mid = 0.5 * (lo + hi)
      if mid == lo or mid == hi:
        break
      fmid = f(mid)
      if (flo < 0.0) == (fmid < 0.0):
        lo, flo = mid, fmid
      else:
        hi = mid
    return 0.5 * (lo + hi)

  ###########################################################################

  def evaluate_days(self, d):
    """Evaluate at `d` days from the element epoch (1999 Dec 31.0 UT)."""
    obliquity = 23.4393 - 3.563e-7 * d

    #########################################################################
    # SUN — elliptic two-body, one eccentric-anomaly correction is plenty at
    # e=0.0167. Ls is kept UNWRAPPED because the sidereal angle accumulates
    # off it.
    #########################################################################
    ws = 282.9404 + _SUN_PERIHELION_RATE * d    # longitude of perihelion
    es = 0.016709 - 1.151e-9 * d
    Ms = 356.0470 + _SUN_ANOM_RATE * d          # mean anomaly
    Ls_unwrapped = ws + Ms                      # mean longitude
    Ms_r = _rev(Ms)
    Es = Ms_r + _RAD2DEG * es * _sind(Ms_r) * (1.0 + es * _cosd(Ms_r))
    xs = _cosd(Es) - es
    ys = _sind(Es) * math.sqrt(1.0 - es * es)
    sun_r = math.hypot(xs, ys)
    sun_true_anomaly = _atan2d(ys, xs)
    sun_lon = _rev(sun_true_anomaly + ws)
    sun_ra, sun_dec = _ecliptic_to_equatorial(sun_lon, 0.0, obliquity)

    #########################################################################
    # SIDEREAL — GMST0 = mean solar longitude + 180; adding d*360 instead of
    # the clock's hour angle keeps the value ACCUMULATING (no midnight seam)
    # while staying identical mod 360. Rate = 360 + 0.9856 = 360.9856 deg/day.
    #########################################################################
    sidereal = Ls_unwrapped + 180.0 + d * 360.0 + self.longitude_deg

    #########################################################################
    # MOON — Kepler on the osculating elements, then the leading periodic
    # perturbations (evection / variation / yearly equation and friends). Those
    # terms are what make the phase land on the right day; without them the
    # moon can be over a degree off and full moons drift.
    #
    # dm is the moon's OWN element clock (MOON PLACEMENT): identical to d for an
    # undeclared moon, shifted/scaled for a declared one. The sun, the sidereal
    # angle and the obliquity above stay on d — the date does not move, only the
    # moon does. The perturbation arguments below then read the TRUE elongation
    # of the re-epoched pair (D = Lm(dm) - Ls(d)), which is what they model.
    #########################################################################
    dm = self._moon_element_days(d)
    nu = self._moon_node_shift                  # orbit-plane turn, see above
    Nm = 125.1228 - _MOON_NODE_RATE * dm + nu   # longitude of ascending node
    im = 5.1454                                 # inclination
    wm = 318.0634 + _MOON_PERIGEE_RATE * dm - nu  # argument of perigee
    am = 60.2666                                # semi-major axis, earth radii
    em = 0.054900
    Mm = 115.3654 + _MOON_ANOM_RATE * dm        # mean anomaly

    Em = Mm + _RAD2DEG * em * _sind(Mm) * (1.0 + em * _cosd(Mm))
    for _ in range(16):                         # e=0.055 needs the iteration
      delta = ((Em - _RAD2DEG * em * _sind(Em) - Mm)
               / (1.0 - em * _cosd(Em)))
      Em -= delta
      if abs(delta) < 1.0e-6:
        break
    xm = am * (_cosd(Em) - em)
    ym = am * math.sqrt(1.0 - em * em) * _sind(Em)
    moon_r = math.hypot(xm, ym)
    moon_true_anomaly = _atan2d(ym, xm)
    u = moon_true_anomaly + wm                  # argument of latitude
    xe = moon_r * (_cosd(Nm) * _cosd(u) - _sind(Nm) * _sind(u) * _cosd(im))
    ye = moon_r * (_sind(Nm) * _cosd(u) + _cosd(Nm) * _sind(u) * _cosd(im))
    ze = moon_r * _sind(u) * _sind(im)
    moon_lon = _rev(_atan2d(ye, xe))
    moon_lat = _atan2d(ze, math.hypot(xe, ye))

    Lm = Nm + wm + Mm                           # moon mean longitude
    D = Lm - Ls_unwrapped                       # mean elongation
    F = Lm - Nm                                 # argument of latitude (mean)
    moon_lon = _rev(moon_lon
                    - 1.274 * _sind(Mm - 2.0 * D)          # evection
                    + 0.658 * _sind(2.0 * D)               # variation
                    - 0.186 * _sind(Ms)                    # yearly equation
                    - 0.059 * _sind(2.0 * Mm - 2.0 * D)
                    - 0.057 * _sind(Mm - 2.0 * D + Ms)
                    + 0.053 * _sind(Mm + 2.0 * D)
                    + 0.046 * _sind(2.0 * D - Ms)
                    + 0.041 * _sind(Mm - Ms)
                    - 0.035 * _sind(D)                     # parallactic
                    - 0.031 * _sind(Mm + Ms)
                    - 0.015 * _sind(2.0 * F - 2.0 * D)
                    + 0.011 * _sind(Mm - 4.0 * D))
    moon_lat = (moon_lat
                - 0.173 * _sind(F - 2.0 * D)
                - 0.055 * _sind(Mm - F - 2.0 * D)
                - 0.046 * _sind(Mm + F - 2.0 * D)
                + 0.033 * _sind(F + 2.0 * D)
                + 0.017 * _sind(2.0 * Mm + F))
    moon_r = (moon_r
              - 0.58 * _cosd(Mm - 2.0 * D)
              - 0.46 * _cosd(2.0 * D))
    moon_ra, moon_dec = _ecliptic_to_equatorial(moon_lon, moon_lat, obliquity)

    #########################################################################
    # PHASE — synodic position from the apparent ecliptic longitude gap (that
    # gap only ever increases, so the cycle never runs backward); elongation is
    # the true angle between the two directions, which differs from the
    # longitude gap by the moon's ecliptic latitude (up to ~5 degrees).
    #########################################################################
    phase = _rev(moon_lon - sun_lon) / 360.0
    elongation = math.acos(max(-1.0, min(1.0,
                               _cosd(moon_lat) * _cosd(moon_lon - sun_lon)))) * _RAD2DEG
    phase_angle = 180.0 - elongation
    illumination = 0.5 * (1.0 + _cosd(phase_angle))

    #########################################################################

    lst = sidereal
    sun_el, sun_az = _alt_az(sun_ra, sun_dec, lst, self.latitude_deg)
    moon_el, moon_az = _alt_az(moon_ra, moon_dec, lst, self.latitude_deg)

    return CelestialSnapshot(
      epoch_days               = d,
      julian_day               = _EPOCH_JD + d,
      sun_elevation_deg        = sun_el,
      sun_azimuth_deg          = sun_az,
      sun_right_ascension_deg  = sun_ra,
      sun_declination_deg      = sun_dec,
      sun_ecliptic_lon_deg     = sun_lon,
      sun_distance_au          = sun_r,
      moon_elevation_deg       = moon_el,
      moon_azimuth_deg         = moon_az,
      moon_right_ascension_deg = moon_ra,
      moon_declination_deg     = moon_dec,
      moon_ecliptic_lon_deg    = moon_lon,
      moon_ecliptic_lat_deg    = moon_lat,
      moon_distance_er         = moon_r,
      moon_phase               = phase,
      moon_phase_angle_deg     = phase_angle,
      moon_illumination        = illumination,
      sun_moon_elongation_deg  = elongation,
      # The star dome: sidereal_angle_deg ACCUMULATES (mod it yourself if you
      # need a bounded angle); the pole sits due north at elevation = latitude,
      # which is negative — below the horizon — south of the equator.
      sidereal_angle_deg       = sidereal,
      pole_elevation_deg       = self.latitude_deg,
      pole_azimuth_deg         = 0.0,
      latitude_deg             = self.latitude_deg,
      longitude_deg            = self.longitude_deg)


__all__ = ["CelestialModel", "CelestialSnapshot",
           "CONFIG_ENV_KEY", "CONFIG_DEFAULTS", "MODEL_CONFIG_KEYS",
           "LEGAL_BODIES", "PHASE_NAMES", "normalize_config", "phase_fraction"]
