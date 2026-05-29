###############################################################################
# colors.py — universal color helpers for the HyperSyn DSL.
#
# Provides:
#   - hsv(h, s, v, a=1.0): lazy HSV color, auto-coerces to vec3 or vec4 at
#     the consumption slot
#   - colors: 64-name palette (colors.red, colors.oak1, colors.skyblue1, …)
#   - _Hsv: the wrapper class; .to_vec3() / .to_vec4() materialize
#   - _PBR_VEC3_FIELDS / _PBR_VEC4_FIELDS: registry of which material fields
#     expect vec3 vs vec4 (used by the PbrMaterial dispatcher's hsv coercion)
#
# Universal — no ECS / dataflow / scenegraph dependency. Anything that
# composes color into the engine surface can use this directly.
###############################################################################

import math
from orkengine.core import vec3, vec4


def _hsv_to_rgb(h, s, v):
  """HSV → RGB tuple. Hue in degrees [0, 360], s/v in [0, 1]."""
  h = (float(h) % 360.0) / 60.0
  c = float(v) * float(s)
  x = c * (1.0 - abs((h % 2.0) - 1.0))
  m = float(v) - c
  if   h < 1.0: r, g, b = c, x, 0.0
  elif h < 2.0: r, g, b = x, c, 0.0
  elif h < 3.0: r, g, b = 0.0, c, x
  elif h < 4.0: r, g, b = 0.0, x, c
  elif h < 5.0: r, g, b = x, 0.0, c
  else:         r, g, b = c, 0.0, x
  return (r + m, g + m, b + m)


def _wavelength_to_rgb(nm):
  """Visible-spectrum wavelength in nm → RGB tuple (perceptual approximation).
  Algorithm: Dan Bruton, http://www.physics.sfasu.edu/astro/color/spectra.html.
  Outside ~380-780nm returns (0, 0, 0). Gamma-corrected for display."""
  wl = float(nm)
  if wl < 380.0 or wl > 780.0:
    return (0.0, 0.0, 0.0)
  # Piecewise primaries
  if wl < 440.0:
    r = -(wl - 440.0) / (440.0 - 380.0); g = 0.0; b = 1.0
  elif wl < 490.0:
    r = 0.0; g = (wl - 440.0) / (490.0 - 440.0); b = 1.0
  elif wl < 510.0:
    r = 0.0; g = 1.0; b = -(wl - 510.0) / (510.0 - 490.0)
  elif wl < 580.0:
    r = (wl - 510.0) / (580.0 - 510.0); g = 1.0; b = 0.0
  elif wl < 645.0:
    r = 1.0; g = -(wl - 645.0) / (645.0 - 580.0); b = 0.0
  else:                          # 645 – 780
    r = 1.0; g = 0.0; b = 0.0
  # Spectrum-edge intensity falloff (avoids hot edges at 380/780)
  if wl < 420.0:
    factor = 0.3 + 0.7 * (wl - 380.0) / 40.0
  elif wl > 700.0:
    factor = 0.3 + 0.7 * (780.0 - wl) / 80.0
  else:
    factor = 1.0
  gamma = 0.8
  r = (r * factor) ** gamma if r > 0.0 else 0.0
  g = (g * factor) ** gamma if g > 0.0 else 0.0
  b = (b * factor) ** gamma if b > 0.0 else 0.0
  return (r, g, b)


def _colortemp_to_rgb(kelvin):
  """Black-body color temperature in Kelvin → RGB tuple.
  Algorithm: Tanner Helland, clamped to [1000, 40000]K. Returns linear-ish
  RGB normalized to [0, 1]. Useful reference points:
      1900K  candle flame                  3200K  tungsten / halogen
      5500K  noon daylight                 6500K  D65 / typical monitor white
     10000K  blue sky / overcast"""
  temp = max(1000.0, min(40000.0, float(kelvin))) / 100.0
  # Red channel
  if temp <= 66.0:
    r = 255.0
  else:
    r = 329.698727446 * ((temp - 60.0) ** -0.1332047592)
  r = max(0.0, min(255.0, r))
  # Green channel
  if temp <= 66.0:
    g = 99.4708025861 * math.log(temp) - 161.1195681661 if temp > 0.0 else 0.0
  else:
    g = 288.1221695283 * ((temp - 60.0) ** -0.0755148492)
  g = max(0.0, min(255.0, g))
  # Blue channel
  if temp >= 66.0:
    b = 255.0
  elif temp <= 19.0:
    b = 0.0
  else:
    b = 138.5177312231 * math.log(temp - 10.0) - 305.0447927307
  b = max(0.0, min(255.0, b))
  return (r / 255.0, g / 255.0, b / 255.0)


