###############################################################################
# _sky_time.py — THE LIVE SKY CLOCK: one control block, shared by every
# celestial consumer in a simulation, that turns the authored time-of-day from
# a LAUNCH-TIME BAKE (SWEST_TOD / SWEST_TIMESCALE and friends: read once while
# the scene is being declared, frozen for the life of the process) into a
# RUNTIME control — scrub the hour, pause the sky, run it faster or slower,
# step the date, while the scene keeps rendering.
#
# PURE PYTHON MATH, imports nothing but `math`, exactly like _celestial.py: it
# is loaded BY PATH inside the ECS sim subinterpreter (see _celestial_orbit.py
# for why a dotted import cannot work there) by BOTH halves of the mechanism —
# sky_time_system.py (the writer: keys + programmatic messages) and
# _celestial_orbit.py (the reader: every sun/moon that aims a light).
#
# WHERE THE STATE LIVES: on the SIMULATION VARMAP under CONTROL_KEY, i.e. one
# block per simulation, not per entity and not per module. Per-sim is the whole
# point — a module global would outlive a stopped simulation and hand the next
# one a stale hour (the ecsedit restart case), an entity varmap would need the
# writer to know the reader's entity names, and the sun and the moon would then
# be able to disagree about what time it is.
#
# THE CLOCK IS A SEGMENT, NOT AN ACCUMULATOR:
#
#     epoch_days(abstime) = anchor_days + (abstime - anchor_abstime) * scale/86400
#
# Every control op REBASES (anchor to the current instant, then change the
# state), so within a segment the evaluation is a pure function of abstime —
# drift-free, scrub-exact and identical for every consumer in a frame. With no
# control ever touched the anchors stay (0.0, the authored instant) and the
# expression is arithmetically the SAME ONE CelestialModel.at() has always
# evaluated: an untouched scene's sky is bit-identical, which is what lets this
# ride under the existing byte-identity gates.
#
# UNITS: `scale` is CLOCK SECONDS PER WALL SECOND (the authored time_scale),
# `rate` multiplies it, `scrub` adds CLOCK HOURS PER WALL SECOND on top (the
# held-key fast-forward), and `paused` zeroes the authored term while leaving a
# scrub free to work — pausing and then scrubbing is the frame-accurate way to
# land on an hour.
###############################################################################

import math

# simulation-varmap key the block is published under (the ONE name the writer
# and the readers have to agree on).
CONTROL_KEY = "sky_time_control"

# Message vocabulary — the PROGRAMMATIC surface. A host sends these to the
# scene's PythonSystem (controller.systemNotify(pysystem, tokens.<NAME>, {...}));
# sky_time_system.py handles them, and the keyboard bindings are implemented by
# calling the SAME methods, so a key and a scripted control can never diverge.
# Every field listed is REQUIRED (a DataTable read of an absent key is not
# distinguishable from a zero, so the messages do not carry optional fields).
MESSAGES = {
    "SkyTimeSet":   ("hour",),    # absolute hour of the current date, 0..24
    "SkyTimeSetDay": ("day",),    # absolute day of year, 1..365 (hour kept)
    "SkyTimeStep":  ("hours",),   # relative nudge, clock hours (may be negative)
    "SkyTimeStepDay": ("days",),  # relative nudge, whole/fractional days
    "SkyTimePause": ("paused",),  # 1 = hold, 0 = run, -1 = toggle
    "SkyTimeRate":  ("rate",),    # multiplier on the AUTHORED time_scale, >= 0
    "SkyTimeScrub": ("rate",),    # clock hours per wall second, +/- (0 = off)
    "SkyTimeReset": (),           # back to the authored hour, date, rate
}


def _frac_day_hours(epoch_days):
  """Hour of day (0..24) at an epoch-days value. The element epoch is 0h UT
  (JD 2451543.5), so the fractional part of epoch_days IS the UT clock — no
  calendar arithmetic is involved in reading or setting the hour."""
  return (epoch_days - math.floor(epoch_days)) * 24.0


