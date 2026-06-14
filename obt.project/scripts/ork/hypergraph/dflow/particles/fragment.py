###############################################################################
# particles.fragment — the FreestyleParticleMaterial SHADER DSL.
#
# Author the particle FRAGMENT as Python expressions (the ptex3d SurfNode IR —
# same SSA/CSE emitter, same `P` op namespace) and materialize a complete
# .fxv2 providing the `tfreestyleparticle_{sprites,streaks}` technique pair,
# referenced through the material's reflected `shader_path` (so the override
# round-trips into the zero-Python player like any other authored state).
#
#   from ork.hypergraph.dflow.particles import FreestyleFragment, materialize_fragment
#   from ork.hypergraph.ptex3d import P
#
#   class FireFrag(FreestyleFragment):
#     def __init__(self, ctx):
#       cookie = ctx.cookie()                       # flipbook sample (vec4)
#       ramp   = ctx.ramp()                         # gradient over life (vec4)
#       lum    = P.length(cookie.xyz) * 0.5773      # cookie opacity
#       rgb    = ramp.xyz * cookie.xyz * ctx.color_factor * ctx.modcolor.xyz
#       a      = P.saturate(ramp.w * lum * ctx.alpha_factor * ctx.modcolor.w)
#       if ctx.is_streak:                           # TRACE-TIME divergence
#         a = a * P.smoothstep(1.0, 0.6, ctx.uv.y)  # fade streak tails
#       self.output(P.vec4(rgb, a))
#
#   mat = particles.FreestyleParticleMaterial.createShared()
#   mat.shader_path = materialize_fragment(FireFrag)
#
# The class is instantiated TWICE (ctx.is_streak False, then True) — Python
# control flow over `ctx.is_streak` IS the sprite/streak branch, unrolled at
# trace time into the two fragment shaders the technique pair binds. Both
# bodies share one libblock (helpers dedup by content).
#
# Fragment input atoms (the SSBO path's fface_psys_grid contract):
#   ctx.unit_age      float  particle life in [0,1) (clamped at 255/256)
#   ctx.random        float  stable per-particle hash in [0,1]
#   ctx.aux           vec2   per-particle aux channel (emitter Aux.x/.y)
#   ctx.uv            vec2   quad-local uv (sprites: billboard; streaks:
#                            x = across, y = head(0) -> tail(1))
#   ctx.modcolor / ctx.color_factor / ctx.alpha_factor — material uniforms
#   ctx.griddim       float  flipbook grid dimension
#   ctx.cookie(t=, uv=)  vec4  flipbook cell sample (defaults: life, quad uv)
#   ctx.ramp(t=)         vec4  gradient ramp sample (default: life)
#   ctx.is_streak     bool   TRACE-TIME geometry kind
#
# The PREMA contract: output rgb ADDS, output a OCCLUDES — author both.
# v1 limits (deliberate): no ctx.param (the material's reflected knobs —
# color/factors/gradient/texture — are the bindable surface; a generic
# extra-uniform block needs material-side binding support first) and no
# extra ctx.tex samplers (ColorMap = the cookie, GradientMap = the ramp).
# P.noise/P.fbm/P.voronoi work (helpers + misctools import flow through).
###############################################################################

import hashlib

from ork.hypergraph.ptex3d.dsl import (
    SurfNode, Const, CtxRef, Op, _wrap, _Emitter, P,
)

# Bump when the template/contract changes so cached files regenerate.
CODEGEN_VERSION = "hyperptc-frag-2"  # v2: heat fragments depth-test against the scene (RCFD_DEPTH_MAP)

_UNIT_AGE_MAX = "255.0/256.0"


###############################################################################
# ctx — the fragment input atoms
###############################################################################

class FragmentCtx:
  """Input atoms for a freestyle particle fragment. `is_streak` is a plain
  Python bool — branch on it freely; the class traces once per geometry kind.
  `is_heat` is the AUX-CHANNEL trace flag (E2B item D): querying it opts the
  shader into the heat technique pair — the class traces two MORE times with
  is_heat=True, and those bodies render additively into the forward node's
  "aux_heat" RT (write heat intensity in .r; the PREMA color contract does
  not apply there). Not querying it = no heat techniques = the material is
  skipped by aux passes entirely."""

  def __init__(self, is_streak, _is_heat=False):
    self.is_streak = bool(is_streak)
    self._is_heat = bool(_is_heat)
    self._heat_queried = False

  @property
  def is_heat(self):
    self._heat_queried = True
    return self._is_heat

  unit_age     = property(lambda self: CtxRef("unit_age",   "float"))
  random       = property(lambda self: CtxRef("frg_uv1.y",  "float"))
  aux          = property(lambda self: CtxRef("frg_uv1.zw", "vec2"))
  uv           = property(lambda self: CtxRef("frg_uv0",    "vec2"))
  modcolor     = property(lambda self: CtxRef("modcolor",   "vec4"))
  color_factor = property(lambda self: CtxRef("ColorFactor", "float"))
  alpha_factor = property(lambda self: CtxRef("AlphaFactor", "float"))
  griddim      = property(lambda self: CtxRef("GridDim",    "float"))

  def cookie(self, t=None, uv=None):
    """Flipbook cookie sample: cell selected by `t` (default: unit_age),
    sampled at quad-local `uv` (default: ctx.uv). GridDim=1 = static cookie."""
    t  = _wrap(t)  if t  is not None else self.unit_age
    uv = _wrap(uv) if uv is not None else self.uv
    return Op("_ptcfrag_cookie({0}, {1})", [t, uv], "vec4", libsrc=_COOKIE_SRC)

  def ramp(self, t=None):
    """Gradient-ramp sample at `t` (default: unit_age) — the material's
    reflected gradient, baked to the 256x1 GradientMap each update."""
    t = _wrap(t) if t is not None else self.unit_age
    return Op("_ptcfrag_ramp({0})", [t], "vec4", libsrc=_RAMP_SRC)