class _LazyColor:
  """Lazy RGBA color — base for hsv()/wavelength()/colortemp() return values.

  Auto-converts to vec3 or vec4 at the consumption slot. Consumers that know
  whether they expect vec3 or vec4 call to_vec3() / to_vec4() to materialize.
  The PbrMaterial dispatcher does this automatically based on the field name.
  For other code paths the conversion is manual:
      ssss_node.subsurface_tint = hsv(120, 1, 1).to_vec3()

  Component-wise arithmetic:
      hsv(...) + hsv(...)       # vec4-wise add (incl. alpha)
      hsv(...) - hsv(...)       # vec4-wise subtract
      hsv(...) * hsv(...)       # vec4-wise modulate (incl. alpha)
      hsv(...) / hsv(...)       # vec4-wise divide
      hsv(...) * 0.5            # RGB * 0.5, alpha preserved (dimming intent)
      hsv(...) + 0.1            # RGB + 0.1, alpha preserved
      hsv(...).with_alpha(0.5)  # replace alpha, RGB unchanged
  Results are _RgbaColor instances — still _LazyColor, still auto-coerce
  to vec3/vec4 at the consumption slot. Math is unclamped — values can
  exceed [0,1]; clamping happens at the GPU."""

  __slots__ = ("_rgba",)

  def to_vec3(self, field_name=""):
    if self._rgba[3] != 1.0:
      raise ValueError(
        f"{self!r} → vec3 slot"
        + (f" {field_name!r}" if field_name else "")
        + ": 3-component color slots have no alpha channel. "
          "Drop the a= argument or default it to 1.0.")
    return vec3(*self._rgba[:3])

  def to_vec4(self):
    return vec4(*self._rgba)

  def with_alpha(self, a):
    """Return a copy with alpha replaced (RGB unchanged)."""
    r, g, b, _ = self._rgba
    return _RgbaColor((r, g, b, float(a)))

  # ---- Component-wise arithmetic -------------------------------------------
  # Color-color ops broadcast over all 4 channels (incl. alpha).
  # Color-scalar ops broadcast scalar across RGB only and preserve alpha
  # (so `color * 0.5` dims without making it transparent — typical art intent).

  def _as_rgba_full(self, other):
    """Coerce `other` to a 4-tuple for vec4-wise ops. NotImplemented if
    `other` isn't a color or 4-tuple (signals "try the other operand")."""
    if isinstance(other, _LazyColor):
      return other._rgba
    if isinstance(other, tuple) and len(other) == 4:
      return tuple(float(v) for v in other)
    return NotImplemented

  def _scalar(self, other):
    """If `other` is a scalar (int/float), return its float value; else None."""
    if isinstance(other, (int, float)) and not isinstance(other, bool):
      return float(other)
    return None

  def __add__(self, other):
    s = self._scalar(other)
    if s is not None:
      r, g, b, a = self._rgba
      return _RgbaColor((r + s, g + s, b + s, a))      # RGB+scalar, alpha kept
    o = self._as_rgba_full(other)
    if o is NotImplemented: return NotImplemented
    return _RgbaColor(tuple(x + y for x, y in zip(self._rgba, o)))
  __radd__ = __add__

  def __sub__(self, other):
    s = self._scalar(other)
    if s is not None:
      r, g, b, a = self._rgba
      return _RgbaColor((r - s, g - s, b - s, a))
    o = self._as_rgba_full(other)
    if o is NotImplemented: return NotImplemented
    return _RgbaColor(tuple(x - y for x, y in zip(self._rgba, o)))

  def __rsub__(self, other):
    s = self._scalar(other)
    if s is not None:
      r, g, b, a = self._rgba
      return _RgbaColor((s - r, s - g, s - b, a))
    o = self._as_rgba_full(other)
    if o is NotImplemented: return NotImplemented
    return _RgbaColor(tuple(y - x for x, y in zip(self._rgba, o)))

  def __mul__(self, other):
    s = self._scalar(other)
    if s is not None:
      r, g, b, a = self._rgba
      return _RgbaColor((r * s, g * s, b * s, a))      # dim RGB, keep alpha
    o = self._as_rgba_full(other)
    if o is NotImplemented: return NotImplemented
    return _RgbaColor(tuple(x * y for x, y in zip(self._rgba, o)))
  __rmul__ = __mul__

  def __truediv__(self, other):
    s = self._scalar(other)
    if s is not None:
      r, g, b, a = self._rgba
      return _RgbaColor((r / s, g / s, b / s, a))
    o = self._as_rgba_full(other)
    if o is NotImplemented: return NotImplemented
    return _RgbaColor(tuple(x / y for x, y in zip(self._rgba, o)))

  def __neg__(self):
    r, g, b, a = self._rgba
    return _RgbaColor((-r, -g, -b, a))