class SkyTimeControl:
  """The live clock + its control surface. One instance per simulation."""

  def __init__(self):
    # adopted from the first celestial model that attaches (see adopt())
    self._adopted = False
    self._model = None
    self._authored_days = 0.0     # epoch_days of the scene's declared instant
    self._authored_scale = 0.0    # authored time_scale (clock sec / wall sec)
    # the segment
    self._anchor_abstime = 0.0
    self._anchor_days = 0.0
    self._scale = 0.0             # clock sec per wall sec in effect NOW
    # the controls
    self.paused = False
    self.rate = 1.0               # multiplier on the authored scale
    self.scrub = 0.0              # clock HOURS per wall second, added on top
    # bookkeeping for the telemetry line (written by the consumers)
    self.report = {}
    self.changes = 0              # ops applied — a monotone liveness counter

  ###########################################################################
  # attach
  ###########################################################################

  def adopt(self, model):
    """First celestial model to run takes ownership of the clock's zero: the
    authored instant and the authored rate. Later models with a DIFFERENT
    authored clock are a scene declaring two skies — the control can only drive
    one, so say so out loud rather than silently steering half the sky."""
    if not self._adopted:
      self._adopted = True
      self._model = model
      self._authored_days = model._epoch_days
      self._authored_scale = model.time_scale
      self._anchor_abstime = 0.0
      self._anchor_days = model._epoch_days
      self._scale = model.time_scale
      return True
    same = (abs(model._epoch_days - self._authored_days) < 1.0e-12
            and model.time_scale == self._authored_scale)
    if not same:
      print("[sky_time] WARNING: a second celestial clock (epoch_days %.9f "
            "scale %g) differs from the adopted one (%.9f scale %g) — the live "
            "controls drive the ADOPTED clock only; this body follows it."
            % (model._epoch_days, model.time_scale,
               self._authored_days, self._authored_scale), flush=True)
    return False

  @property
  def adopted(self):
    return self._adopted

  ###########################################################################
  # evaluation — what every consumer calls, once per frame
  ###########################################################################

  def _effective_scale(self):
    base = 0.0 if self.paused else self._authored_scale * self.rate
    return base + self.scrub * 3600.0

  def days_at(self, abstime):
    """epoch_days (days from the ephemeris element epoch) at this sim time."""
    return (self._anchor_days
            + (abstime - self._anchor_abstime) * self._scale / 86400.0)

  def hour_at(self, abstime):
    return _frac_day_hours(self.days_at(abstime))

  ###########################################################################
  # the ops — every one of them rebases first, so the segment before the
  # change is evaluated with the state that was in force during it.
  ###########################################################################

  def _rebase(self, abstime):
    self._anchor_days = self.days_at(abstime)
    self._anchor_abstime = abstime
    self._scale = self._effective_scale()

  def _apply(self, abstime, mutate):
    self._rebase(abstime)
    mutate()
    self._scale = self._effective_scale()
    self.changes += 1

  def set_paused(self, abstime, paused):
    def _m():
      self.paused = bool(paused)
    self._apply(abstime, _m)

  def toggle_paused(self, abstime):
    self.set_paused(abstime, not self.paused)

  def set_rate(self, abstime, rate):
    rate = float(rate)
    if rate < 0.0:
      raise ValueError("sky time rate must be >= 0 (got %g) — run the clock "
                       "backward with a negative scrub, not a negative rate" % rate)
    def _m():
      self.rate = rate
    self._apply(abstime, _m)

  def set_scrub(self, abstime, hours_per_second):
    def _m():
      self.scrub = float(hours_per_second)
    self._apply(abstime, _m)

  def step_hours(self, abstime, hours):
    def _m():
      self._anchor_days += float(hours) / 24.0
    self._apply(abstime, _m)

  def step_days(self, abstime, days):
    def _m():
      self._anchor_days += float(days)
    self._apply(abstime, _m)

  def set_hour(self, abstime, hour):
    """Jump to an absolute hour of the CURRENT date (0..24, fractional ok)."""
    hour = float(hour)
    def _m():
      self._anchor_days = math.floor(self._anchor_days) + hour / 24.0
    self._apply(abstime, _m)

  def set_day_of_year(self, abstime, day_of_year):
    """Jump to an absolute day of the year, keeping the hour. Needs the adopted
    model's calendar (the year the day belongs to), so it is loud when no
    celestial body has attached yet rather than silently doing nothing."""
    if self._model is None:
      raise RuntimeError("sky time: set_day_of_year before any celestial body "
                         "attached — no calendar to place the day on")
    day = float(day_of_year)
    def _m():
      hour = _frac_day_hours(self._anchor_days)
      self._anchor_days = self._model.epoch_days_for(day, hour)
    self._apply(abstime, _m)

  def reset(self, abstime):
    """Back to exactly what the scene declared: hour, date and rate."""
    def _m():
      self.paused = False
      self.rate = 1.0
      self.scrub = 0.0
      self._anchor_days = self._authored_days
    self._apply(abstime, _m)

  ###########################################################################

  def state(self, abstime):
    """Everything a telemetry line or a HUD needs, as plain floats."""
    d = self.days_at(abstime)
    return {"epoch_days": d,
            "hour": _frac_day_hours(d),
            "paused": 1.0 if self.paused else 0.0,
            "rate": self.rate,
            "scrub": self.scrub,
            "scale": self._scale,
            "changes": float(self.changes)}
