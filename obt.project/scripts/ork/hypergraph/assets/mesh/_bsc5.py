###############################################################################
# _bsc5.py — the BRIGHT STAR CATALOGUE reader, plus the physical chain that
# turns a catalog row into the three things a star splat needs: a DOME-SPACE
# direction, a linear relative luminance, and a linear-sRGB tint.
#
# The catalog itself is committed raw under ork.data/src/catalogs/bsc5 (see the
# PROVENANCE.md there for the compilation, the archive identifiers and the
# license position); this module is the only thing that reads it, and it parses
# the fixed-width byte columns straight out of the committed ReadMe.
#
# ENGINE-FREE BY CONSTRUCTION — stdlib only, like _celestial.py. The bake runs
# at asset-import time inside the engine, but the whole convention/photometry
# chain is testable under a bare python3 with no staging and no GPU, which is
# what obt.project/unittests/star_catalog.py does.
#
###############################################################################
# DOME-SPACE CONVENTION — the derivation
#
# The star-dome entity is oriented every frame by
#
#     Q = tilt(+X, phi - 90) * spin(+Y, -theta)
#
# (_star_dome.py onUpdate; CelestialSnapshot.star_dome_quat in _celestial.py),
# where phi = observer latitude and theta = local sidereal angle. The world
# frame is +X east / +Y up / +Z south, and _celestial._alt_az + _sky_vector put
# a star of right ascension A and declination d at world direction
#
#     w = ( -cos d sin H,
#            sin phi sin d + cos phi cos d cos H,
#           -(sin d cos phi - cos d sin phi cos H) ),      H = theta - A
#
# So the baked object-space vector v is the solution of Q v = w.
#
#   1. Undo the tilt. cos(phi-90) = sin phi and sin(phi-90) = -cos phi, so
#      Rx(phi-90) p = ( p.x, p.y sin phi + p.z cos phi, -p.y cos phi + p.z sin phi ).
#      Matching that against w component by component gives, uniquely,
#          p = ( -cos d sin H, sin d, cos d cos H )
#      — the EQUATORIAL frame with +Y on the celestial pole and the hour angle
#      measured about it.
#   2. Undo the spin. Ry(-theta) v = ( v.x cos theta - v.z sin theta, v.y,
#      v.x sin theta + v.z cos theta ). Substituting H = theta - A and expanding
#      sin(theta-A) / cos(theta-A) matches term for term with
#
#          v = ( cos d sin A,  sin d,  cos d cos A )
#
# i.e. object +Y = NORTH CELESTIAL POLE, object +Z = RA 0h on the equator, and
# RA increases from +Z toward +X. NEITHER LATITUDE NOR THE CLOCK APPEARS — that
# is the point: the dome's orientation carries the entire observer dependence,
# so one baked vertex buffer serves every site and every instant.
#
###############################################################################
# PHOTOMETRY
#
#   luminance — EXACT Pogson: L = 10^(-0.4 V), relative to V=0 (so V=0 gives
#     1.0 and a 5-magnitude step is a factor of exactly 100). No clamping, no
#     curve shaping, no gain: the whole point of baking the real catalog is that
#     the relative brightnesses are physically right, and twilight visibility
#     downstream depends on that ratio. The brightest star (Sirius, V=-1.46)
#     lands at 3.83 and the faintest BSC entry (V=7.96) at 6.5e-4.
#
#   tint — B-V color index -> effective temperature -> blackbody spectrum ->
#     CIE XYZ -> linear sRGB -> normalized to peak 1 (a pure CHROMA; all the
#     magnitude lives in the luminance above). The steps are published, in
#     order: Ballesteros (2012, EPL 97, 34008) for B-V -> T_eff; Planck's law
#     for the spectrum; the Wyman/Sklar/Skala multi-lobe Gaussian fit to the
#     CIE 1931 2-degree color matching functions (JCGT 2(2), 2013) for the
#     integration; the IEC 61966-2-1 D65 matrix for XYZ -> linear sRGB.
#     Blackbody radiance is used up to a constant factor (c1 cancels under the
#     peak normalization). Out-of-gamut negatives are clipped to 0.
###############################################################################

import functools
import gzip
import math
import os

