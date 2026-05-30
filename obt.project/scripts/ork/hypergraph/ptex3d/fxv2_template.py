###############################################################################
# GEOV2 Phase 1 — .fxv2 template + variant-table generator.
#
# generate_surface_fxv2(surface_body, ...) -> complete .fxv2 text.
# materialize_surface_fxv2(...)            -> hash-named file under the cache.
#
# The generated shader is a self-contained custom PBR surface (variant-emission
# "path A", the bring-up form — the target is path B injecting into PBR2's
# SurfaceFragment, see ~/GEOV2.md §16-A). It reuses the STOCK forward-PBR machine
# entirely via orkshader:// imports; the only per-material content is:
#   - an optional helper `libblock` (e.g. fbm/worley helpers — Phase 2 emits this)
#   - the `surface_body` GLSL that writes the SurfaceOut fields.
#
# The surface body runs with these locals in scope (the GEOV2 inputs):
#   vec3 wpos;   // world position          (frg_wpos.xyz)
#   vec3 opos;   // OBJECT-space position   (frg_opos)  — stable on the mesh
#   vec2 uv;     // free-range uv           (frg_uv0)
#   vec4 cd;     // Cd — 4D per-vertex SELECTOR (frg_clr), NOT a color
#   mat3 tbn;    // world tangent/bitangent/normal basis (frg_tbn)
#   vec3 wnrm;   // world geometric normal  (frg_tbn[2])
#   vec3 onrm;   // OBJECT-space normal     (frg_onrm) — for surface-aware proctex
#   vec3 eye;    // camera world position   (EyePostion)
# and writes any of (defaults pre-set):
#   o.albedo (vec3=0.8) o.metallic (0) o.roughness (0.5)
#   o.normal (vec3=wnrm) o.emissive (vec3=0) o.ao (1)   // AO inert until PBR2
###############################################################################

import os
import hashlib

from orkengine.core import Path as _Path

# Bump when the template/contract changes so cached files regenerate.
CODEGEN_VERSION = "geov2-ptex-fxv2-6"   # bumped: displacement height + analytic bump (Phase 4)

# The surface output contract (mirrors the eventual PBR2 SurfaceFragment subset).
SURFACE_OUT_FIELDS = ("albedo", "metallic", "roughness", "normal", "emissive", "ao")

# Always-present stock imports (the forward-PBR machine + common noise/util).
_STD_IMPORTS = (
  "orkshader://mathtools.i2",
  "orkshader://misctools.i2",
  "orkshader://envtools.i2",
  "orkshader://pbrtools.i2",
  "orkshader://stdtools.i2",
  "orkshader://ssaotools.i2",
)

_cache_dir = None


def _dslcache_dir():
  global _cache_dir
  if _cache_dir is None:
    d = _Path.expandPathString("<staging>/dslshadercache")
    if "<" in d:  # path expanders not registered (headless / no app init) → $OBT_STAGE
      stage = os.environ.get("OBT_STAGE")
      if stage:
        d = os.path.join(stage, "dslshadercache")
    os.makedirs(d, exist_ok=True)
    _cache_dir = d
  return _cache_dir


def _params_block(params):
  """params: [(name, gtype, default), ...] -> (block_decl, inherit_clause).
  Each bindable param is one vec4 member (vec4-padded → std140-safe, and a
  whole-vec4 write from any bindParam type can never clobber a neighbor)."""
  if not params:
    return "", ""
  members = "\n".join("  vec4 %s;" % name for (name, _gtype, _default) in params)
  block = (
    "///////////////////////////////////////////////////////////////\n"
    "// GEOV2 Phase 3 — bindable runtime uniforms (ctx.param). Each is\n"
    "// vec4-padded (std140-safe); read swizzled in ptex_surface(). Bind or\n"
    "// rebind from Python/C++ via material.bindParam(name, value).\n"
    "uniform_block ublk_ptex_params (descriptor_set 0) {\n%s\n}" % members)
  return block, " : ublk_ptex_params"