class _RgbaColor(_LazyColor):
  """Result of arithmetic on _LazyColor instances. Stores raw RGBA — no
  origin parameters to preserve, just the materialized channels."""

  __slots__ = ()

  def __init__(self, rgba):
    self._rgba = tuple(float(v) for v in rgba)

  def __repr__(self):
    r, g, b, a = self._rgba
    return f"rgba({r:.4g}, {g:.4g}, {b:.4g}, a={a:.4g})"


class _Hsv(_LazyColor):
  """HSV-constructed lazy color (returned by hsv())."""

  __slots__ = ("h", "s", "v", "a")

  def __init__(self, h, s, v, a):
    self.h = float(h); self.s = float(s); self.v = float(v); self.a = float(a)
    r, g, b = _hsv_to_rgb(self.h, self.s, self.v)
    self._rgba = (r, g, b, self.a)

  def __repr__(self):
    return f"hsv({self.h}, {self.s}, {self.v}, a={self.a})"


class _Wavelength(_LazyColor):
  """Wavelength-constructed lazy color (returned by wavelength())."""

  __slots__ = ("nm", "a")

  def __init__(self, nm, a):
    self.nm = float(nm); self.a = float(a)
    r, g, b = _wavelength_to_rgb(self.nm)
    self._rgba = (r, g, b, self.a)

  def __repr__(self):
    return f"wavelength({self.nm}, a={self.a})"


class _ColorTemp(_LazyColor):
  """Color-temperature-constructed lazy color (returned by colortemp())."""

  __slots__ = ("k", "a")

  def __init__(self, k, a):
    self.k = float(k); self.a = float(a)
    r, g, b = _colortemp_to_rgb(self.k)
    self._rgba = (r, g, b, self.a)

  def __repr__(self):
    return f"colortemp({self.k}, a={self.a})"


def hsv(h, s, v, a=1.0):
  """HSV(A) → color. Auto-converts to vec3 or vec4 at the consumption slot.

  Hue in degrees [0, 360], saturation/value/alpha in [0, 1].

  Examples:
      base_color       = hsv(0,   1.0, 1.0)        # → vec4 red opaque
      base_color       = hsv(0,   1.0, 1.0, 0.5)   # → vec4 red half-transparent
      subsurface_color = hsv(20,  0.6, 1.0)        # → vec3 warm pink
      subsurface_color = hsv(20,  0.6, 1.0, 0.7)   # → ValueError (alpha in vec3 slot)"""
  return _Hsv(h, s, v, a)


def wavelength(nm, a=1.0):
  """Visible-spectrum wavelength (nm) → color. Same auto-coercion as hsv().

  Useful reference points:
      405nm  violet laser    532nm  green laser    650nm  red laser
      450nm  blue / sky      555nm  peak photopic sensitivity (green)
      589nm  sodium-D / classic streetlamp yellow

  Outside ~380-780nm returns black."""
  return _Wavelength(nm, a)


