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
#   vec3 vnrm;   // VIEW-space normal       — OPT-IN: present only if the body uses `vnrm`
#   vec3 eye;    // camera world position   (EyePostion)
# and writes any of (defaults pre-set):
#   o.albedo (vec3=0.8) o.metallic (0) o.roughness (0.5)
#   o.normal (vec3=wnrm) o.emissive (vec3=0) o.ao (1)   // AO live: attenuates ambient/diffuse IBL
###############################################################################

import os
import hashlib

from orkengine.core import Path as _Path

# Bump when the template/contract changes so cached files regenerate.
CODEGEN_VERSION = "geov2-ptex-fxv2-19"  # bumped: E.4 instanced SSBO depth-prepass variant

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


def _dslcache_dir(family=""):
  """<staging>/dslshadercache[/<family>] — THE home for ALL DSL-generated
  shaders (owner policy, 2026-06-12: machine-local staging cache namespaced
  per family; generated artifacts NEVER persist in the source tree — the
  serialized REFERENCE is the relocatable "<staging>/..." token form and
  the file regenerates at authoring time)."""
  global _cache_dir
  if _cache_dir is None:
    d = _Path.expandPathString("<staging>/dslshadercache")
    if "<" in d:  # path expanders not registered (headless / no app init) → $OBT_STAGE
      stage = os.environ.get("OBT_STAGE")
      if stage:
        d = os.path.join(stage, "dslshadercache")
    _cache_dir = d
  d = os.path.join(_cache_dir, family) if family else _cache_dir
  os.makedirs(d, exist_ok=True)
  return d


def dslcache_write(family, fname, text):
  """Write `text` content-addressed into the family cache (skip if present);
  return the RELOCATABLE "<staging>/dslshadercache/<family>/<fname>" token
  reference (expands C++ and Python alike at load)."""
  path = os.path.join(_dslcache_dir(family), fname)
  if not os.path.exists(path):
    with open(path, "w") as fp:
      fp.write(text)
  return "<staging>/dslshadercache/%s/%s" % (family, fname) if family \
      else "<staging>/dslshadercache/%s" % fname


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


def _samplers_block(samplers):
  """samplers: [name, ...] -> (block_decl, inherit_clause). Each ctx.tex(name) is a REAL
  sampler2D uniform (a bindable — attach a Texture at runtime via material.bindParam(name,
  texture)), declared in a sampler_set inherited by the surface libblock — mirroring the
  ctx.param uniform_block path (proven to bind through libblock -> fragment -> pipeline)."""
  if not samplers:
    return "", ""
  members = "\n".join("  sampler2D %s;" % name for name in samplers)
  block = (
    "///////////////////////////////////////////////////////////////\n"
    "// ctx.tex() bindable 2D textures (NOT baked); attach via material.bindParam(name, texture).\n"
    "sampler_set sset_ptex_tex (descriptor_set 0) {\n%s\n}" % members)
  return block, " : sset_ptex_tex"


def _height_blocks(height_body, height_expr, displace_scale):
  """-> (height_function, bump_block). Empty when no displacement is declared.

  height_function: `float ptex_height(vec3 coord, vec3 wpos, vec3 wnrm, vec3 onrm,
    vec2 uv, vec4 cd, vec3 eye)` in the surface libblock — a re-evaluable scalar field.
    `coord` is the only VARYING atom (= opos, offset for the finite-diff / marched for
    parallax); wpos/wnrm/uv/cd/eye are the fragment's values passed in CONSTANT, so any
    surface expression (incl. masks via ctx.P / ctx.N / ctx.uv and ctx.tex samplers, which
    are globals) is valid in displace — exactly the atoms ptex_surface gets.
  bump_block: GEOV2 §18 ANALYTIC BUMP — finite-difference ptex_height in OBJECT
    tangent space (otan/obin from onrm + frg_obinormal), then map the perturbed
    tangent-space normal to world via the world TBN. No screen derivatives, so no
    crease dots; object-anchored. Overrides `wnrm` before the surface eval."""
  if not (height_expr and str(height_expr).strip()):
    return "", ""
  # scale may be a GLSL expr (bindable param) or a plain number (baked)
  s  = displace_scale if isinstance(displace_scale, str) else repr(float(displace_scale))
  hb = _indent(height_body.strip(), 4) if height_body.strip() else ""
  height_function = (
    "  // GEOV2 Phase 4 — procedural displacement height (re-evaluable at any\n"
    "  // coordinate; drives the analytic bump, parallax-occlusion later).\n"
    "  // coord is the only VARYING atom (offset for the finite-diff / marched for\n"
    "  // parallax). wpos/wnrm/uv/cd/eye come in CONSTANT (the fragment's values), so\n"
    "  // mask expressions (ctx.P / ctx.N / ctx.uv / ctx.tex) are valid here too.\n"
    "  float ptex_height(vec3 coord, vec3 wpos, vec3 wnrm, vec3 onrm, vec2 uv, vec4 cd, vec3 eye) {\n"
    "%s\n"
    "    return %s;\n"
    "  }") % (hb, height_expr)
  bump_block = (
    "  {  // analytic bump from ptex_height (object tangent space -> world TBN).\n"
    "     // Forward difference: 3 height taps (h0 reused), not 4 — the height\n"
    "     // field (warp+voronoi) is the cost, so each tap saved matters.\n"
    "    vec3  _obin = normalize(frg_obin);\n"
    "    vec3  _otan = cross(onrm, _obin);\n"
    "    float _e    = 0.0015;\n"
    "    float _h0   = ptex_height(opos,            frg_wpos.xyz, wnrm, onrm, frg_uv0, frg_clr, EyePostion);\n"
    "    float _du   = ptex_height(opos + _e*_otan, frg_wpos.xyz, wnrm, onrm, frg_uv0, frg_clr, EyePostion) - _h0;\n"
    "    float _dv   = ptex_height(opos + _e*_obin, frg_wpos.xyz, wnrm, onrm, frg_uv0, frg_clr, EyePostion) - _h0;\n"
    "    float _k    = %s / _e;\n"
    "    vec3  _ts   = normalize(vec3(-_du * _k, -_dv * _k, 1.0));\n"
    "    wnrm = normalize(frg_tbn * _ts);\n"
    "  }") % (s,)
  return height_function, bump_block


