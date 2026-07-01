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


def _samplers_block(samplers, setname="sset_ptex_tex"):
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
    "sampler_set %s (descriptor_set 0) {\n%s\n}" % (setname, members))
  return block, " : %s" % setname


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
  "  frg_uv0     = {fwd_uv};\n"   # planar uv0 by default; the relax stored-forward fills ruv (atlas param)
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

# CAPTURE VS tail — the atlas-bake "UV rasterizer" (report §1.2). gl_Position comes from the atlas
# parameterization ({cap_uv}) instead of mvp*position: with cap_uv=uv0 (planar) this is byte-identical to
# the top-down ortho MVP (worldXZ/extent -> NDC, up=-Z gives the v flip), so existing atlases are
# unchanged; with cap_uv=ruv (the relax_uv module's RELAXED uv) the atlas bakes in RELAXED space -> steep
# faces get an equal texel budget. frg_uv0 stays PLANAR (fwd_uv=uv0) so the capture fragment's channel
# taps (surface()) read the correct grid cell. Everything after gl_Position matches _VS_TAIL.
_CAP_VS_TAIL = _VS_TAIL.replace(
  "  gl_Position = mvp * position;\n",
  "  gl_Position = vec4(2.0*{cap_uv}.x - 1.0, 1.0 - 2.0*{cap_uv}.y, 0.0, 1.0);  // UV-space rasterize (atlas param)\n")


def _ssbo_block(ssbo_layout, ssbo_lib, ssbo_vs_body, ssbo_compute, ssbo_vs_inherits, vtx_outputs, vs_tail,
                ssbo_extra_blocks="", ssbo_vs_post="", ssbo_instanced=False, ssbo_wants_inst_data=False,
                cap_vs_tail="", ssbo_wants_capture=False):
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
  # PER-INSTANCE data for the VS body (e.g. a vtx_displace reading per-tree wind phase/amp/freq). The
  # SAME body runs in every VS variant, but `_instance_attrs` exists only in the INSTANCED variants, so
  # each variant declares `inst_data` differently: the instanced VS reads its per-instance attr vec4, the
  # non-instanced VS gets vec4(0) (a single mesh => phase 0 => unchanged). Off by default => byte-identical
  # output for materials with no displace. body_ni = non-instanced body; body_inst = instanced body.
  if ssbo_wants_inst_data:
    body_ni   = "  vec4 inst_data = vec4(0.0);\n" + body
    body_inst = "  vec4 inst_data = _instance_attrs[gl_InstanceIndex];\n" + body
  else:
    body_ni = body_inst = body
  # optional VS snippet spliced AFTER the shared tail (i.e. after gl_Position is computed) — e.g. a
  # clip-space depth bias `gl_Position.z -= b*gl_Position.w`. Default empty -> output byte-identical.
  post = ("\n" + _indent(ssbo_vs_post.strip(), 2)) if (ssbo_vs_post and str(ssbo_vs_post).strip()) else ""
  # capture UV-rasterizer VS (the atlas bake): SAME SSBO-pull body, but gl_Position comes from uv0
  # (cap_vs_tail). Only emitted when wants_capture; FWD_SSBO_CUSTOM_CAPTURE uses it. Byte-identical
  # atlas for a planar uv0; a relaxed uv0 (relax_uv module) -> the atlas bakes in relaxed space.
  cap_vs_block = ""
  if ssbo_wants_capture and str(cap_vs_tail).strip():
    cap_vs_block = (
      "vertex_shader vs_ptex_ssbo_cap\n"
      "  : vif_ptex_ssbo\n"
      "  : lib_pbr_vtx\n"
      "  : ublk_std_matrices%s {\n"
      "%s\n"
      "%s\n"
      "}\n") % (inh, body_ni, cap_vs_tail)
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
    "}\n") % (inh, body_ni)
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
    "  : storage_inst_attr\n"   # inst_data (per-tree wind) is read in body_inst; depth must match color
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
    "}\n") % (inh, body_inst, vs_tail, post, inh, body_inst)) if ssbo_instanced else ""
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
    "%s"   # vs_ptex_ssbo_cap (capture UV-rasterizer; empty unless wants_capture)
    "%s"
    "%s"
    "%s") % (layout, extra_blocks, lib_block, vtx_outputs, inh, body_ni, vs_tail, post,
             cap_vs_block, dpp_block, inst_block, compute_block)