# ReadMe, File Summary: catalog has 9110 records, Lrecl 197.
CATALOG_RECORDS = 9110
# ReadMe, Description: "of which 9096 are stars (14 objects ... are novae or
# extragalactic objects that have been retained to preserve the numbering, but
# most of their data are omitted)".
CATALOG_STARS = 9096

# Those 14, by HR number — every one of them has blank J2000 coordinates AND a
# blank V magnitude, which is how the parser finds them (the HR list is the
# cross-check, not the filter).
NON_STELLAR_HR = (92, 95, 182, 1057, 1841, 2472, 2496,
                  3515, 3671, 6309, 6515, 7189, 7539, 8296)

# 310 stars carry a position and a V magnitude but no B-V. They are tinted as
# if B-V = 0, which is not a taste choice: the UBV system is DEFINED so that an
# unreddened A0V star has B-V = 0, making it the system's own zero point.
MISSING_BV = 0.0

_CATALOG_RELPATH = ("ork.data", "src", "catalogs", "bsc5", "catalog.gz")
_CATALOG_ENV = "ORKID_BSC5_CATALOG"


def _tree_root():
  """Repo root, derived from THIS FILE's location
  (<root>/obt.project/scripts/ork/hypergraph/assets/mesh/_bsc5.py).

  Deliberately not $ORKID_WORKSPACE_DIR: the catalog and this parser are
  committed in the same tree, so the file's own path is the one anchor that
  cannot resolve into a different checkout (a lane worktree run under another
  workspace's env would otherwise read that workspace's data, or nothing). It
  also lets a bare python3 test run find the catalog with no environment."""
  here = os.path.dirname(os.path.abspath(__file__))
  return os.path.abspath(os.path.join(here, *([os.pardir] * 6)))


def catalog_path():
  """Absolute path of the committed catalog.gz ($ORKID_BSC5_CATALOG overrides).
  Missing = a hard error naming the path: a star field silently baked from
  nothing is a black sky nobody can debug."""
  path = os.environ.get(_CATALOG_ENV) or os.path.join(_tree_root(),
                                                      *_CATALOG_RELPATH)
  if not os.path.isfile(path):
    raise FileNotFoundError(
      "BSC5 catalog not found at %r — it is committed raw under "
      "ork.data/src/catalogs/bsc5 (see PROVENANCE.md there); set $%s to point "
      "elsewhere." % (path, _CATALOG_ENV))
  return path


###############################################################################
# photometry
###############################################################################


def pogson_luminance(vmag):
  """Linear luminance relative to V=0: L = 10^(-0.4 V). EXACT Pogson — this
  function has no parameters and must never grow any."""
  return 10.0 ** (-0.4 * float(vmag))


def bv_to_temperature(bv):
  """B-V color index -> effective temperature in kelvin, Ballesteros (2012):

      T = 4600 * ( 1/(0.92(B-V) + 1.7) + 1/(0.92(B-V) + 0.62) )

  Fit over roughly -0.4 < B-V < 2.0; the BSC's handful of extreme carbon stars
  (B-V up to 5.74) run past it and come out very cool, which is the right
  direction if not a defensible number. Non-positive output is impossible for
  the catalog's B-V range and is refused rather than propagated."""
  bv = float(bv)
  d1 = 0.92 * bv + 1.7
  d2 = 0.92 * bv + 0.62
  if d1 <= 0.0 or d2 <= 0.0:
    raise ValueError("bv_to_temperature: B-V %g is outside the formula's pole "
                     "(0.92*B-V + 0.62 must stay positive)" % bv)
  t = 4600.0 * (1.0 / d1 + 1.0 / d2)
  if not (t > 0.0):
    raise ValueError("bv_to_temperature: non-physical temperature %r for "
                     "B-V %g" % (t, bv))
  return t


# Wyman/Sklar/Skala multi-lobe fit to the CIE 1931 2-degree observer, as
# (weight, center_nm, sigma_below, sigma_above) lobes per channel.
_CIE_X_LOBES = ((1.056, 599.8, 37.9, 31.0),
                (0.362, 442.0, 16.0, 26.7),
                (-0.065, 501.1, 20.4, 26.2))