def _cellular_bump_block(coord_body, coord_expr, width_expr, scale_expr):
  """GEOV2 §18 option 1 — ANALYTIC cellular bump. One gradient-voronoi eval
  (`_ptex_voronoi_g` returns fwedge + the cell-wall normal = the height gradient
  direction); the smoothstep slope scales it; project to object tangent space and
  map to world via the TBN. No finite differences, no per-tap warp re-eval.
  width_expr / scale_expr are GLSL (may be bindable-param uniforms)."""
  if not (coord_expr and str(coord_expr).strip()):
    return ""
  cb = _indent(coord_body.strip(), 4) if coord_body.strip() else ""
  return (
    "  {  // ANALYTIC cellular bump — one gradient-voronoi eval (no finite diffs)\n"
    "    vec3  _obin = normalize(frg_obin);\n"
    "    vec3  _otan = cross(onrm, _obin);\n"
    "%s\n"
    "    vec4  _vg = _ptex_voronoi_g(%s, onrm);\n"
    "    float _W  = max(%s, 1e-5);\n"
    "    float _bs = %s;\n"
    "    float _u  = clamp(_vg.x / _W, 0.0, 1.0);\n"
    "    float _sl = 6.0 * _u * (1.0 - _u) / _W;          // d/dx smoothstep(0,_W,fwedge)\n"
    "    vec3  _grad = _sl * _vg.yzw;                      // grad(height), object space\n"
    "    vec3  _ts = normalize(vec3(-dot(_grad,_otan) * _bs, -dot(_grad,_obin) * _bs, 1.0));\n"
    "    wnrm = normalize(frg_tbn * _ts);\n"
    "  }") % (cb, coord_expr, width_expr, scale_expr)


def _parallax_block(steps, depth_expr):
  """GEOV2 §18 — PROCEDURAL parallax occlusion. Marches ptex_height along the
  tangent-space view ray (object space), updating `opos` to the displaced hit
  point; the bump + surface then evaluate at `opos`, so the relief shows real
  parallax + self-occlusion. TEMPORARY/EXPENSIVE: re-runs the height field
  `steps`× per fragment — replace with a baked-height texture sample once auto-uv
  lands (GEOV2 §18.5). depth_expr (GLSL, may be a bindable param) is the relief
  depth in object units; requires the general displace() path (ptex_height)."""
  if not (steps and depth_expr and str(depth_expr).strip()):
    return ""
  n = int(steps)
  return (
    "  {  // ===== PROCEDURAL PARALLAX OCCLUSION (TEMPORARY/EXPENSIVE) =====\n"
    "     // marches ptex_height %d steps/fragment; swap for a baked height\n"
    "     // texture once auto-uv lands (GEOV2 §18.5).\n"
    "    vec3  _obin = normalize(frg_obin);\n"
    "    vec3  _otan = cross(onrm, _obin);\n"
    "    vec3  _V    = normalize(EyePostion - frg_wpos.xyz);\n"
    "    float _vN   = max(dot(_V, frg_tbn[2]), 0.05);\n"
    "    vec2  _Pt   = vec2(dot(_V, frg_tbn[0]), dot(_V, frg_tbn[1])) / _vN * (%s);\n"
    "    float _dL   = 1.0 / float(%d);\n"
    "    vec2  _dO   = _Pt / float(%d);\n"
    "    vec2  _o = vec2(0.0); float _L = 0.0;\n"
    "    float _d = 1.0 - ptex_height(opos, frg_wpos.xyz, wnrm, onrm, frg_uv0, frg_clr, EyePostion);\n"
    "    vec2  _po = _o; float _pL = _L;\n"
    "    for (int _i = 0; _i < %d; _i++) {       // linear search: bracket the hit\n"
    "      if (_L >= _d) break;\n"
    "      _po = _o; _pL = _L;\n"
    "      _o -= _dO; _L += _dL;\n"
    "      _d  = 1.0 - ptex_height(opos + _o.x*_otan + _o.y*_obin, frg_wpos.xyz, wnrm, onrm, frg_uv0, frg_clr, EyePostion);\n"
    "    }\n"
    "    for (int _b = 0; _b < 6; _b++) {        // binary search: refine (kills jitter)\n"
    "      vec2  _m  = 0.5 * (_po + _o);\n"
    "      float _mL = 0.5 * (_pL + _L);\n"
    "      float _md = 1.0 - ptex_height(opos + _m.x*_otan + _m.y*_obin, frg_wpos.xyz, wnrm, onrm, frg_uv0, frg_clr, EyePostion);\n"
    "      if (_mL < _md) { _po = _m; _pL = _mL; } else { _o = _m; _L = _mL; }\n"
    "    }\n"
    "    opos = opos + _o.x*_otan + _o.y*_obin;\n"
    "  }") % (n, depth_expr, n, n, n)