def _height_blocks(height_body, height_expr, displace_scale):
  """-> (height_function, bump_block). Empty when no displacement is declared.

  height_function: `float ptex_height(vec3 coord, vec3 onrm)` placed in the
    surface libblock — a re-evaluable scalar field (drives the bump now, the
    parallax march later).
  bump_block: GEOV2 §18 ANALYTIC BUMP — finite-difference ptex_height in OBJECT
    tangent space (otan/obin from onrm + frg_obinormal), then map the perturbed
    tangent-space normal to world via the world TBN. No screen derivatives, so no
    crease dots; object-anchored. Overrides `wnrm` before the surface eval."""
  if not (height_expr and str(height_expr).strip()):
    return "", ""
  s  = repr(float(displace_scale))   # always has a decimal point
  hb = _indent(height_body.strip(), 4) if height_body.strip() else ""
  height_function = (
    "  // GEOV2 Phase 4 — procedural displacement height (re-evaluable at any\n"
    "  // coordinate; drives the analytic bump, parallax-occlusion later).\n"
    "  float ptex_height(vec3 coord, vec3 onrm) {\n"
    "%s\n"
    "    return %s;\n"
    "  }") % (hb, height_expr)
  bump_block = (
    "  {  // analytic bump from ptex_height (object tangent space -> world TBN)\n"
    "    vec3  _obin = normalize(frg_obin);\n"
    "    vec3  _otan = cross(onrm, _obin);\n"
    "    float _e    = 0.0015;\n"
    "    float _du   = ptex_height(frg_opos + _e*_otan, onrm) - ptex_height(frg_opos - _e*_otan, onrm);\n"
    "    float _dv   = ptex_height(frg_opos + _e*_obin, onrm) - ptex_height(frg_opos - _e*_obin, onrm);\n"
    "    float _k    = %s / (2.0 * _e);\n"
    "    vec3  _ts   = normalize(vec3(-_du * _k, -_dv * _k, 1.0));\n"
    "    wnrm = normalize(frg_tbn * _ts);\n"
    "  }") % (s,)
  return height_function, bump_block


def generate_surface_fxv2(surface_body,
                          *,
                          libblock="",
                          lib_inherits=(),
                          extra_imports=(),
                          params=(),
                          height_body="",
                          height_expr="",
                          displace_scale=0.05):
  """Assemble a complete forward-PBR .fxv2 around a GLSL surface body.

  surface_body : GLSL statements assigning to o.<field> (see module docstring).
  libblock     : optional GLSL helper functions the body calls (e.g. fbm/worley).
  lib_inherits : libblock names the surface needs in scope (e.g. "lib_mmnoise",
                 "lib_sdftools"); inherited on the surface libblock.
  extra_imports: extra orkshader:// .i2 files the lib_inherits come from beyond
                 the standard set (e.g. "orkshader://sdftools.i2").
  params       : [(name, gtype, default), ...] bindable runtime uniforms — emits
                 a ublk_ptex_params block inherited by the surface libblock.
  """
  imports = list(_STD_IMPORTS) + [i for i in extra_imports if i not in _STD_IMPORTS]
  import_lines = "\n".join('  import "%s";' % i for i in imports)
  # the surface libblock inherits its needed noise/util libblocks
  surf_inherits = "".join(" : %s" % n for n in lib_inherits)
  out_struct = "  struct SurfaceOut { vec3 albedo; float metallic; float roughness; vec3 normal; vec3 emissive; float ao; };"
  params_block, params_inherit = _params_block(params)
  height_function, bump_block = _height_blocks(height_body, height_expr, displace_scale)

  return _TEMPLATE.format(
    import_lines=import_lines,
    surf_inherits=surf_inherits,
    out_struct=out_struct,
    params_block=("\n" + params_block + "\n" if params_block else ""),
    params_inherit=params_inherit,
    libblock=("\n" + libblock.strip() + "\n" if libblock.strip() else ""),
    height_function=("\n" + height_function + "\n" if height_function else ""),
    bump_block=("\n" + bump_block if bump_block else ""),
    surface_body=_indent(surface_body.strip(), 4),
  )