_CIE_Y_LOBES = ((0.821, 568.8, 46.9, 40.5),
                (0.286, 530.9, 16.3, 31.1))
_CIE_Z_LOBES = ((1.217, 437.0, 11.8, 36.0),
                (0.681, 459.0, 26.0, 13.8))

_SPECTRUM_NM = tuple(range(360, 831))     # the fit's stated support
_PLANCK_C2 = 1.438776877e-2               # hc/k, meter-kelvin (CODATA)


def _lobe(x, mu, s_lo, s_hi):
  t = (x - mu) / (s_lo if x < mu else s_hi)
  return math.exp(-0.5 * t * t)


def _cie_xyz_bar(nm):
  return (sum(w * _lobe(nm, m, a, b) for w, m, a, b in _CIE_X_LOBES),
          sum(w * _lobe(nm, m, a, b) for w, m, a, b in _CIE_Y_LOBES),
          sum(w * _lobe(nm, m, a, b) for w, m, a, b in _CIE_Z_LOBES))


_CIE_BAR = tuple((nm, _cie_xyz_bar(nm)) for nm in _SPECTRUM_NM)


def _planck(nm, temperature_k):
  """Spectral radiance up to the constant c1, which cancels under the peak
  normalization: B ~ lambda^-5 / (exp(hc/(lambda k T)) - 1)."""
  lam = nm * 1.0e-9
  return 1.0 / (lam ** 5 * math.expm1(_PLANCK_C2 / (lam * temperature_k)))


@functools.lru_cache(maxsize=None)
def blackbody_linear_srgb(temperature_k):
  """Blackbody at `temperature_k` -> linear sRGB normalized to peak 1.

  Cached because the tint is a pure function of the temperature, which is a
  pure function of B-V, and the catalog holds only ~253 distinct B-V values —
  so the 471-sample integral runs a couple hundred times, not 9096."""
  temperature_k = float(temperature_k)
  if temperature_k <= 0.0:
    raise ValueError("blackbody_linear_srgb: temperature must be positive; "
                     "got %r" % temperature_k)
  X = Y = Z = 0.0
  for nm, (xb, yb, zb) in _CIE_BAR:
    p = _planck(nm, temperature_k)
    X += p * xb
    Y += p * yb
    Z += p * zb
  rgb = (3.2406 * X - 1.5372 * Y - 0.4986 * Z,
         -0.9689 * X + 1.8758 * Y + 0.0415 * Z,
         0.0557 * X - 0.2040 * Y + 1.0570 * Z)
  rgb = tuple(max(0.0, c) for c in rgb)          # clip out-of-gamut negatives
  peak = max(rgb)
  if peak <= 0.0:
    raise ValueError("blackbody_linear_srgb: %gK integrated to no visible "
                     "response" % temperature_k)
  return tuple(c / peak for c in rgb)


def bv_to_linear_srgb(bv):
  """B-V -> linear sRGB tint, peak-normalized to 1 (chroma only)."""
  return blackbody_linear_srgb(bv_to_temperature(bv))


###############################################################################
# dome space
###############################################################################


def dome_direction(ra_deg, dec_deg):
  """(RA, dec) J2000 degrees -> unit direction in STAR-DOME OBJECT SPACE.

  +Y is the north celestial pole, +Z is RA 0h on the equator, RA increases
  toward +X. See the derivation in this module's header: this is exactly the
  mapping that makes tilt(phi-90) * spin(-theta) land the star where
  _celestial._alt_az puts it."""
  a = math.radians(float(ra_deg))
  d = math.radians(float(dec_deg))
  cd = math.cos(d)
  return (cd * math.sin(a), math.sin(d), cd * math.cos(a))


###############################################################################
# the catalog
###############################################################################


