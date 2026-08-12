###############################################################################
# card_bake — bake a UV-CARD material's FIELD FUNCTION to one shared RGBA
# texture, content-addressed (the stored-materials front door for leaf/needle
# cards, aug09).
#
# THE CONTRACT (what makes this general — it must survive look tuning):
#   * A card material's LOOK lives in ONE plain function, its "fields fn":
#         fields_fn(P, u, v, ramp, **params) -> (alpha, shade, transl, cavity)
#     - `P`     the op protocol (the shared elementwise subset of the ptex3d
#               P ops — abs/sin/cos/pow/fract/floor/mix/smoothstep/min/max/
#               clamp/step/saturate/sqrt/mod)
#     - `u`,`v` card UV (0..1; v = petiole..tip)
#     - `ramp`  ramp(x, e0, e1) -> 0..1 smooth edge (LIVE: the material's
#               bandlimited aa_ramp; BAKE: plain smoothstep — mips then own
#               the distance filtering)
#     - returns FOUR 0..1 fields (the card.v2 channel layout):
#         alpha   silhouette coverage
#         shade   grayscale albedo-structure pattern (1 = full albedo)
#         transl  thin-blade TRANSLUCENCY mask (1 = light passes; veins/stems
#                 low) — drives the live sun backlight term
#         cavity  cavity/AO (1 = open, <1 = groove/crowded) — feeds the ao lobe
#   * The LIVE material class calls it with DSL Ops (GLSL emission); the BAKER
#     calls it with numpy arrays. SAME BODY, both realizations — the bake can
#     never drift from the authored look, and any edit to the function (or its
#     params) re-keys the cache and re-cooks. An op outside the protocol fails
#     LOUDLY (AttributeError on NumpyP) — extend NumpyP + use the matching
#     P op, never fork the look.
#   * Everything PER-INSTANCE (ctx.Cd seeds) or PER-ASSET (tint constants)
#     stays OUT of the fields fn — applied live by the stored sampler class.
#
# OUTPUT: RGBA8 PNG, r = shade, g = transl, b = cavity (all gutter-dilated
# into transparent texels so minification never pulls black), a = straight
# alpha (engine PNG load is pinned unassociated). Cache:
#   <assetcache>/ptex3d_capture/card/<key>__r<res>.png
# The returned path keeps the <assetcache> token (both materializers expand it)
# so the .ecs stays machine-portable.
###############################################################################
import hashlib
import inspect
import os

import numpy as np

from orkengine.core import Path as _Path

# format epoch — bump to invalidate every card cache (layout/semantic changes)
# v2: fields contract grew to (alpha, shade, transl, cavity); rgb repacked
#     r=shade g=transl b=cavity (v1 replicated shade into g/b)
CARD_BAKE_EPOCH = "ptex3d.card.v2"

# gutter-dilation ring count (mirrors section_bake's mip-0 footprint rationale)
_CARD_DILATE_TEXELS = 4


class NumpyP:
  """numpy realization of the card fields op protocol (the elementwise subset of
  ptex3d's P). Keep in lockstep with what fields fns use; missing ops raise
  AttributeError LOUDLY by construction."""
  @staticmethod
  def abs(x):    return np.abs(x)
  @staticmethod
  def sin(x):    return np.sin(x)
  @staticmethod
  def cos(x):    return np.cos(x)
  @staticmethod
  def sqrt(x):   return np.sqrt(x)
  @staticmethod
  def pow(x, e): return np.power(x, e)
  @staticmethod
  def fract(x):  return x - np.floor(x)
  @staticmethod
  def floor(x):  return np.floor(x)
  @staticmethod
  def mod(a, b): return a - np.floor(a / b) * b       # GLSL mod (sign of b)
  @staticmethod
  def mix(a, b, t): return a + (b - a) * t
  @staticmethod
  def min(a, b): return np.minimum(a, b)
  @staticmethod
  def max(a, b): return np.maximum(a, b)
  @staticmethod
  def clamp(x, lo, hi): return np.clip(x, lo, hi)
  @staticmethod
  def saturate(x): return np.clip(x, 0.0, 1.0)
  @staticmethod
  def step(edge, x): return (np.asarray(x) >= edge).astype(np.float64)
  @staticmethod
  def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / np.maximum(np.asarray(e1 - e0, dtype=np.float64), 1e-9), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def _np_ramp(x, e0, e1):
  """the bake-side ramp: plain smoothstep (no fwidth in a fixed-res bake — the
  mip chain owns minification filtering)."""
  return NumpyP.smoothstep(e0, e1, x)