def _capture_struct(capture_targets):
  """The CaptureOut struct (one vec4 per packed target) — declared in the typeblock (types_ptex), exactly
  like SurfaceOut; shadlang requires structs at typeblock scope, not inside a libblock."""
  if not capture_targets:
    return ""
  fields = "\n".join("  vec4 %s;" % t for t in capture_targets)
  return ("\n  struct CaptureOut {\n%s\n  };" % fields)


def _capture_func(capture_body, capture_targets, surf_vnrm_param, cap_samplers_inherit=""):
  """lib_ptex_capture — the ptex_capture() function, ISOLATED in its own libblock (inherits lib_ptex_surface
  for helpers/types/params + the capture-ONLY sampler set sset_ptex_cap). Only ps_ptex_capture inherits THIS,
  so ps_ptex_forward does NOT carry the capture exprs' samplers (which would push the forward FS past Metal's
  16-samplers/stage cap -> MSL 'sampler out of bounds' -> vkCreateGraphicsPipelines fails). Returns the
  CaptureOut struct (declared in the typeblock): one vec4 per packed target (`c.<target> = vec4(...)`)."""
  if not capture_targets:
    return ""
  return ('''
///////////////////////////////////////////////////////////////
libblock lib_ptex_capture : lib_ptex_surface%s {
  CaptureOut ptex_capture(vec3 wpos, vec3 opos, vec2 uv, vec4 cd, mat3 tbn, vec3 wnrm, vec3 onrm, vec3 eye%s) {
    CaptureOut c;
%s
    return c;
  }
}''' % (cap_samplers_inherit, surf_vnrm_param, _indent(capture_body, 4)))


