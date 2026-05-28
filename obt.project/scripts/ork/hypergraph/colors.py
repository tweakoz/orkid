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


class _Hsv:
  """Lazy HSV color — auto-converts to vec3 or vec4 at the consumption slot.

  Returned by hsv(). Consumers that know whether they expect vec3 or vec4
  call to_vec3() / to_vec4() to materialize. The PbrMaterial dispatcher
  does this automatically based on the field name. For other code paths
  the conversion is manual: ssss_node.subsurface_tint = hsv(120, 1, 1).to_vec3()."""

  __slots__ = ("h", "s", "v", "a", "_rgba")

  def __init__(self, h, s, v, a):
    self.h = float(h); self.s = float(s); self.v = float(v); self.a = float(a)
    r, g, b = _hsv_to_rgb(self.h, self.s, self.v)
    self._rgba = (r, g, b, self.a)

  def to_vec3(self, field_name=""):
    if self.a != 1.0:
      raise ValueError(
        f"hsv({self.h}, {self.s}, {self.v}, a={self.a}) → vec3 slot"
        + (f" {field_name!r}" if field_name else "")
        + f": 3-component color slots have no alpha channel. "
          "Drop the a= argument or default it to 1.0.")
    return vec3(*self._rgba[:3])

  def to_vec4(self):
    return vec4(*self._rgba)

  def __repr__(self):
    return f"hsv({self.h}, {self.s}, {self.v}, a={self.a})"


def hsv(h, s, v, a=1.0):
  """HSV(A) → color. Auto-converts to vec3 or vec4 at the consumption slot.

  Hue in degrees [0, 360], saturation/value/alpha in [0, 1].

  Examples:
      base_color       = hsv(0,   1.0, 1.0)        # → vec4 red opaque
      base_color       = hsv(0,   1.0, 1.0, 0.5)   # → vec4 red half-transparent
      subsurface_color = hsv(20,  0.6, 1.0)        # → vec3 warm pink
      subsurface_color = hsv(20,  0.6, 1.0, 0.7)   # → ValueError (alpha in vec3 slot)"""
  return _Hsv(h, s, v, a)


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


__all__ = ["hsv", "colors", "_Hsv", "_PBR_VEC3_FIELDS", "_PBR_VEC4_FIELDS"]