def colortemp(k, a=1.0):
  """Black-body color temperature (Kelvin) → color. Same auto-coercion.

  Useful reference points:
      1900K  candle flame              3200K  tungsten / halogen
      5500K  noon daylight             6500K  D65 / monitor white
     10000K  overcast / open shade

  Clamped to [1000, 40000]K."""
  return _ColorTemp(k, a)


def mix(a, b, t=0.5):
  """Linear interpolation between two colors. Operates on all 4 channels
  including alpha, so mix(opaque, opaque) stays opaque (unlike `a + b`
  which sums the alphas to 2).

      mix(colors.jade, colors.skyblue1)          # midpoint
      mix(colors.jade, colors.skyblue1, 0.25)    # 25% sky into jade
      mix(hsv(0,1,1), hsv(240,1,1), Expr.time)   # animated transition

  `a` and `b` may be _LazyColor or any RGBA-shaped 4-tuple; `t` is a scalar
  in [0, 1] (unclamped — extrapolation works too)."""
  ra = a._rgba if isinstance(a, _LazyColor) else tuple(float(v) for v in a)
  rb = b._rgba if isinstance(b, _LazyColor) else tuple(float(v) for v in b)
  t = float(t)
  return _RgbaColor(tuple((1.0 - t) * x + t * y for x, y in zip(ra, rb)))


# Color slot kinds, used by PbrMaterial._coerce_hsv. Update when new
# reflected color fields land on the material — fields not listed here
# default to vec3 (the common case across all glTF lobes).
_PBR_VEC4_FIELDS = frozenset({"base_color"})
_PBR_VEC3_FIELDS = frozenset({
  "subsurface_color", "subsurface_radius",
  "sheen_color", "specular_color",
  "attenuation_color", "diffuse_transmission_color",
})


###############################################################################
# Named color palette — 64 commonly useful colors as lazy _Hsv values.
# Each auto-coerces to vec3 or vec4 at the consumption slot, same as hsv().
# Used as `colors.red`, `colors.oak1`, `colors.skyblue1`, etc.
#
# Conventions:
#   - Numbered variants (oak1/oak2, skin1..6, leaf1/leaf2, skyblue1/skyblue2)
#     go light→dark or near→far. skyblue1 = horizon-paler; skyblue2 = zenith.
#   - All values are HSV with H in degrees [0, 360], S/V in [0, 1], alpha=1.
#   - Override via colors.red.to_vec4() / .to_vec3() if explicit type needed.
###############################################################################