_COOKIE_SRC = """
vec4 _ptcfrag_cookie(float cell_t, vec2 quad_uv) {   // flipbook cell select + sample
  float gd        = max(GridDim, 1.0);
  float fgridcell = float(int(clamp(cell_t, 0.0, %s) * (gd * gd - 1.0)));
  vec2 gridUV     = vec2(mod(fgridcell, gd) / gd, floor(fgridcell / gd) / gd);
  return texture(ColorMap, gridUV + quad_uv / gd);
}
""" % _UNIT_AGE_MAX

_RAMP_SRC = """
vec4 _ptcfrag_ramp(float t) { return texture(GradientMap, vec2(t, 0.0)); }
"""


###############################################################################
# the authoring base class
###############################################################################

class FreestyleFragment:
  """Subclass with __init__(self, ctx, **params); call self.output(vec4_expr)
  exactly once. The expression is the PREMA fragment color (rgb adds, a
  occludes). Branch on ctx.is_streak for sprite/streak divergence."""

  def output(self, expr):
    expr = _wrap(expr)
    if expr._type != "vec4":
      raise TypeError(
          "FreestyleFragment.output() takes a vec4 (PREMA: rgb adds, a "
          "occludes — author the alpha explicitly); got %s" % expr._type)
    self._out = expr


###############################################################################
# emission + template
###############################################################################

def _emit_body(node):
  """SurfNode -> (body_glsl, libsrcs, inherits, imports). The body assumes the
  `unit_age` prelude local and writes `out_clr`."""
  em = _Emitter()
  final = em.expr(node)
  if em.params:
    raise TypeError(
        "particle fragments have no ctx.param yet — use the material's "
        "reflected knobs (color/colorFactor/alphaFactor/gradient/texture); "
        "got param(s): %s" % ", ".join(em.params))
  if em.samplers:
    raise TypeError(
        "particle fragments have no ctx.tex yet — the cookie (ColorMap) and "
        "ramp (GradientMap) are the bindable samplers; got: %s"
        % ", ".join(em.samplers))
  lines = ["float unit_age = clamp(frg_uv1.x, 0.0, %s);" % _UNIT_AGE_MAX]
  lines += em.lines
  lines.append("out_clr = %s;" % final)
  return "\n".join("  " + l for l in lines), list(em.libsrcs), set(em.inherits), set(em.imports)