# The vertex->fragment varying contract (ONE definition), shared by the stock rigid
# vertex_interface (vif_ptex) AND the SSBO-pull variant (vif_ptex_ssbo) so the same
# fragment (ps_ptex_forward) links against either VS. {vif_vnrm} is the opt-in view-normal.
_VTX_OUTPUTS = (
  "    vec4 frg_wpos;\n"
  "    vec4 frg_clr;\n"
  "    vec2 frg_uv0;\n"
  "    mat3 frg_tbn;\n"
  "    float frg_camdist;\n"
  "    vec3 frg_camz;\n"
  "    vec3 frg_opos;\n"
  "    vec3 frg_onrm;\n"
  "    vec3 frg_obin;\n"
  "{vif_vnrm}")

# The VS body AFTER the per-vertex locals (position/normal/binormal/uv0/vtxcolor) exist —
# identical for the attribute-sourced (vs_ptex) and SSBO-sourced (vs_ptex_ssbo) variants.
# {vs_vnrm} is the opt-in object->view normal transform.
_VS_TAIL = (
  "  gl_Position = mvp * position;\n"
  "  frg_uv0     = uv0;\n"
  "  frg_wpos    = m * position;\n"
  "  frg_opos    = position.xyz;\n"
  "  frg_onrm    = normalize(normal);          // object-space normal (surface-aware proctex)\n"
  "  frg_obin    = normalize(binormal);        // object-space binormal (analytic-bump tangent)\n"
  "  frg_clr     = vtxcolor;\n"
  "{vs_vnrm}  vec3 wn = normalize(mat3(m) * normal);\n"
  "  vec3 wb = normalize(mat3(m) * binormal);\n"
  "  vec3 wt = cross(wn, wb);\n"
  "  frg_tbn = mat3(wt, wb, wn);\n"
  "  frg_camdist = 0.0;\n"
  "  frg_camz    = vec3(0, 0, 0);")