class Star:
  """One BSC5 row reduced to what the splat bake consumes.

  direction — unit vector in dome object space.
  luminance — linear, relative to V=0 (exact Pogson).
  tint      — linear sRGB chroma, peak-normalized to 1.
  radiance  — tint * luminance, the product the vertex buffer carries."""

  __slots__ = ("hr", "name", "ra_deg", "dec_deg", "vmag", "bv", "has_bv",
               "direction", "luminance", "tint")

  def __init__(self, hr, name, ra_deg, dec_deg, vmag, bv, has_bv):
    self.hr        = hr
    self.name      = name
    self.ra_deg    = ra_deg
    self.dec_deg   = dec_deg
    self.vmag      = vmag
    self.bv        = bv
    self.has_bv    = has_bv
    self.direction = dome_direction(ra_deg, dec_deg)
    self.luminance = pogson_luminance(vmag)
    self.tint      = bv_to_linear_srgb(bv)

  @property
  def radiance(self):
    return tuple(c * self.luminance for c in self.tint)

  def __repr__(self):
    return ("Star(HR %d %r RA=%.4f Dec=%.4f V=%.2f B-V=%.2f)"
            % (self.hr, self.name, self.ra_deg, self.dec_deg,
               self.vmag, self.bv))


def _col(line, first, last):
  """ReadMe byte columns are 1-based and inclusive on both ends."""
  return line[first - 1:last]


def load_catalog(path=None, verbose=True):
  """Parse the committed BSC5 -> (list[Star], stats dict).

  Filters the records that carry neither J2000 coordinates nor a V magnitude —
  the 14 non-stellar entries the ReadMe describes. Both the record total and
  the identity of that filtered set are ASSERTED: a truncated download or a
  substituted file must fail here, loudly, rather than quietly bake a partial
  sky that looks merely dim."""
  path = path or catalog_path()
  with gzip.open(path, "rt", encoding="latin-1") as fh:
    lines = fh.read().splitlines()

  if len(lines) != CATALOG_RECORDS:
    raise ValueError(
      "BSC5 %r: expected %d records (ReadMe File Summary), got %d — the "
      "committed catalog is byte-exact from CDS and must not be edited"
      % (path, CATALOG_RECORDS, len(lines)))

  stars    = []
  filtered = []
  no_bv    = 0
  for line in lines:
    line = line.ljust(197)
    hr   = int(_col(line, 1, 4))
    ra_s = _col(line, 76, 90)
    v_s  = _col(line, 103, 107).strip()
    if not ra_s.strip() or not v_s:
      filtered.append(hr)
      continue
    ra_deg = (int(_col(line, 76, 77))
              + int(_col(line, 78, 79)) / 60.0
              + float(_col(line, 80, 83)) / 3600.0) * 15.0
    dec = (int(_col(line, 85, 86))
           + int(_col(line, 87, 88)) / 60.0
           + int(_col(line, 89, 90)) / 3600.0)
    if _col(line, 84, 84) == "-":
      dec = -dec
    bv_s = _col(line, 110, 114).strip()
    if not bv_s:
      no_bv += 1
    stars.append(Star(hr        = hr,
                      name      = _col(line, 5, 14).strip(),
                      ra_deg    = ra_deg,
                      dec_deg   = dec,
                      vmag      = float(v_s),
                      bv        = float(bv_s) if bv_s else MISSING_BV,
                      has_bv    = bool(bv_s)))

  if tuple(filtered) != NON_STELLAR_HR:
    raise ValueError(
      "BSC5 %r: the coordinate-less records are %s, but the ReadMe's 14 "
      "non-stellar entries are %s" % (path, filtered, list(NON_STELLAR_HR)))
  if len(stars) != CATALOG_STARS:
    raise ValueError("BSC5 %r: expected %d stars, parsed %d"
                     % (path, CATALOG_STARS, len(stars)))

  stats = {"records":  len(lines),
           "stars":    len(stars),
           "filtered": tuple(filtered),
           "no_bv":    no_bv,
           "path":     path}
  if verbose:
    print("BSC5: %d records, %d stars, %d non-stellar filtered %s, "
          "%d without B-V (tinted at B-V=%g)"
          % (stats["records"], stats["stars"], len(filtered), list(filtered),
             no_bv, MISSING_BV), flush=True)
  return stars, stats


__all__ = ["CATALOG_RECORDS", "CATALOG_STARS", "NON_STELLAR_HR", "MISSING_BV",
           "Star", "catalog_path", "load_catalog", "dome_direction",
           "pogson_luminance", "bv_to_temperature", "blackbody_linear_srgb",
           "bv_to_linear_srgb"]