def materialize_surface_fxv2(surface_body,
                             *,
                             libblock="",
                             lib_inherits=(),
                             extra_imports=(),
                             params=(),
                             height_body="",
                             height_expr="",
                             displace_scale=0.05,
                             name_hint="ptex"):
  """Generate + write the .fxv2 to <staging>/dslshadercache/<hint>_<hash>.fxv2.

  Content-addressed: identical generated text reuses the same file (and the
  downstream SPIR-V DataBlockCache dedups compilation). Returns the abs path.
  """
  text = generate_surface_fxv2(surface_body,
                               libblock=libblock,
                               lib_inherits=lib_inherits,
                               extra_imports=extra_imports,
                               params=params,
                               height_body=height_body,
                               height_expr=height_expr,
                               displace_scale=displace_scale)
  digest = hashlib.sha1((CODEGEN_VERSION + "\n" + text).encode("utf-8")).hexdigest()[:16]
  path = os.path.join(_dslcache_dir(), "%s_%s.fxv2" % (name_hint, digest))
  if not os.path.exists(path):
    with open(path, "w") as f:
      f.write(text)
  return path


def _indent(text, n):
  pad = " " * n
  return "\n".join((pad + line if line else line) for line in text.split("\n"))


###############################################################################
# The template. {surface_body} fills the "hole" inside ptex_surface(); the rest
# is fixed boilerplate proven in Phase 0 (geov2_phase0_marble.fxv2): a rigid
# vertex-color-passthrough interface carrying Cd + uv + object-space position,
# the dual-MRT forward fragment calling _forward_lightingZ, the depth-prepass,
# and the CV/CT/dpp technique table.
###############################################################################