def _capture_block(capture_targets, surf_vnrm_arg, surf_storage_inherit):
  """FWD_SSBO_CUSTOM_CAPTURE technique. EXPLICIT named/packed captures (capture_targets given) -> one MRT
  location per target, filled by ptex_capture(). Otherwise (impostor) -> the whole-surface fixed 3-MRT
  from ptex_surface (premultiplied PBR, the geometry-LOD bake). EyePostion comes from ublk_std_pbr."""
  if capture_targets:
    outs   = "\n".join("    layout(location = %d) vec4 out_%s;" % (i, t) for i, t in enumerate(capture_targets))
    routes = "\n".join("  out_%s = c.%s;" % (t, t) for t in capture_targets)
    # DEBUG (env-gated at CODEGEN time): override the FIRST capture target with a chosen surface() INPUT
    # so one stored-mode bake reveals which input collapses across the atlas. Set ORKID_PTEX_CAP_DBG to:
    #   uv0  -> frg_uv0 (the VS-decoded planar uv; clean gradient => decode/geometry fine)
    #   wpos -> world position xz (height-read driven; constant => terr_pos/SSBO height broken)
    #   onrm -> object normal (the SSBO normal read)
    #   opos -> object position xz
    # The override changes the shader text -> new cap_dir -> a guaranteed fresh (non-stale) debug bake.
    _dbg = os.environ.get("ORKID_PTEX_CAP_DBG", "").strip()
    if _dbg:
      _t0 = capture_targets[0]
      _t1 = capture_targets[1] if len(capture_targets) > 1 else _t0
      _UV0  = "  out_%s = vec4(frg_uv0.x, frg_uv0.y, 0.0, 1.0);" % _t0
      _WPOS = "  out_%s = vec4(fract(frg_wpos.x*0.001), fract(frg_wpos.z*0.001), fract(frg_wpos.y*0.004), 1.0);" % _t1
      _dbgmap = {
        # one-bake combined: base = uv0 (VS decode/geometry), nrmao = world-pos (height/SSBO read)
        "diag": _UV0 + "\n" + _WPOS,
        "uv0":  _UV0,
        "wpos": "  out_%s = vec4(fract(frg_wpos.x*0.001), fract(frg_wpos.z*0.001), 0.0, 1.0);" % _t0,
        "onrm": "  out_%s = vec4(onrm*0.5 + 0.5, 1.0);" % _t0,
        "opos": "  out_%s = vec4(fract(frg_opos.x*0.001), fract(frg_opos.z*0.001), 0.0, 1.0);" % _t0,
      }
      if _dbg in _dbgmap:
        routes = routes + "\n" + _dbgmap[_dbg] + "   // ORKID_PTEX_CAP_DBG=" + _dbg
    return ('''///////////////////////////////////////////////////////////////
// FWD_SSBO_CUSTOM_CAPTURE — explicit named captures -> packed MRT (one target per location, via ptex_capture).
///////////////////////////////////////////////////////////////
fragment_interface fif_ptex_capture
  : vif_ptex
  : ublk_std_pbr%s {
  outputs {
%s
  }
}
fragment_shader ps_ptex_capture
  : fif_ptex_capture
  : lib_math
  : lib_def
  : lib_ptex_capture {
  vec3 wnrm = normalize(frg_tbn[2]);
  vec3 onrm = normalize(frg_onrm);
  CaptureOut c = ptex_capture(frg_wpos.xyz, frg_opos, frg_uv0, frg_clr, frg_tbn, wnrm, onrm, EyePostion%s);
%s
}
technique FWD_SSBO_CUSTOM_CAPTURE {
  fxconfig = fxcfg_default;
  vf_pass = { vs_ptex_ssbo_cap, ps_ptex_capture, sb_ptex }
}''' % (surf_storage_inherit, outs, surf_vnrm_arg, routes))
  # IMPOSTOR — whole-surface fixed 3-MRT from SurfaceOut (premultiplied by coverage); the atlas clears to
  # (0,0,0,0) so the silhouette mip blend is a coverage-WEIGHTED sum (the billboard FS unpremultiplies).
  return ('''///////////////////////////////////////////////////////////////
// FWD_SSBO_CUSTOM_CAPTURE — impostor bake: SSBO-pull VS + raw-PBR-to-MRT fragment (no lighting).
//   loc0 = albedo.rgb, opacity (coverage)   loc1 = worldNormal*0.5+0.5, ao   loc2 = metallic, roughness
///////////////////////////////////////////////////////////////
fragment_interface fif_ptex_capture
  : vif_ptex
  : ublk_std_pbr%s {
  outputs {
    layout(location = 0) vec4 out_albedo_cov;
    layout(location = 1) vec4 out_normal_ao;
    layout(location = 2) vec4 out_metal_rough;
  }
}
fragment_shader ps_ptex_capture
  : fif_ptex_capture
  : lib_math
  : lib_def
  : lib_ptex_surface {
  vec3 wnrm = normalize(frg_tbn[2]);
  vec3 onrm = normalize(frg_onrm);
  vec3 opos = frg_opos;
  SurfaceOut s = ptex_surface(frg_wpos.xyz, opos, frg_uv0, frg_clr, frg_tbn, wnrm, onrm, EyePostion%s);
  float _cov      = s.opacity;
  out_albedo_cov  = vec4(s.albedo * _cov, _cov);
  out_normal_ao   = vec4((normalize(s.normal) * 0.5 + 0.5) * _cov, s.ao * _cov);
  out_metal_rough = vec4(s.metallic * _cov, s.roughness * _cov, 0.0, _cov);
}
technique FWD_SSBO_CUSTOM_CAPTURE {
  fxconfig = fxcfg_default;
  vf_pass = { vs_ptex_ssbo, ps_ptex_capture, sb_ptex }
}''' % (surf_storage_inherit, surf_vnrm_arg))