def generate_fragment_fxv2(dsl_class, **params):
  """Trace `dsl_class` for sprites AND streaks (plus the heat pair when the
  class queries ctx.is_heat); return the complete .fxv2 text."""
  bodies = {}
  libsrcs, inherits, imports = [], set(), set()
  heat_opted = False
  variants = [("sprites", False, False), ("streaks", True, False)]
  for kind, is_streak, is_heat in variants:
    ctx = FragmentCtx(is_streak, _is_heat=is_heat)
    inst = dsl_class(ctx, **params)
    out = getattr(inst, "_out", None)
    if out is None:
      raise RuntimeError("%s built no output() (is_streak=%s is_heat=%s)"
                         % (dsl_class.__name__, is_streak, is_heat))
    body, lsrc, inh, imp = _emit_body(out)
    bodies[kind] = body
    for s in lsrc:
      if s not in libsrcs:
        libsrcs.append(s)
    inherits |= inh
    imports |= imp
    # ctx.is_heat queried during a base trace -> the class authors a heat
    # variant; trace the heat pair too (appended once, after the base pair)
    if not is_heat and ctx._heat_queried and not heat_opted:
      heat_opted = True
      variants += [("sprites_heat", False, True), ("streaks_heat", True, True)]
  if "lib_mmnoise" in inherits:
    imports.add("orkshader://misctools.i2")

  import_lines = "\n".join('  import "%s";' % i for i in
                           ["orkshader://particle_common.i2",
                            "orkshader://particle_ssbo.i2"] + sorted(imports))
  lib_inherit = "".join(" : %s" % i for i in sorted(inherits))
  helpers = "\n".join(s.strip() for s in libsrcs)
  lib_block = ""
  lib_ref = ""
  if helpers:
    lib_block = (
        "///////////////////////////////////////////////////////////////\n"
        "libblock lib_ptcfrag : ublk_frg : uset_frg_grid : sset_frg%s {\n%s\n}\n"
        % (lib_inherit, helpers))
    lib_ref = " : lib_ptcfrag"

  heat_block = ""
  if heat_opted:
    # the aux RTG carries NO depth attachment — occlusion is a MANUAL
    # depth test against the scene depth (RCFD_DEPTH_MAP, the read-only
    # prepass depth the color pass already samples for water etc.):
    # fragments behind terrain/solids discard, so heat doesn't shimmer
    # through hills. (Caveat: sampler2D — MSAA>1 primary unsupported.)
    occl = ("  vec2 _suv = gl_FragCoord.xy / vec2(textureSize(DepthMap, 0));\n"
            "  if (gl_FragCoord.z > texture(DepthMap, _suv).r)\n"
            "    discard;\n")
    heat_block = """///////////////////////////////////////////////////////////////
// AUX "heat" channel variants (ctx.is_heat traces) — additive into the
// forward node's aux_heat RT; .r = heat intensity. Scene-depth occluded
// (manual test — the aux RTG has no depth attachment).
sampler_set sset_ptcfrag_depth (descriptor_set 0) {
  sampler2D DepthMap;
}
///////////////////////////////////////////////////////////////
fragment_shader ps_dsl_sprites_heat : fface_psys_grid : sset_ptcfrag_depth%s {
%s
}
///////////////////////////////////////////////////////////////
fragment_shader ps_dsl_streaks_heat : fface_psys_grid : sset_ptcfrag_depth%s {
%s
}
///////////////////////////////////////////////////////////////
technique tfreestyleparticle_sprites_heat {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_sprite_ssbo;
    fragment_shader = ps_dsl_sprites_heat;
    state_block     = sb_default;
  }
}
technique tfreestyleparticle_streaks_heat {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streak_ssbo;
    fragment_shader = ps_dsl_streaks_heat;
    state_block     = sb_default;
  }
}
""" % (lib_ref, occl + bodies["sprites_heat"],
       lib_ref, occl + bodies["streaks_heat"])

  return """///////////////////////////////////////////////////////////////
// GENERATED by ork.hypergraph.dflow.particles.fragment — DO NOT EDIT.
// Freestyle particle fragment authored by %s; loaded through
// FreestyleParticleMaterial.shader_path (reflected -> round-trips).
///////////////////////////////////////////////////////////////
fxconfig fxcfg_default {
  glsl_version = "330";
%s
}
%s///////////////////////////////////////////////////////////////
fragment_shader ps_dsl_sprites : fface_psys_grid%s {
%s
}
///////////////////////////////////////////////////////////////
fragment_shader ps_dsl_streaks : fface_psys_grid%s {
%s
}
///////////////////////////////////////////////////////////////
technique tfreestyleparticle_sprites {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_sprite_ssbo;
    fragment_shader = ps_dsl_sprites;
    state_block     = sb_default;
  }
}
technique tfreestyleparticle_streaks {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streak_ssbo;
    fragment_shader = ps_dsl_streaks;
    state_block     = sb_default;
  }
}
%s""" % (dsl_class.__name__, import_lines, lib_block,
       lib_ref, bodies["sprites"], lib_ref, bodies["streaks"], heat_block)


###############################################################################
# persist — <staging>/dslshadercache/particles (owner policy: DSL-generated
# shaders live in the per-family staging cache, never the source tree; the
# returned reference is the relocatable "<staging>/..." token, expanded by
# C++ and Python alike at load — the file regenerates at authoring time).
###############################################################################

def materialize_fragment(dsl_class, *, name_hint=None, **params):
  """Trace + emit + persist; returns the shader_path reference string
  ("<staging>/dslshadercache/particles/<hint>_<hash>.fxv2"). Content-
  addressed: identical generated text reuses the same file."""
  from ork.hypergraph.ptex3d.fxv2_template import dslcache_write
  text = generate_fragment_fxv2(dsl_class, **params)
  digest = hashlib.sha1((CODEGEN_VERSION + "\n" + text).encode("utf-8")).hexdigest()[:16]
  fname = "%s_%s.fxv2" % (name_hint or dsl_class.__name__.lower(), digest)
  return dslcache_write("particles", fname, text)


__all__ = ["FreestyleFragment", "FragmentCtx", "materialize_fragment",
           "generate_fragment_fxv2", "CODEGEN_VERSION"]