def _ssbo_block(ssbo_layout, ssbo_lib, ssbo_vs_body, ssbo_compute, ssbo_vs_inherits, vtx_outputs, vs_tail,
                ssbo_extra_blocks="", ssbo_vs_post="", ssbo_instanced=False):
  """FWD_SSBO_CUSTOM — an SSBO-sourced VERTEX variant paired with the SHARED forward
  fragment (ps_ptex_forward). Empty vertex input: the VS reads gl_VertexID and decodes
  from a DSL-supplied std430 layout (sif_ptex_vtx) into the locals position/normal/
  binormal/uv0/vtxcolor, then runs the shared VS tail. The SSBO is filled by the consumer
  (compute cull/gen, CPU upload, ...) — fill-agnostic. Empty when no ssbo_vs_body is given.

  ssbo_lib     : optional `libblock lib_ptex_vtx` body (helpers the pull VS / compute call,
                 may reference the sif_ptex_vtx storage members) — inherited by the VS + compute.
  ssbo_compute : optional whole compute_interface + compute_shader decls that FILL the SSBO.
                 Emitting them HERE (same program as vs_ptex_ssbo, which uses sif_ptex_vtx) keeps
                 sif_ptex_vtx a MERGED resource @0 for the compute too — sidestepping the
                 compute-only fallback-binding bug. They should inherit sif_ptex_vtx (+ lib_ptex_vtx)."""
  if not (ssbo_vs_body and str(ssbo_vs_body).strip()):
    return ""
  inh = "".join(" : %s" % n for n in ssbo_vs_inherits)
  has_lib = bool(ssbo_lib and str(ssbo_lib).strip())
  if has_lib:
    inh = " : lib_ptex_vtx" + inh
  layout = _indent(ssbo_layout.strip(), 4) if ssbo_layout.strip() else "    uint _pad;"
  body = _indent(ssbo_vs_body.strip(), 2)
  # optional VS snippet spliced AFTER the shared tail (i.e. after gl_Position is computed) — e.g. a
  # clip-space depth bias `gl_Position.z -= b*gl_Position.w`. Default empty -> output byte-identical.
  post = ("\n" + _indent(ssbo_vs_post.strip(), 2)) if (ssbo_vs_post and str(ssbo_vs_post).strip()) else ""
  lib_block = (
    "libblock lib_ptex_vtx {\n%s\n}\n" % _indent(ssbo_lib.strip(), 2)) if has_lib else ""
  # extra vertex storage blocks (e.g. SoA channel SSBOs sif_N/sif_B/...). They are inherited by the
  # VS via ssbo_vs_inherits, so the pull VS can read MULTIPLE bound SSBOs by gl_VertexID (an indirect
  # draw only supplies the count — it's agnostic to how many SSBOs the VS sources from).
  extra_blocks = (ssbo_extra_blocks.strip() + "\n") if (ssbo_extra_blocks and ssbo_extra_blocks.strip()) else ""
  compute_block = (
    "///////////////////////////////////////////////////////////////\n"
    "// compute that FILLS sif_ptex_vtx (shares the merged @0 binding with vs_ptex_ssbo).\n"
    "%s\n" % ssbo_compute.strip()) if (ssbo_compute and str(ssbo_compute).strip()) else ""
  # DEPTH-PREPASS variant: same SSBO-pull body (-> identical `position` -> matching depth, no
  # z-fight with the color pass), position-only, paired with the SHARED depth fragment ps_ptex_dpp.
  # Gives early-z AND puts the terrain into the depth buffer (prerequisite for occlusion culling).
  dpp_block = (
    "///////////////////////////////////////////////////////////////\n"
    "// FWD_SSBO_CUSTOM_DEPTHPREPASS — SSBO-pull depth-only variant (early-z; depth occluder).\n"
    "vertex_interface vif_ptex_ssbo_dpp : sif_ptex_vtx {\n"
    "  outputs { float frg_camz; }\n"
    "}\n"
    "vertex_shader vs_ptex_ssbo_dpp\n"
    "  : vif_ptex_ssbo_dpp\n"
    "  : ublk_std_matrices%s {\n"
    "%s\n"
    "  vec4 _hpos  = mvp * position;\n"
    "  gl_Position = _hpos;\n"
    "  frg_camz    = _hpos.z / _hpos.w;\n"
    "}\n"
    "technique FWD_SSBO_CUSTOM_DEPTHPREPASS {\n"
    "  fxconfig = fxcfg_default;\n"
    "  vf_pass = { vs_ptex_ssbo_dpp, ps_ptex_dpp, sb_ptex }\n"
    "}\n") % (inh, body)
  # FWD_SSBO_CUSTOM_INSTANCED — same SSBO-pull geometry, but each vertex is additionally placed by a
  # PER-INSTANCE matrix (storage_inst_mtx, indexed by gl_InstanceIndex). xformPoint/xformVec read only the
  # 4x3 part of the matrix (rows 0-2), so the matrix's BOTTOM ROW (m[0..2].w) carries 3 free floats of
  # per-instance data -> frg_clr (the Cd selector the surface reads). Reuses vif_ptex_ssbo + ps_ptex_forward.
  inst_block = ((
    "///////////////////////////////////////////////////////////////\n"
    "// FWD_SSBO_CUSTOM_INSTANCED — SSBO geometry (gl_VertexID) x per-instance matrix (gl_InstanceIndex).\n"
    "storage_interface storage_inst_mtx (descriptor_set 0) {\n"
    "  buffer layout(std430) inst_mtx_blk { mat4 _instance_matrices[]; };\n"
    "}\n"
    "storage_interface storage_inst_attr (descriptor_set 0) {\n"
    "  buffer layout(std430) inst_attr_blk { vec4 _instance_attrs[]; };\n"
    "}\n"
    "libblock lib_inst_xform {\n"
    "  vec3 xformPoint(mat4 m, vec3 p) {           // affine transform (rows 0-2); ignores row 3 (= data)\n"
    "    vec4 v = vec4(p, 1.0);\n"
    "    return vec3(dot(vec4(m[0].x, m[1].x, m[2].x, m[3].x), v),\n"
    "                dot(vec4(m[0].y, m[1].y, m[2].y, m[3].y), v),\n"
    "                dot(vec4(m[0].z, m[1].z, m[2].z, m[3].z), v));\n"
    "  }\n"
    "  vec3 xformVec(mat4 m, vec3 v) {              // rotation/scale only (no translation, ignores row 3)\n"
    "    return vec3(dot(vec3(m[0].x, m[1].x, m[2].x), v),\n"
    "                dot(vec3(m[0].y, m[1].y, m[2].y), v),\n"
    "                dot(vec3(m[0].z, m[1].z, m[2].z), v));\n"
    "  }\n"
    "}\n"
    "vertex_shader vs_ptex_ssbo_inst\n"
    "  : vif_ptex_ssbo\n"
    "  : lib_pbr_vtx\n"
    "  : lib_inst_xform\n"
    "  : storage_inst_mtx\n"
    "  : storage_inst_attr\n"
    "  : ublk_std_matrices%s {\n"
    "  // pull body -> position/normal/binormal/uv0/vtxcolor, THEN place by the per-instance matrix\n"
    "%s\n"
    "  mat4 _IM    = _instance_matrices[gl_InstanceIndex];\n"
    "  position    = vec4(xformPoint(_IM, position.xyz), 1.0);\n"
    "  normal      = xformVec(_IM, normal);\n"
    "  binormal    = xformVec(_IM, binormal);\n"
    "  // E.2: per-instance data is the TYPED attrs SSBO (x=type_id, y=seed01) -> frg_clr.\n"
    "  // (the matrix-bottom-row smuggle is RETIRED; rows are pure transform again.)\n"
    "  vtxcolor    = _instance_attrs[gl_InstanceIndex];\n"
    "%s%s\n"
    "}\n"
    "technique FWD_SSBO_CUSTOM_INSTANCED {\n"
    "  fxconfig = fxcfg_default;\n"
    "  vf_pass = { vs_ptex_ssbo_inst, ps_ptex_forward, sb_ptex }\n"
    "}\n"
    "///////////////////////////////////////////////////////////////\n"
    "// FWD_SSBO_CUSTOM_INSTANCED_DEPTHPREPASS — instanced SSBO-pull, depth-only (E.4:\n"
    "// early-z for instanced hypermesh; same position math as the color pass -> no z-fight).\n"
    "vertex_shader vs_ptex_ssbo_inst_dpp\n"
    "  : vif_ptex_ssbo_dpp\n"
    "  : lib_inst_xform\n"
    "  : storage_inst_mtx\n"
    "  : ublk_std_matrices%s {\n"
    "%s\n"
    "  mat4 _IM    = _instance_matrices[gl_InstanceIndex];\n"
    "  position    = vec4(xformPoint(_IM, position.xyz), 1.0);\n"
    "  vec4 _hpos  = mvp * position;\n"
    "  gl_Position = _hpos;\n"
    "  frg_camz    = _hpos.z / _hpos.w;\n"
    "}\n"
    "technique FWD_SSBO_CUSTOM_INSTANCED_DEPTHPREPASS {\n"
    "  fxconfig = fxcfg_default;\n"
    "  vf_pass = { vs_ptex_ssbo_inst_dpp, ps_ptex_dpp, sb_ptex }\n"
    "}\n") % (inh, body, vs_tail, post, inh, body)) if ssbo_instanced else ""
  return (
    "///////////////////////////////////////////////////////////////\n"
    "// FWD_SSBO_CUSTOM — SSBO-sourced vertices (custom pull VS + custom std430 layout),\n"
    "// paired with the SHARED forward fragment. Empty vertex input; rendered via the\n"
    "// indirect pull path. Selected BY NAME (e.g. ComputeDrawable). The SSBO is filled by\n"
    "// the consumer (optionally by the ssbo_compute spliced below, sharing this program).\n"
    "storage_interface sif_ptex_vtx (descriptor_set 0) {\n"
    "  buffer layout(std430) ptex_vtx_data {\n"
    "%s\n"
    "  };\n"
    "}\n"
    "%s"
    "%s"
    "vertex_interface vif_ptex_ssbo : ub_std_vtx : sif_ptex_vtx {\n"
    "  outputs {\n"
    "%s  }\n"
    "}\n"
    "vertex_shader vs_ptex_ssbo\n"
    "  : vif_ptex_ssbo\n"
    "  : lib_pbr_vtx\n"
    "  : ublk_std_matrices%s {\n"
    "  // pull body: decode gl_VertexID -> position/normal/binormal/uv0/vtxcolor (DSL-supplied)\n"
    "%s\n"
    "%s%s\n"
    "}\n"
    "technique FWD_SSBO_CUSTOM {\n"
    "  fxconfig = fxcfg_default;\n"
    "  vf_pass = { vs_ptex_ssbo, ps_ptex_forward, sb_ptex }\n"
    "}\n"
    "%s"
    "%s"
    "%s") % (layout, extra_blocks, lib_block, vtx_outputs, inh, body, vs_tail, post, dpp_block, inst_block, compute_block)