_TEMPLATE = '''\
///////////////////////////////////////////////////////////////
// GENERATED by ork.hypergraph.ptex3d (GEOV2 Phase 1). Do not hand-edit.
///////////////////////////////////////////////////////////////
fxconfig fxcfg_default {{
{import_lines}
}}
///////////////////////////////////////////////////////////////
state_block sb_ptex : default {{
  CullTest  = PASS_FRONT;
  DepthTest = LEQUALS;
  BlendMode = OFF;
}}
///////////////////////////////////////////////////////////////
vertex_interface vif_ptex : ub_std_vtx {{
  inputs {{
    vec4 position : POSITION;
    vec3 normal : NORMAL;
    vec3 binormal : BINORMAL;
    vec2 uv0 : TEXCOORD0;
    vec4 vtxcolor : COLOR0;
  }}
  outputs {{
    vec4 frg_wpos;
    vec4 frg_clr;
    vec2 frg_uv0;
    mat3 frg_tbn;
    float frg_camdist;
    vec3 frg_camz;
    vec3 frg_opos;
    vec3 frg_onrm;
    vec3 frg_obin;
  }}
}}
///////////////////////////////////////////////////////////////
fragment_interface fif_ptex
  : vif_ptex
  : ub_frg_fwd
  : storage_fwd_lighting
  : ss_frg_fwd {{
  outputs {{
    layout(location = 0) vec4 out_clr;
    layout(location = 1) vec4 out_diffuse;
  }}
}}
///////////////////////////////////////////////////////////////
vertex_shader vs_ptex
  : vif_ptex
  : lib_pbr_vtx
  : ublk_std_matrices {{
  gl_Position = mvp * position;
  frg_uv0     = uv0;
  frg_wpos    = m * position;
  frg_opos    = position.xyz;
  frg_onrm    = normalize(normal);          // object-space normal (surface-aware proctex)
  frg_obin    = normalize(binormal);        // object-space binormal (analytic-bump tangent)
  frg_clr     = vtxcolor;
  vec3 wn = normalize(mat3(m) * normal);
  vec3 wb = normalize(mat3(m) * binormal);
  vec3 wt = cross(wn, wb);
  frg_tbn = mat3(wt, wb, wn);
  frg_camdist = 0.0;
  frg_camz    = vec3(0, 0, 0);
}}
///////////////////////////////////////////////////////////////
typeblock types_ptex {{
{out_struct}
}}{params_block}
///////////////////////////////////////////////////////////////
libblock lib_ptex_surface : types_ptex{params_inherit}{surf_inherits} {{
{libblock}{height_function}
  SurfaceOut ptex_surface(vec3 wpos, vec3 opos, vec2 uv, vec4 cd, mat3 tbn, vec3 wnrm, vec3 onrm, vec3 eye) {{
    SurfaceOut o;
    o.albedo   = vec3(0.8);
    o.metallic = 0.0;
    o.roughness= 0.5;
    o.normal   = wnrm;
    o.emissive = vec3(0.0);
    o.ao       = 1.0;
{surface_body}
    return o;
  }}
}}
///////////////////////////////////////////////////////////////
fragment_shader ps_ptex_forward
  : fif_ptex
  : lib_math
  : lib_brdf
  : lib_def
  : lib_fwd
  : lib_ptex_surface
  : std_forward_all {{
  vec3 wnrm = normalize(frg_tbn[2]);
  vec3 onrm = normalize(frg_onrm);
{bump_block}
  SurfaceOut s = ptex_surface(frg_wpos.xyz, frg_opos, frg_uv0, frg_clr, frg_tbn, wnrm, onrm, EyePostion);
  // TEMP roughness linearization (stopgap — REMOVE when the PBR importance-
  // sampling code is calibrated). Maps authored roughness into the
  // perceptually-visible band [0.3, 1.0] (floor 0.3 keeps low cells shiny).
  // Tunable: the affine (scale/bias) sets the band, the pow exponent the curve.
  float _rough = pow(s.roughness * 0.7 + 0.3, 1.0);
  vec3 ambrufmtl = vec3(s.ao, _rough, s.metallic);   // ambrufmtl.x = AO (inert until PBR2)
  ShadingResult sr = _forward_lightingZ(
    ModColor.xyz, s.albedo, ambrufmtl, s.emissive, EyePostion, s.normal, false);
  out_clr     = vec4(sr.specular + sr.diffuse, 1.0);
  out_diffuse = vec4(sr.diffuse, 0.0);
}}
///////////////////////////////////////////////////////////////
// Depth prepass (geometry-only)
///////////////////////////////////////////////////////////////
vertex_interface vif_ptex_dpp {{
  inputs {{ vec4 position : POSITION; }}
  outputs {{ float frg_camz; }}
}}
fragment_interface fif_ptex_dpp : vif_ptex_dpp {{
  outputs {{ layout(location = 0) float out_z; }}
}}
vertex_shader vs_ptex_dpp : vif_ptex_dpp : ublk_std_matrices {{
  vec4 hpos = mvp * position;
  gl_Position = hpos;
  frg_camz = hpos.z / hpos.w;
}}
fragment_shader ps_ptex_dpp : fif_ptex_dpp {{
  out_z = frg_camz;
}}
///////////////////////////////////////////////////////////////
technique FWD_CV_NM_RI_NI_MO {{
  fxconfig = fxcfg_default;
  vf_pass = {{ vs_ptex, ps_ptex_forward, sb_ptex }}
}}
technique FWD_CT_NM_RI_NI_MO {{
  fxconfig = fxcfg_default;
  vf_pass = {{ vs_ptex, ps_ptex_forward, sb_ptex }}
}}
technique FWD_DEPTHPREPASS_RI_NI_MO {{
  fxconfig = fxcfg_default;
  vf_pass = {{ vs_ptex_dpp, ps_ptex_dpp, sb_ptex }}
}}
'''