def _impostor_block(vtx_outputs):
  """FWD_SSBO_CUSTOM_IMPOSTOR — the impostor BILLBOARD draw technique (the far LOD tier). A camera-facing
  quad per instance (gl_VertexID corner + storage_inst_mtx[gl_InstanceIndex] placement); surface() SAMPLES
  the baked hemi-oct atlas (ImpAlbedo/ImpNormal/ImpMetalRough) and runs the SAME forward PBR lighting
  (_forward_lightingZ) as the mesh LOD tiers — so the impostor COLOR-MATCHES the real mesh. The atlas
  textures + grid/radius are PBRMaterial params bound by the forward pipeline's impostor branch (fwdnode).
  Emitted alongside the capture technique (impostor=True), instanced materials only (needs storage_inst_mtx).
  The billboard VS reuses the forward varying contract (vtx_outputs) + fif_ptex's forward env, exactly like
  FWD_SSBO_CUSTOM pairs an SSBO VS with the shared forward fragment — only the surface SOURCE differs."""
  return ('''///////////////////////////////////////////////////////////////
// FWD_SSBO_CUSTOM_IMPOSTOR — LOD billboard: SSBO instance-matrix pull -> camera-facing quad -> atlas
// sample -> the SAME forward PBR lighting as the mesh (color-matched). Emitted with the capture technique.
///////////////////////////////////////////////////////////////
sampler_set sset_impostor (descriptor_set 0) {
  sampler2D ImpAlbedo;     // rgb=albedo, a=coverage
  sampler2D ImpNormal;     // rgb=worldNormal*0.5+0.5, a=ao
  sampler2D ImpMetalRough; // r=metallic, g=roughness
}
uniform_block ublk_impostor (descriptor_set 0) {
  vec4 ImpCenter;   // xyz = OBJECT-space bbox center the bake recentered on, w = bound radius (ortho half-extent)
  vec4 ImpGrid;     // x = hemi-oct grid N (atlas is N x N tiles); vec4-padded (UBO alignment)
}
libblock lib_octa {
  // world dir (upper hemisphere) -> CONTINUOUS hemi-oct atlas uv in [0,1] (inverse of the bake forward map),
  // NOT floored to a cell. The FS converts this to a grid coord and blends the 3 nearest views.
  vec2 octa_uv(vec3 dir) {
    vec3 d    = dir / max(abs(dir.x) + abs(dir.y) + abs(dir.z), 1e-5);
    float ex  = d.x + d.z;
    float ey  = d.x - d.z;
    return clamp(vec2(ex, ey) * 0.5 + 0.5, 0.0, 1.0);
  }
  // one premultiplied tap from the tile at grid CELL (clamped), at the billboard-local uv (V-flipped).
  vec4 octa_tap(sampler2D atl, vec2 cell, vec2 uvloc, float gridN) {
    vec2 torg = clamp(cell, vec2(0.0), vec2(gridN - 1.0)) / gridN;
    return texture(atl, torg + uvloc / gridN);
  }
}
// billboard VS interface — NO geometry SSBO (the quad is built from gl_VertexID); just the std vertex UBO
// + the forward varying contract (matches fif_ptex_impostor's vif_ptex outputs, like vif_ptex_ssbo).
vertex_interface vif_ptex_impostor_vtx : ub_std_vtx {
  outputs {
%s  }
}
vertex_shader vs_ptex_impostor
  : vif_ptex_impostor_vtx
  : storage_inst_mtx
  : ublk_std_matrices
  : ublk_impostor {
  vec2 corner = vec2(((gl_VertexID == 1) || (gl_VertexID == 2)) ? 1.0 : -1.0,
                     ((gl_VertexID == 2) || (gl_VertexID == 3)) ? 1.0 : -1.0);
  mat4 inst   = _instance_matrices[gl_InstanceIndex];
  // center the billboard on the mesh's bbox center IN WORLD SPACE (the bake recentered the atlas there), not
  // the instance origin — otherwise the quad floats low under the mesh (the origin sits at the tree base).
  vec3 center = (inst * vec4(ImpCenter.xyz, 1.0)).xyz;
  float scale = length(inst[0].xyz);         // uniform scale
  vec3 right  = normalize(inv_v[0].xyz);     // camera-facing billboard basis (inverse-view)
  vec3 up     = normalize(inv_v[1].xyz);
  vec3 wpos   = center + (corner.x * right + corner.y * up) * (scale * ImpCenter.w);
  gl_Position = mvp * vec4(wpos, 1.0);       // mvp = VP (the hypermesh drawable's model is identity)
  frg_wpos    = vec4(wpos, 1.0);
  frg_uv0     = corner * 0.5 + 0.5;
  frg_clr     = vec4(1.0);
  frg_tbn     = mat3(right, up, normalize(cross(right, up)));
  frg_opos    = wpos;
  frg_onrm    = vec3(0.0, 1.0, 0.0);
  frg_obin    = vec3(1.0, 0.0, 0.0);
  frg_camdist = gl_Position.w;
  frg_camz    = vec3(0.0);
}
fragment_interface fif_ptex_impostor
  : vif_ptex
  : ub_frg_fwd
  : storage_fwd_lighting
  : ss_frg_fwd
  : sset_impostor
  : ublk_impostor {
  outputs {
    layout(location = 0) vec4 out_clr;
    layout(location = 1) vec4 out_diffuse;
  }
}
fragment_shader ps_ptex_impostor
  : fif_ptex_impostor
  : lib_math
  : lib_brdf
  : lib_def
  : lib_fwd
  : lib_octa
  : std_forward_all {
  // 3-VIEW OCTAHEDRAL BLEND: the per-instance view dir maps to a CONTINUOUS hemi-oct grid coord; the impostor
  // blends the 3 nearest baked views (barycentric over the cell's triangle) so the silhouette/angle morph
  // smoothly instead of snapping between the 8x8 discrete views (the pop at the mesh<->impostor switch).
  vec3  vdir  = normalize(EyePostion - frg_wpos.xyz);
  float gridN = max(ImpGrid.x, 1.0);
  // grid coord, centered: the bake sampled cell CENTERS ((i+0.5)/gridN), so subtract 0.5 -> integer at a view.
  vec2  gc    = octa_uv(vdir) * gridN - 0.5;
  vec2  cell  = floor(gc);
  vec2  f     = gc - cell;
  // the unit cell is split into 2 triangles along the diagonal; pick the one containing f + its 3 corners.
  vec3  w  = vec3(1.0 - f.x - f.y, f.x, f.y);
  vec2  c0 = cell;
  vec2  c1 = cell + vec2(1.0, 0.0);
  vec2  c2 = cell + vec2(0.0, 1.0);
  if (f.x + f.y >= 1.0) {
    w  = vec3(f.x + f.y - 1.0, 1.0 - f.y, 1.0 - f.x);
    c0 = cell + vec2(1.0, 1.0);
  }
  // V-flipped billboard-local uv (atlas stored V-flipped vs the billboard — render-to-texture Y convention).
  vec2  uvloc = vec2(frg_uv0.x, 1.0 - frg_uv0.y);
  // weighted-sum the PREMULTIPLIED taps from the 3 view tiles, then unpremultiply the blend ONCE.
  vec4  alb = w.x * octa_tap(ImpAlbedo,     c0, uvloc, gridN) + w.y * octa_tap(ImpAlbedo,     c1, uvloc, gridN) + w.z * octa_tap(ImpAlbedo,     c2, uvloc, gridN);
  vec4  nao = w.x * octa_tap(ImpNormal,     c0, uvloc, gridN) + w.y * octa_tap(ImpNormal,     c1, uvloc, gridN) + w.z * octa_tap(ImpNormal,     c2, uvloc, gridN);
  vec4  mr  = w.x * octa_tap(ImpMetalRough, c0, uvloc, gridN) + w.y * octa_tap(ImpMetalRough, c1, uvloc, gridN) + w.z * octa_tap(ImpMetalRough, c2, uvloc, gridN);
  float cov = alb.a;                          // blended coverage (atlas is PREMULTIPLIED by coverage)
  // silhouette coverage cutoff: the mip chain + 3-view blend DILATE thin features, so a hard discard<cutoff
  // gives the right canopy DENSITY but an aliased edge. covMid = that cutoff (ImpGrid.z = imposter(coverage=),
  // default 0.25); the A2C alpha is a smoothstep CENTERED on it (50%% crossover == the cutoff) so density is
  // preserved while the silhouette edge anti-aliases over +/- covAA.
  float covMid = (ImpGrid.z > 0.0) ? ImpGrid.z : 0.25;
  float covAA  = 0.025;                         // half-width of the soft AA band around the cutoff
  // DISTANCE FADE (ImpGrid.y = max draw distance, = cull_distance): a per-pixel HASHED screen-door dither
  // over the last 15%% before maxDist. Order-independent + MSAA-agnostic (~256 levels), so a SMOOTH whole-
  // object dissolve — A2C alone would step at (msaa+1) levels. A2C below handles the silhouette EDGE AA.
  float maxDist = ImpGrid.y;
  if (maxDist > 0.0) {
    float dist = length(EyePostion - frg_wpos.xyz);
    float fade = 1.0 - smoothstep(maxDist * 0.85, maxDist, dist);
    float dith = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    if (fade < dith) { discard; }
  }
  if (cov < covMid - covAA) { discard; }      // below the AA band -> fully transparent (fill savings)
  // UNPREMULTIPLY: divide by the blended coverage -> the coverage-weighted average of the COVERED texels.
  // The cleared (a:0) background contributed 0 to both numerator and denominator, so no contaminated edges.
  float inv = 1.0 / cov;
  vec3  albedo    = alb.rgb * inv;
  vec3  N         = normalize((nao.rgb * inv) * 2.0 - 1.0);
  vec3  ambrufmtl = vec3(nao.a * inv, mr.y * inv, mr.x * inv); // ao, roughness, metallic
  ShadingResult sr = _forward_lightingZ(ModColor.xyz, albedo, ambrufmtl, vec3(0.0), EyePostion, N, false);
  out_clr     = vec4(sr.specular + sr.diffuse, smoothstep(covMid - covAA, covMid + covAA, cov));
  out_diffuse = vec4(sr.diffuse, 0.0);
}
technique FWD_SSBO_CUSTOM_IMPOSTOR {
  fxconfig = fxcfg_default;
  vf_pass = { vs_ptex_impostor, ps_ptex_impostor, sb_ptex }
}''' % vtx_outputs)


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
                          ssbo_wants_inst_data=False,
                          surf_storage="",
                          surf_storage_inherits=(),
                          surf_body_append="",
                          surface_mode="lit",
                          blend="off",
                          depth_test="leq",
                          depth_write=True,
                          cull="front",
                          alpha_to_coverage=False,
                          wants_capture=False,
                          capture_body="",
                          capture_targets=(),
                          capture_samplers=(),
                          relax_uv=False):
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
  # capture-only samplers go to a SEPARATE set (sset_ptex_cap) inherited only by lib_ptex_capture, so the
  # forward FS stays under Metal's 16-samplers/stage cap.
  cap_samplers_block, cap_samplers_inherit = _samplers_block(capture_samplers, "sset_ptex_cap")
  if cap_samplers_block:
    samplers_block = (samplers_block + "\n" + cap_samplers_block) if samplers_block else cap_samplers_block
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
  # the capture UV-rasterizer VS is needed ONLY for the explicit NAMED-capture path (the terrain
  # proctex atlas — rasterized in the atlas-uv space). The impostor bake keeps mvp*position (a real ortho
  # viewpoint), so it does NOT use the cap VS. Hence: cap VS emitted iff named-capture is wanted.
  wants_cap_vs = bool(wants_capture and capture_targets)
  # RELAX-UV (terrain equal-area atlas): when the vertex source is relaxed it provides a `ruv` local
  # (relaxed uv) alongside uv0 (planar grid uv). The atlas is parameterized by RELAXED uv, so the CAP VS
  # rasterizes at ruv (gl_Position) and the STORED forward fragment (surface_stored = atlas reconstruction)
  # reads frg_uv0 = ruv. The CAP fragment (= surface(), channel taps) keeps frg_uv0 = uv0 (planar grid),
  # and the PROC forward likewise. Contract: surface_stored() samples ONLY the atlas (relaxed); surface()
  # samples ONLY channels (planar). relax_uv=False -> both fall back to uv0 (byte-identical to pre-relax).
  _fwd_uv     = "ruv" if (relax_uv and wants_cap_vs) else "uv0"  # stored+relax forward reads relaxed
  _cap_uv     = "ruv" if relax_uv else "uv0"                     # cap rasterizes at the atlas param
  # the RIGID forward VS (vs_ptex; reads a vertex-attribute uv0) ALWAYS uses planar uv0 — only the
  # SSBO-pull VS has the relaxed `ruv` local. So `vs_tail` (rigid, used in _TEMPLATE) stays planar;
  # `ssbo_vs_tail` (passed to _ssbo_block) carries the relax routing. Wiring them to the SAME tail
  # would emit `frg_uv0 = ruv` into the rigid VS, where `ruv` is undeclared (shader-compile failure).
  vs_tail      = _VS_TAIL.format(vs_vnrm=vs_vnrm, fwd_uv="uv0")
  ssbo_vs_tail = _VS_TAIL.format(vs_vnrm=vs_vnrm, fwd_uv=_fwd_uv)
  cap_vs_tail  = _CAP_VS_TAIL.format(vs_vnrm=vs_vnrm, fwd_uv="uv0", cap_uv=_cap_uv)
  # RELAXED atlas bake — route through the ortho mvp (the SAME projection-matrix path the forward VS and
  # hypermesh use), with position = Prelax (the relaxed worldXZ emitted by gpu_chunk). NOT a direct
  # gl_Position = 2*ruv-1: a direct vec4 built from an SSBO read (with constant z) is mis-rasterized by
  # MoltenVK/Metal below the NDC anti-diagonal — confirmed by an 8-bake bisection (the forward VS, also
  # SSBO-sourced, works precisely because it goes through mvp). The ortho maps Prelax.xz -> 2*ruv-1.
  if _cap_uv == "ruv":
    cap_vs_tail = cap_vs_tail.replace(
      "gl_Position = vec4(2.0*ruv.x - 1.0, 1.0 - 2.0*ruv.y, 0.0, 1.0)",
      "gl_Position = mvp * vec4(Prelax, 1.0)")
  ssbo_block   = _ssbo_block(ssbo_layout, ssbo_lib, ssbo_vs_body, ssbo_compute,
                             ssbo_vs_inherits, vtx_outputs, ssbo_vs_tail,
                             ssbo_extra_blocks=ssbo_extra_blocks, ssbo_vs_post=ssbo_vs_post,
                             ssbo_instanced=ssbo_instanced, ssbo_wants_inst_data=ssbo_wants_inst_data,
                             cap_vs_tail=cap_vs_tail, ssbo_wants_capture=wants_cap_vs)
  # capture technique — OPT-IN (wants_capture), SSBO materials only (needs vs_ptex_ssbo). EXPLICIT named
  # captures (capture_targets) -> packed MRT via ptex_capture (the sibling fn, spliced into lib_ptex_surface
  # below); otherwise the impostor whole-surface fixed-3-MRT from ptex_surface.
  capture_struct = _capture_struct(capture_targets)                                  # -> typeblock
  capture_func   = _capture_func(capture_body, capture_targets, surf_vnrm_param, cap_samplers_inherit)
  capture_block  = _capture_block(capture_targets, surf_vnrm_arg, surf_storage_inherit) if (ssbo_block and wants_capture) else ""
  # the impostor billboard DRAW technique rides the same opt-in; it needs storage_inst_mtx (instanced only).
  impostor_block = _impostor_block(vtx_outputs) if (ssbo_block and wants_capture and ssbo_instanced) else ""

  # unlit/blend: pick the FS shade body (lit lighting vs unlit emissive) + the rasterstate tokens.
  # A2C also needs the fragment to OUTPUT opacity as alpha (alphaToCoverage converts alpha->coverage),
  # even with blend="off" (A2C is order-independent, no blending).
  _alpha       = "s.opacity" if (str(blend) != "off" or alpha_to_coverage) else "1.0"
  fs_shade     = (_LIT_SHADE if surface_mode == "lit" else _UNLIT_SHADE) % _alpha
  st_blend     = _BLEND_TOK[blend]
  st_depthtest = _DTEST_TOK[depth_test]
  st_cull      = _CULL_TOK[cull]
  st_depthmask = "ON" if depth_write else "OFF"
  # A2C: only emit the state-block line when ON, so non-A2C materials stay byte-identical.
  st_a2c_line  = "  AlphaToCoverage = ON;\n" if alpha_to_coverage else ""
  return _TEMPLATE.format(
    fs_shade=fs_shade,
    st_blend=st_blend,
    st_depthtest=st_depthtest,
    st_cull=st_cull,
    st_depthmask=st_depthmask,
    st_a2c_line=st_a2c_line,
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
    capture_struct=capture_struct,
    capture_func=("\n" + capture_func if capture_func else ""),
    capture_block=("\n" + capture_block if capture_block else ""),
    impostor_block=("\n" + impostor_block if impostor_block else ""),
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
                             ssbo_wants_inst_data=False,
                             surf_storage="",
                             surf_storage_inherits=(),
                             surf_body_append="",
                             surface_mode="lit",
                             blend="off",
                             depth_test="leq",
                             depth_write=True,
                             cull="front",
                             alpha_to_coverage=False,
                             wants_capture=False,
                             capture_body="",
                             capture_targets=(),
                             capture_samplers=(),
                             relax_uv=False,
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
                               ssbo_wants_inst_data=ssbo_wants_inst_data,
                               surf_storage=surf_storage,
                               surf_storage_inherits=surf_storage_inherits,
                               surf_body_append=surf_body_append,
                               surface_mode=surface_mode,
                               blend=blend,
                               depth_test=depth_test,
                               depth_write=depth_write,
                               cull=cull,
                               alpha_to_coverage=alpha_to_coverage,
                               wants_capture=wants_capture,
                               capture_body=capture_body,
                               capture_targets=capture_targets,
                               capture_samplers=capture_samplers,
                               relax_uv=relax_uv)
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
{st_a2c_line}}}
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
{out_struct}{capture_struct}
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
}}{capture_func}
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
}}{ssbo_block}{capture_block}{impostor_block}
'''