# unlit/blend support: the FS "shade" block (lit lighting vs unlit emissive) + the rasterstate
# tokens are parameterized so surface_mode + blend select the output. `%s` = the alpha expr
# (s.opacity when blending, else 1.0). lit-opaque defaults keep the generated FS byte-identical.
_LIT_SHADE = (
  "  vec3 ambrufmtl = vec3(s.ao, _rough, s.metallic);   // ambrufmtl.x = AO -> _forward_lightingZ threads it to ambient/diffuse IBL (matAO)\n"
  "  ShadingResult sr = _forward_lightingZ(\n"
  "    ModColor.xyz, s.albedo, ambrufmtl, s.emissive, EyePostion, s.normal, false);\n"
  "  out_clr     = vec4(sr.specular + sr.diffuse, %s);\n"
  "  out_diffuse = vec4(sr.diffuse, 0.0);")
_UNLIT_SHADE = (
  "  out_clr     = vec4(s.emissive, %s);   // unlit: emissive color straight out, no lighting\n"
  "  out_diffuse = vec4(0.0);")
_BLEND_TOK = {"off": "OFF", "alpha": "ALPHA", "additive": "ADDITIVE", "alpha_additive": "ALPHA_ADDITIVE"}
_DTEST_TOK = {"off": "OFF", "less": "LESS", "leq": "LEQUALS", "lequals": "LEQUALS",
              "greater": "GREATER", "always": "ALWAYS"}