def card_cache_path(fields_fn, res, params=None):
  """content-addressed TOKEN path for a card bake. Key folds: format epoch, the
  fields fn's SOURCE (comments included — any look edit re-keys), its params,
  and the resolution."""
  src = inspect.getsource(fields_fn)
  key = hashlib.sha256(("%s\x00%s\x00%r\x00r%d"
                        % (CARD_BAKE_EPOCH, src, sorted((params or {}).items()), int(res))
                        ).encode("utf-8")).hexdigest()[:16]
  return "<assetcache>/ptex3d_capture/card/%s__r%d.png" % (key, int(res))


def _dilate_rgb_preserve_alpha(rgba, iters):
  """Gutter dilation that pushes covered RGB outward into uncovered texels while
  PRESERVING the soft alpha channel (unlike the section-bake dilator, which
  stamps coverage into alpha — wrong for a silhouette card)."""
  rgb = rgba[..., :3].astype(np.float32)
  cov = (rgba[..., 3] > 8).astype(np.float32)          # covered = meaningfully inside
  offs = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]
  def shift(a, dy, dx):
    h, w = a.shape[:2]
    out = np.zeros_like(a)
    out[max(0, -dy):h + min(0, -dy), max(0, -dx):w + min(0, -dx)] = \
        a[max(0, dy):h + min(0, dy), max(0, dx):w + min(0, dx)]
    return out
  for _ in range(int(iters)):
    acc = np.zeros_like(rgb)
    n   = np.zeros_like(cov)
    for dy, dx in offs:
      c = shift(cov, dy, dx)
      acc += shift(rgb, dy, dx) * c[..., None]
      n   += c
    grow = (cov < 0.5) & (n > 0.0)
    rgb[grow] = (acc[grow] / n[grow, None])
    cov = np.where(grow, 1.0, cov)
  out = rgba.copy()
  out[..., :3] = np.clip(rgb, 0, 255).astype(np.uint8)
  return out


def bake_card(fields_fn, *, res=256, params=None):
  """Bake (or WARM-hit) the shared card texture for `fields_fn`. Returns the
  <assetcache>-token PNG path for sampler_textures binding. Deterministic:
  same fn source + params + res -> same file, byte-stable."""
  token = card_cache_path(fields_fn, res, params)
  path  = _Path.expandPathString(token)
  if os.path.isfile(path):
    return token                                        # WARM
  R = int(res)
  # texel-center UV grid; v runs petiole(0)->tip(1) upward in the image
  u = (np.arange(R, dtype=np.float64) + 0.5) / R
  v = (np.arange(R, dtype=np.float64) + 0.5) / R
  uu, vv = np.meshgrid(u, v)
  alpha, shade, transl, cavity = fields_fn(NumpyP, uu, vv, _np_ramp, **(params or {}))
  def _ch(x):
    return np.clip(np.broadcast_to(x, (R, R)).astype(np.float64), 0.0, 1.0)
  alpha = _ch(alpha)
  rgba = np.zeros((R, R, 4), dtype=np.uint8)
  rgba[..., 0] = np.round(_ch(shade)  * 255.0).astype(np.uint8)
  rgba[..., 1] = np.round(_ch(transl) * 255.0).astype(np.uint8)
  rgba[..., 2] = np.round(_ch(cavity) * 255.0).astype(np.uint8)
  rgba[..., 3] = np.round(alpha * 255.0).astype(np.uint8)
  rgba = _dilate_rgb_preserve_alpha(rgba, _CARD_DILATE_TEXELS)
  os.makedirs(os.path.dirname(path), exist_ok=True)
  from PIL import Image as _PILImage
  tmp = path + ".tmp-%d.png" % os.getpid()              # atomic publish (concurrent players)
  _PILImage.fromarray(rgba, "RGBA").save(tmp, format="PNG")
  os.replace(tmp, path)
  return token


__all__ = ["bake_card", "card_cache_path", "NumpyP", "CARD_BAKE_EPOCH"]