class _Colors:
  """64 named colors. Each auto-coerces to vec3 or vec4 at use site."""
  __slots__ = ()

  # ---- Primaries / secondaries (12) ----
  red       = _Hsv(0,   1.00, 1.00, 1.0)
  green     = _Hsv(120, 1.00, 1.00, 1.0)
  blue      = _Hsv(240, 1.00, 1.00, 1.0)
  yellow    = _Hsv(60,  1.00, 1.00, 1.0)
  cyan      = _Hsv(180, 1.00, 1.00, 1.0)
  magenta   = _Hsv(300, 1.00, 1.00, 1.0)
  orange    = _Hsv(30,  1.00, 1.00, 1.0)
  purple    = _Hsv(270, 0.80, 0.70, 1.0)
  pink      = _Hsv(330, 0.40, 1.00, 1.0)
  lime      = _Hsv(80,  0.95, 1.00, 1.0)
  teal      = _Hsv(180, 0.70, 0.60, 1.0)
  indigo    = _Hsv(255, 0.70, 0.50, 1.0)

  # ---- Neutrals (8) ----
  white     = _Hsv(0,   0.00, 1.00, 1.0)
  black     = _Hsv(0,   0.00, 0.00, 1.0)
  gray      = _Hsv(0,   0.00, 0.50, 1.0)
  lightgray = _Hsv(0,   0.00, 0.75, 1.0)
  darkgray  = _Hsv(0,   0.00, 0.25, 1.0)
  charcoal  = _Hsv(0,   0.00, 0.15, 1.0)
  cream     = _Hsv(40,  0.10, 0.97, 1.0)
  tan       = _Hsv(33,  0.33, 0.82, 1.0)

  # ---- Skin tones, light → dark (6) ----
  skin1     = _Hsv(20,  0.20, 0.95, 1.0)   # pale pink
  skin2     = _Hsv(22,  0.35, 0.90, 1.0)   # warm peach
  skin3     = _Hsv(30,  0.40, 0.78, 1.0)   # olive
  skin4     = _Hsv(25,  0.50, 0.60, 1.0)   # tan brown
  skin5     = _Hsv(20,  0.55, 0.40, 1.0)   # deep brown
  skin6     = _Hsv(15,  0.60, 0.25, 1.0)   # very dark

  # ---- Wood (4) ----
  oak1      = _Hsv(35,  0.40, 0.75, 1.0)   # pale oak
  oak2      = _Hsv(25,  0.55, 0.45, 1.0)   # aged oak
  mahogany  = _Hsv(10,  0.70, 0.40, 1.0)
  birch     = _Hsv(35,  0.15, 0.92, 1.0)

  # ---- Stone / minerals (6) ----
  granite   = _Hsv(0,   0.02, 0.42, 1.0)
  marble    = _Hsv(50,  0.05, 0.92, 1.0)
  slate     = _Hsv(220, 0.15, 0.40, 1.0)
  jade      = _Hsv(140, 0.55, 0.55, 1.0)
  ruby      = _Hsv(355, 0.85, 0.55, 1.0)
  sapphire  = _Hsv(220, 0.80, 0.50, 1.0)

  # ---- Plants (6) ----
  leaf1     = _Hsv(95,  0.70, 0.85, 1.0)   # spring green
  leaf2     = _Hsv(110, 0.65, 0.45, 1.0)   # forest
  grass     = _Hsv(90,  0.75, 0.60, 1.0)
  moss      = _Hsv(75,  0.45, 0.40, 1.0)
  sage      = _Hsv(85,  0.25, 0.65, 1.0)
  autumn    = _Hsv(20,  0.85, 0.70, 1.0)   # foliage orange

  # ---- Sky / water (8) ----
  skyblue1  = _Hsv(210, 0.35, 0.95, 1.0)   # horizon, paler
  skyblue2  = _Hsv(220, 0.70, 0.85, 1.0)   # zenith, deeper
  sunset    = _Hsv(15,  0.75, 0.95, 1.0)
  dawn      = _Hsv(20,  0.40, 0.95, 1.0)
  fog       = _Hsv(210, 0.05, 0.85, 1.0)
  ocean     = _Hsv(210, 0.60, 0.40, 1.0)
  lagoon    = _Hsv(180, 0.55, 0.70, 1.0)
  lake      = _Hsv(205, 0.50, 0.45, 1.0)

  # ---- Metals (6) ----
  gold      = _Hsv(45,  0.85, 0.95, 1.0)
  silver    = _Hsv(0,   0.02, 0.85, 1.0)
  copper    = _Hsv(20,  0.60, 0.75, 1.0)
  brass     = _Hsv(45,  0.55, 0.75, 1.0)
  iron      = _Hsv(220, 0.05, 0.35, 1.0)
  rust      = _Hsv(15,  0.75, 0.50, 1.0)

  # ---- Fire / warm (4) ----
  ember     = _Hsv(15,  1.00, 0.40, 1.0)   # deep red-orange
  flame     = _Hsv(30,  1.00, 1.00, 1.0)   # bright
  terracotta = _Hsv(15, 0.55, 0.70, 1.0)   # warm clay
  coral     = _Hsv(7,   0.55, 1.00, 1.0)

  # ---- Food / misc (4) ----
  chocolate = _Hsv(20,  0.60, 0.35, 1.0)
  wine      = _Hsv(350, 0.70, 0.30, 1.0)   # burgundy
  lavender  = _Hsv(265, 0.30, 0.85, 1.0)
  salmon    = _Hsv(10,  0.50, 1.00, 1.0)


colors = _Colors()


__all__ = [
  "hsv", "wavelength", "colortemp", "mix", "colors",
  "_LazyColor", "_Hsv", "_Wavelength", "_ColorTemp", "_RgbaColor",
  "_PBR_VEC3_FIELDS", "_PBR_VEC4_FIELDS",
]