_CULL_TOK  = {"off": "OFF", "none": "OFF", "front": "PASS_FRONT", "back": "PASS_BACK"}


def generate_surface_fxv2(surface_body,
                          *,
                          libblock="",
                          lib_inherits=(),
                          extra_imports=(),
                          params=(),
                          samplers=(),
                          height_body="",
                          height_expr="",
                          displace_scale=0.05,
                          cellular_body="",
                          cellular_coord="",
                          cellular_width="",
                          cellular_scale=0.1,
                          parallax_steps=0,
                          parallax_depth="",
                          ssbo_layout="",
                          ssbo_lib="",
                          ssbo_vs_body="",
                          ssbo_compute="",
                          ssbo_vs_inherits=(),
                          ssbo_extra_blocks="",
                          ssbo_vs_post="",
                          ssbo_instanced=False,
                          surf_storage="",
                          surf_storage_inherits=(),
                          surf_body_append="",
                          surface_mode="lit",
                          blend="off",
                          depth_test="leq",
                          depth_write=True,
                          cull="front"):
  """Assemble a complete forward-PBR .fxv2 around a GLSL surface body.

  surf_storage          : optional storage_interface decl(s) the FRAGMENT surface reads (e.g. a
                          per-triangle face-id buffer for face-viz). Declared top-level + inherited
                          by lib_ptex_surface so the surface body can index it (graphics-side).
  surf_storage_inherits : the interface names to inherit on lib_ptex_surface (e.g. ("sif_triface",)).
  surf_body_append      : raw GLSL appended AFTER the DSL surface body (so it can override o.albedo
                          etc. using surf_storage + FS builtins like gl_PrimitiveID).

  surface_body : GLSL statements assigning to o.<field> (see module docstring).
  libblock     : optional GLSL helper functions the body calls (e.g. fbm/worley).
  lib_inherits : libblock names the surface needs in scope (e.g. "lib_mmnoise",
                 "lib_sdftools"); inherited on the surface libblock.
  extra_imports: extra orkshader:// .i2 files the lib_inherits come from beyond
                 the standard set (e.g. "orkshader://sdftools.i2").
  params       : [(name, gtype, default), ...] bindable runtime uniforms — emits
                 a ublk_ptex_params block inherited by the surface libblock.
  """
  # FS-side surface storage (face-viz: a per-triangle face-id buffer) + an optional raw-GLSL append
  # to the surface body (so it can read that storage + FS builtins like gl_PrimitiveID).
  surf_storage_block   = (surf_storage.strip() + "\n") if (surf_storage and surf_storage.strip()) else ""
  surf_storage_inherit = "".join(" : %s" % n for n in surf_storage_inherits)
  # raw GLSL injected into the FS main AFTER the ptex_surface() call — so it can read the FS storage
  # (surf_storage) + FS builtins (gl_PrimitiveID; only valid in the fragment stage, not the libblock)
  # and override the computed SurfaceOut `s` (s.albedo/s.emissive/...). Used by TopoView (face-viz).
  surf_fs_post = ("\n" + _indent(surf_body_append.strip(), 2)) if (surf_body_append and surf_body_append.strip()) else ""

  imports = list(_STD_IMPORTS) + [i for i in extra_imports if i not in _STD_IMPORTS]
  import_lines = "\n".join('  import "%s";' % i for i in imports)
  # the surface libblock inherits its needed noise/util libblocks
  surf_inherits = "".join(" : %s" % n for n in lib_inherits)
  out_struct = (
    "  struct SurfaceOut { vec3 albedo; float metallic; float roughness; vec3 normal; vec3 emissive; float ao; float opacity; };\n"
    "  struct ptex_voro_t { float f1; float edge; float fwedge; float cellA; float cellB; };")
  params_block, params_inherit = _params_block(params)
  samplers_block, samplers_inherit = _samplers_block(samplers)
  if cellular_coord and str(cellular_coord).strip():
    # analytic cellular relief: no ptex_height (no finite-diff taps)
    height_function = ""
    bump_block = _cellular_bump_block(cellular_body, cellular_coord, cellular_width, cellular_scale)
  else:
    height_function, bump_block = _height_blocks(height_body, height_expr, displace_scale)
  # PROCEDURAL parallax march (general displace path only — needs ptex_height).
  parallax_block = _parallax_block(parallax_steps, parallax_depth) if height_function else ""

  # ctx.NV (VIEW-space normal) is OPT-IN: emit the varying + the per-vertex transform
  # + the surface() param ONLY when the body references `vnrm`, so materials that
  # don't use it pay nothing (no extra varying, no vertex math). Computed per-vertex
  # as mat3(mv)*normal (object->view; `mv` is already bound — no rebuild needed). The
  # exact/per-fragment form would instead read the bound `mvit_rot` normal matrix.
  uses_vnrm = "vnrm" in surface_body
  vif_vnrm        = "    vec3 frg_vnrm;\n" if uses_vnrm else ""
  vs_vnrm         = ("  frg_vnrm = normalize(mat3(mv) * normal);   // object->view (per-vertex)\n"
                     if uses_vnrm else "")
  surf_vnrm_param = ", vec3 vnrm" if uses_vnrm else ""
  surf_vnrm_arg   = ", normalize(frg_vnrm)" if uses_vnrm else ""

  # the varying contract + VS tail are shared by the rigid VS and the optional SSBO-pull VS.
  vtx_outputs = _VTX_OUTPUTS.format(vif_vnrm=vif_vnrm)
  vs_tail     = _VS_TAIL.format(vs_vnrm=vs_vnrm)
  ssbo_block  = _ssbo_block(ssbo_layout, ssbo_lib, ssbo_vs_body, ssbo_compute,
                            ssbo_vs_inherits, vtx_outputs, vs_tail,
                            ssbo_extra_blocks=ssbo_extra_blocks, ssbo_vs_post=ssbo_vs_post,
                            ssbo_instanced=ssbo_instanced)

  # unlit/blend: pick the FS shade body (lit lighting vs unlit emissive) + the rasterstate tokens.
  _alpha       = "s.opacity" if str(blend) != "off" else "1.0"
  fs_shade     = (_LIT_SHADE if surface_mode == "lit" else _UNLIT_SHADE) % _alpha
  st_blend     = _BLEND_TOK[blend]
  st_depthtest = _DTEST_TOK[depth_test]
  st_cull      = _CULL_TOK[cull]
  st_depthmask = "ON" if depth_write else "OFF"
  return _TEMPLATE.format(
    fs_shade=fs_shade,
    st_blend=st_blend,
    st_depthtest=st_depthtest,
    st_cull=st_cull,
    st_depthmask=st_depthmask,
    import_lines=import_lines,
    surf_inherits=surf_inherits,
    out_struct=out_struct,
    params_block=("\n" + params_block + "\n" if params_block else ""),
    params_inherit=params_inherit,
    samplers_block=("\n" + samplers_block + "\n" if samplers_block else ""),
    samplers_inherit=samplers_inherit,
    libblock=("\n" + libblock.strip() + "\n" if libblock.strip() else ""),
    height_function=("\n" + height_function + "\n" if height_function else ""),
    parallax_block=("\n" + parallax_block if parallax_block else ""),
    bump_block=("\n" + bump_block if bump_block else ""),
    surface_body=_indent(surface_body.strip(), 4),
    vtx_outputs=vtx_outputs,
    vs_tail=vs_tail,
    ssbo_block=("\n" + ssbo_block if ssbo_block else ""),
    surf_vnrm_param=surf_vnrm_param,
    surf_vnrm_arg=surf_vnrm_arg,
    surf_storage_block=surf_storage_block,
    surf_storage_inherit=surf_storage_inherit,
    surf_fs_post=surf_fs_post,
  )


def materialize_surface_fxv2(surface_body,
                             *,
                             libblock="",
                             lib_inherits=(),
                             extra_imports=(),
                             params=(),
                             samplers=(),
                             height_body="",
                             height_expr="",
                             displace_scale=0.05,
                             cellular_body="",
                             cellular_coord="",
                             cellular_width="",
                             cellular_scale=0.1,
                             parallax_steps=0,
                             parallax_depth="",
                             ssbo_layout="",
                             ssbo_lib="",
                             ssbo_vs_body="",
                             ssbo_compute="",
                             ssbo_vs_inherits=(),
                             ssbo_extra_blocks="",
                             ssbo_vs_post="",
                             ssbo_instanced=False,
                             surf_storage="",
                             surf_storage_inherits=(),
                             surf_body_append="",
                             surface_mode="lit",
                             blend="off",
                             depth_test="leq",
                             depth_write=True,
                             cull="front",
                             name_hint="ptex"):
  """Generate + write the .fxv2 to <staging>/dslshadercache/ptex3d/<hint>_<hash>.fxv2.

  Content-addressed: identical generated text reuses the same file (and the
  downstream SPIR-V DataBlockCache dedups compilation). Returns the
  RELOCATABLE "<staging>/..." token reference (owner policy: DSL-generated
  shaders live in the staging cache, namespaced per family — never in the
  source tree; the .shaders/-next-to-asset form is RETIRED).
  """
  text = generate_surface_fxv2(surface_body,
                               libblock=libblock,
                               lib_inherits=lib_inherits,
                               extra_imports=extra_imports,
                               params=params,
                               samplers=samplers,
                               height_body=height_body,
                               height_expr=height_expr,
                               displace_scale=displace_scale,
                               cellular_body=cellular_body,
                               cellular_coord=cellular_coord,
                               cellular_width=cellular_width,
                               cellular_scale=cellular_scale,
                               parallax_steps=parallax_steps,
                               parallax_depth=parallax_depth,
                               ssbo_layout=ssbo_layout,
                               ssbo_lib=ssbo_lib,
                               ssbo_vs_body=ssbo_vs_body,
                               ssbo_compute=ssbo_compute,
                               ssbo_vs_inherits=ssbo_vs_inherits,
                               ssbo_extra_blocks=ssbo_extra_blocks,
                               ssbo_vs_post=ssbo_vs_post,
                               ssbo_instanced=ssbo_instanced,
                               surf_storage=surf_storage,
                               surf_storage_inherits=surf_storage_inherits,
                               surf_body_append=surf_body_append,
                               surface_mode=surface_mode,
                               blend=blend,
                               depth_test=depth_test,
                               depth_write=depth_write,
                               cull=cull)
  digest = hashlib.sha1((CODEGEN_VERSION + "\n" + text).encode("utf-8")).hexdigest()[:16]
  fname  = "%s_%s.fxv2" % (name_hint, digest)
  return dslcache_write("ptex3d", fname, text)


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
  CullTest  = {st_cull};
  DepthTest = {st_depthtest};
  DepthMask = {st_depthmask};
  BlendMode = {st_blend};
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
{vtx_outputs}  }}
}}
///////////////////////////////////////////////////////////////
{surf_storage_block}fragment_interface fif_ptex
  : vif_ptex
  : ub_frg_fwd
  : storage_fwd_lighting{surf_storage_inherit}
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
{vs_tail}
}}
///////////////////////////////////////////////////////////////
typeblock types_ptex {{
{out_struct}
}}{params_block}{samplers_block}
///////////////////////////////////////////////////////////////
libblock lib_ptex_surface : types_ptex{params_inherit}{samplers_inherit}{surf_inherits} {{
{libblock}{height_function}
  SurfaceOut ptex_surface(vec3 wpos, vec3 opos, vec2 uv, vec4 cd, mat3 tbn, vec3 wnrm, vec3 onrm, vec3 eye{surf_vnrm_param}) {{
    SurfaceOut o;
    o.albedo   = vec3(0.8);
    o.metallic = 0.0;
    o.roughness= 0.5;
    o.normal   = wnrm;
    o.emissive = vec3(0.0);
    o.ao       = 1.0;
    o.opacity  = 1.0;
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
  vec3 opos = frg_opos;        // object position; PARALLAX displaces it, bump + surface read it
{parallax_block}
{bump_block}
  SurfaceOut s = ptex_surface(frg_wpos.xyz, opos, frg_uv0, frg_clr, frg_tbn, wnrm, onrm, EyePostion{surf_vnrm_arg});{surf_fs_post}
  // TEMP roughness linearization (stopgap — REMOVE when the PBR importance-
  // sampling code is calibrated). Maps authored roughness into the
  // perceptually-visible band [0.3, 1.0] (floor 0.3 keeps low cells shiny).
  // Tunable: the affine (scale/bias) sets the band, the pow exponent the curve.
  float _rough = pow(s.roughness * 0.8 + 0.2, 1.0);
  {{  // GEOMETRIC SPECULAR AA (Kaplanyan 2016 / Tokuyoshi 2017).
     // The procedural bump injects high-frequency normal detail; on glossy
     // surfaces the sub-pixel normal spread aliases the highlight. Measure that
     // spread from screen derivatives of the shading normal and widen the NDF
     // (raise roughness) to cover it. Identity where the normal is smooth.
    vec3  _dnx = dFdx(s.normal);
    vec3  _dny = dFdy(s.normal);
    float _var = 0.5 * (dot(_dnx, _dnx) + dot(_dny, _dny));   // SPECULAR_AA_VARIANCE
    float _ker = min(2.0 * _var, 0.18);                        // SPECULAR_AA_THRESHOLD
    float _a2  = _rough * _rough;                              // -> alpha domain
    _rough = sqrt(sqrt(clamp(_a2 * _a2 + _ker, 0.0, 1.0)));    // back to perceptual
  }}
{fs_shade}
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
}}{ssbo_block}
'''
