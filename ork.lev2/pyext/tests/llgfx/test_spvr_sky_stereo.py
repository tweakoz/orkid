#!/usr/bin/env ork.python
################################################################################
# SPVR — THE SKY UNDER SINGLE-PASS STEREO (linux/NV, offscreen).
#
# THE DEFECT THIS CLOSES. The skybox fragment derives its view ray by
# unprojecting the fullscreen quad through inv_vp, and inv_vp arrives from the
# MONO per-draw provider. Under DMVR that is correct for free (one draw per eye,
# one matrix per draw). Under single-pass stereo it CANNOT be: one draw carries
# both views, and VR frusta are per-eye ASYMMETRIC, so no single matrix produces
# both eyes' rays. The peer authored here (FWD_SKYBOX_PROC_ST) unprojects through
# spvr_inv_vp[ofx_viewIndex] instead — a one-matrix substitution, everything
# downstream verbatim.
#
# WHY A VALIDATION-CLEAN RUN PROVES NOTHING HERE. A sky drawn with the wrong
# matrix renders, validates and captures perfectly. The only evidence is pixels,
# and it takes TWO measurements pointing in OPPOSITE directions:
#
#   the IDENTICAL-EYE trap   both eyes get one matrix -> the sky is the same
#                            image in both layers even when it must not be
#   the WRONG-MATRIX trap    the ray picks up the eye TRANSLATION -> the sky
#                            smears with head separation, which a sky at
#                            infinity must never do
#
# A test that only measures "the eyes agree" passes the first trap. A test that
# only measures "the eyes differ" passes the second. So the sky is measured in
# TWO camera configurations with the SAME metric and OPPOSITE required verdicts:
#
#   CONFIG T (translated eyes, identical orientation and projection)
#       correct sky = the two layers AGREE (a sky at infinity has no parallax)
#       correct finite geometry = the two layers DIFFER (leg 1's parallax)
#   CONFIG A (co-located eyes, per-eye YAW — the asymmetry a headset actually
#       hands the shader)
#       correct sky = the two layers DIVERGE, each matching ITS OWN eye's mono
#       reference; a shared matrix collapses them and fails
#
# LEGS (all must pass)
#   (a) SOURCE      FWD_SKYBOX_PROC_ST exists, names a fragment stage that reads
#                   spvr_inv_vp[ofx_viewIndex], its mono twin does NOT, and
#                   neither BRANCHES on the view index (index-select is the
#                   tile-hardware law). Also pins the probe shader below to the
#                   authored unprojection text.
#   (b) RESOLVE     FWD_SKYBOX_PROC_ST and FWD_SKYBOX_PROC both resolve non-null
#                   out of the real compiled orkshader://pbr — the JIT-compile
#                   proof for the authored fragment.
#   (c) GEOMETRY    (leg 1) CONFIG T, finite quads at four depths: each eye layer
#                   matches its OWN mono reference and the two layers DIFFER.
#   (d) SKY AGREE   (leg 2, direction one) CONFIG T, sky only: the two layers
#                   must AGREE. Catches the eye-translation smear.
#   (e) SKY DIVERGE (leg 2, direction two) CONFIG A, sky only: each layer matches
#                   ITS OWN eye's mono reference and the two layers DIFFER.
#                   Catches the one-matrix-for-both-eyes collapse.
#
# IN-RUN TEETH (every run, no environment)
#   T1  GEOMETRY driven with the MONO technique in the same stereo pass must
#       collapse the layers -- otherwise leg (c)'s difference is not the view index.
#   T2  SKY DIVERGE's cross-pairing (layer0 vs the OTHER eye's reference) must
#       land OUTSIDE the match bar the straight pairing lands inside, or the
#       match bar is satisfied by everything.
#   T3  SKY DIVERGE driven with the MONO technique must collapse the layers.
#   T4  AN INJECTED SMEAR. A third sky stage, identical except that it derives
#       the ray from the far point ALONE (dropping the near point, which is what
#       makes the ray translation-free), is rendered in CONFIG T. Its layers must
#       FAIL the agree bar leg (d) passes. Without this, leg (d) is a bar nobody
#       has shown can be broken.
#   T5  'duplicate named object' -- the shadlang collision signature -- must not
#       appear in the child's output. A duplicate technique name is only a log
#       line + first-definition-wins, so it is otherwise operationally silent.
#
# HONEST BOUND, stated rather than implied: legs (c)-(e) render a PROBE whose sky
# stage carries the shipping fragment's unprojection text verbatim (leg (a) pins
# that, and refuses to run if the two drift apart). The shipping skybox fragment
# cannot be driven below the compositor -- it wants the sky-view LUT, the cookie
# set and the atmosphere block. So: the shipping peer is proven to EXIST, COMPILE
# and carry the idiom (legs a+b); the idiom is proven to produce a per-view,
# translation-free sky on this device (legs c-e). Neither alone is the claim.
#
# Self-configuring: ORKID_VULKAN_VALIDATE=2 (continue mode -- =1 traps with no
# printed evidence, which would false-negative any validation grep). Default
# invocation needs no arguments and no environment.
#
#   ork.python test_spvr_sky_stereo.py [outdir]
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ.setdefault("ORKID_VULKAN_VALIDATE", "2")

import sys
import re
import math
import glob
import time
import json
import tempfile
import subprocess

sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "bin"))

SHADER_DIR = os.path.join(_ROOT, "ork.data", "platform_lev2", "shaders", "fxv2")

W = H = 256
ASPECT = float(W) / float(H)
NEAR, FAR, FOVY = 0.5, 60.0, 45.0

# CONFIG T -- eyes separated in X, both looking straight down -Z. Identical
# orientation AND identical projection, so the only difference between the two
# views is the eye TRANSLATION: finite geometry must parallax, the sky must not.
EYES_T = ((-0.35, 0.0, 6.0), (0.35, 0.0, 6.0))
# CONFIG A -- eyes CO-LOCATED and YAWED apart. No translation at all, so any
# difference between the layers can only have come from the per-view matrix.
EYE_A = (0.0, 0.0, 6.0)
YAW_A_DEG = 6.0

# world-space quads (center_x, center_y, half_extent, world_z), FAR to NEAR. The
# z spread is what makes leg (c)'s disparity depth-dependent rather than a
# uniform slide that a broken index-select could imitate.
QUADS = (
    (-0.90, 0.50, 2.20, -6.0),
    (1.10, -0.60, 1.60, -2.5),
    (-0.40, -0.20, 1.00, 0.5),
    (0.60, 0.90, 0.55, 2.5),
)
TINTS = (
    (1.00, 0.30, 0.30),
    (0.30, 1.00, 0.40),
    (0.40, 0.50, 1.00),
    (1.00, 0.95, 0.35),
)

# bars, in 0..1 luma.
MATCH_MEAN_CEIL = 2.0 / 255.0   # a layer vs its OWN reference: at most one code
DIFF_FRAC_FLOOR = 0.02          # a layer vs the OTHER view: this fraction must differ
DIFF_EPS = 2.0 / 255.0          # one 8-bit code either way is not a difference
RANGE_FLOOR = 0.05              # a flat/black capture must not be believed
# CONFIG T's sky: the two layers are computed from DIFFERENT matrices that
# describe the SAME ray field, so they agree to float noise rather than bitwise.
AGREE_MEAN_CEIL = 1.0 / 255.0
AGREE_FRAC_CEIL = 0.01

################################################################################
# THE AUTHORED IDIOM. Leg (c)-(e) are only evidence about the shipping peer while
# the probe renders the shipping peer's text. This block is required to appear,
# character for character, in BOTH pbrtools.i2 and the probe shader below.
################################################################################

AUTHORED_SKY_IDIOM = """  mat4 view_inv_vp = spvr_inv_vp[ofx_viewIndex];

  vec4 xyzw = vec4(uvn, 0, 1);
  xyzw      = view_inv_vp * xyzw;
  xyzw.xyz *= (1.0 / xyzw.w);
  vec3 posA = xyzw.xyz;
  xyzw      = vec4(uvn, 1, 1);
  xyzw      = view_inv_vp * xyzw;
  xyzw.xyz *= (1.0 / xyzw.w);
  vec3 posB = xyzw.xyz;"""

# ...and the clip idiom leg (c) shares with every other _ST vertex stage.
AUTHORED_CLIP_IDIOM = "(spvr_vp[ofx_viewIndex] * m) * position"

# the technique pair under test, in the real shader.
SKY_MONO_TEK = "FWD_SKYBOX_PROC"
SKY_STEREO_TEK = "FWD_SKYBOX_PROC_ST"

VIEW_READ = re.compile(r"spvr_(?:vp|inv_vp|eyepos)\s*\[\s*ofx_viewIndex\s*\]")

# the shadlang duplicate-declaration signature (T5).
DUP_SIGNATURE = "duplicate named object"


PROBE_SHADER = """
fxconfig fxcfg_default {
  glsl_version = "330";
}
uniform_block ub_skyprobe (descriptor_set 0) {
  mat4 mvp;
  mat4 m;
  mat4 sky_inv_vp;
  vec4 Tint;
}
uniform_block ublk_stereo (descriptor_set 0) {
  mat4 spvr_vp[2];
  mat4 spvr_inv_vp[2];
  vec4 spvr_eyepos[2];
}
vertex_interface vif_skyprobe {
  inputs {
    vec4 position : POSITION;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec2 frg_uv0;
    vec4 frg_clr;
  }
}
fragment_interface fif_skyprobe : vif_skyprobe {
  outputs {
    layout(location = 0) vec4 out_clr;
  }
}
vertex_shader vs_geo_mono : vif_skyprobe : ub_skyprobe {
  gl_Position = mvp * position;
  frg_uv0     = uv0;
  frg_clr     = position;
}
vertex_shader vs_geo_stereo : vif_skyprobe : ub_skyprobe : ublk_stereo {
  gl_Position = (spvr_vp[ofx_viewIndex] * m) * position;
  frg_uv0     = uv0;
  frg_clr     = position;
}
fragment_shader ps_geo : fif_skyprobe : ub_skyprobe {
  vec2 cell     = floor(frg_uv0 * 8.0);
  float checker = mod(cell.x + cell.y, 2.0);
  out_clr       = vec4(Tint.rgb * (0.30 + 0.70 * checker), 1.0);
}
vertex_shader vs_sky : vif_skyprobe : ub_skyprobe {
  gl_Position = position;
  frg_uv0     = uv0;
  frg_clr     = position;
}
fragment_shader ps_sky_mono : fif_skyprobe : ub_skyprobe {
  vec2 uvn = frg_clr.xy;

  mat4 view_inv_vp = sky_inv_vp;

  vec4 xyzw = vec4(uvn, 0, 1);
  xyzw      = view_inv_vp * xyzw;
  xyzw.xyz *= (1.0 / xyzw.w);
  vec3 posA = xyzw.xyz;
  xyzw      = vec4(uvn, 1, 1);
  xyzw      = view_inv_vp * xyzw;
  xyzw.xyz *= (1.0 / xyzw.w);
  vec3 posB = xyzw.xyz;
  vec3 VN   = normalize(posB - posA);
  out_clr   = vec4(0.5 + 0.5 * VN, 1.0);
}
fragment_shader ps_sky_stereo : fif_skyprobe : ub_skyprobe : ublk_stereo {
  vec2 uvn = frg_clr.xy;

  mat4 view_inv_vp = spvr_inv_vp[ofx_viewIndex];

  vec4 xyzw = vec4(uvn, 0, 1);
  xyzw      = view_inv_vp * xyzw;
  xyzw.xyz *= (1.0 / xyzw.w);
  vec3 posA = xyzw.xyz;
  xyzw      = vec4(uvn, 1, 1);
  xyzw      = view_inv_vp * xyzw;
  xyzw.xyz *= (1.0 / xyzw.w);
  vec3 posB = xyzw.xyz;
  vec3 VN   = normalize(posB - posA);
  out_clr   = vec4(0.5 + 0.5 * VN, 1.0);
}
fragment_shader ps_sky_drag : fif_skyprobe : ub_skyprobe : ublk_stereo {
  // T4's INJECTED DEFECT: the unprojected NEAR point used as a direction. Taking
  // one point instead of the difference of two is what lets the eye POSITION into
  // the ray -- the exact smear leg (d) forbids. (The far point is the same bug,
  // but at this near/far ratio its smear lands inside one 8-bit code, which would
  // make the tooth prove only that the bug can be small.)
  vec2 uvn = frg_clr.xy;
  mat4 view_inv_vp = spvr_inv_vp[ofx_viewIndex];
  vec4 xyzw = vec4(uvn, 0, 1);
  xyzw      = view_inv_vp * xyzw;
  xyzw.xyz *= (1.0 / xyzw.w);
  vec3 VN   = normalize(xyzw.xyz);
  out_clr   = vec4(0.5 + 0.5 * VN, 1.0);
}
state_block sb_geo : default {
  BlendMode = OFF;
  DepthTest = LEQUALS;
  CullTest  = OFF;
}
state_block sb_sky : default {
  BlendMode = OFF;
  DepthTest = OFF;
  CullTest  = OFF;
}
technique GEO_MO {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_geo_mono;
    fragment_shader = ps_geo;
    state_block     = sb_geo;
  }
}
technique GEO_ST {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_geo_stereo;
    fragment_shader = ps_geo;
    state_block     = sb_geo;
  }
}
technique SKY_MO {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_sky;
    fragment_shader = ps_sky_mono;
    state_block     = sb_sky;
  }
}
technique SKY_ST {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_sky;
    fragment_shader = ps_sky_stereo;
    state_block     = sb_sky;
  }
}
technique SKY_ST_DRAG {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_sky;
    fragment_shader = ps_sky_drag;
    state_block     = sb_sky;
  }
}
"""


################################################################################
# LEG (a) -- SOURCE. Pure text; no GPU, no engine.
################################################################################


def _read_shader_sources():
  rval = {}
  for path in sorted(glob.glob(os.path.join(SHADER_DIR, "*.fxv2")) +
                     glob.glob(os.path.join(SHADER_DIR, "*.i2"))):
    rval[os.path.basename(path)] = open(path).read()
  return rval


_TEK_RE = re.compile(r"technique\s+([A-Za-z0-9_]+)\s*\{(.*?)\n\}", re.S)
_STAGE_RE = re.compile(r"(?:vertex_shader|fragment_shader)\s*=\s*([A-Za-z0-9_]+)")
_VFPASS_RE = re.compile(r"vf_pass\s*=\s*\{([^}]*)\}")
_STAGE_DEF_RE = re.compile(
    r"^(?:vertex_shader|fragment_shader)\s+([A-Za-z0-9_]+)(.*?)^\}", re.S | re.M)


def _techniques(sources):
  rval = {}
  for _fname, text in sources.items():
    for m in _TEK_RE.finditer(text):
      name, body = m.group(1), m.group(2)
      stages = list(_STAGE_RE.findall(body))
      for vf in _VFPASS_RE.findall(body):
        stages += [t.strip() for t in vf.split(",") if t.strip()]
      rval[name] = stages
  return rval


def _stage_bodies(sources):
  rval = {}
  for _fname, text in sources.items():
    for m in _STAGE_DEF_RE.finditer(text):
      rval[m.group(1)] = m.group(2)
  return rval


def leg_source():
  sources = _read_shader_sources()
  teks = _techniques(sources)
  bodies = _stage_bodies(sources)
  fails = []

  # the probe must still be rendering what the engine ships.
  if AUTHORED_SKY_IDIOM not in PROBE_SHADER:
    fails.append("the probe shader no longer carries the authored sky unprojection")
  if AUTHORED_SKY_IDIOM not in sources.get("pbrtools.i2", ""):
    fails.append("pbrtools.i2 no longer carries the authored sky unprojection -- the "
                 "GPU legs would be measuring a form the shipping peer has moved off of")
  if AUTHORED_CLIP_IDIOM not in PROBE_SHADER:
    fails.append("the probe shader no longer carries the authored per-view clip idiom")
  if AUTHORED_CLIP_IDIOM not in sources.get("pbrtools.i2", ""):
    fails.append("pbrtools.i2 no longer carries the authored per-view clip idiom")

  if SKY_STEREO_TEK not in teks:
    fails.append("%s ABSENT -- the sky falls back to the mono technique under "
                 "single-pass stereo (one matrix, two views)" % SKY_STEREO_TEK)
  elif SKY_MONO_TEK not in teks:
    fails.append("%s absent -- an _ST with no mono twin is not a peer" % SKY_MONO_TEK)
  else:
    st_stages = teks[SKY_STEREO_TEK]
    mo_stages = teks[SKY_MONO_TEK]
    if not any(VIEW_READ.search(bodies.get(s, "")) for s in st_stages):
      fails.append("%s names no stage that reads spvr_inv_vp[ofx_viewIndex] -- both "
                   "eye layers would get the same sky" % SKY_STEREO_TEK)
    if any(VIEW_READ.search(bodies.get(s, "")) for s in mo_stages):
      fails.append("%s (the MONO peer) performs a per-view read" % SKY_MONO_TEK)
    for s in st_stages:
      if re.search(r"(?:if|switch)\s*\([^)]*ofx_viewIndex", bodies.get(s, "")):
        fails.append("%s: stage %s BRANCHES on ofx_viewIndex (index-select only)"
                     % (SKY_STEREO_TEK, s))

  # THE _ST NAMING CONTRACT. Per-view technique names are looked up as strings by
  # C++ (<mono> + "_ST") and by every peer table, so the suffix is an interface,
  # not a style preference: a technique that does the per-view read under some
  # other name is invisible to the code that wants it and the pass silently takes
  # the mono twin. Scoped by BEHAVIOUR rather than by an exemption list -- the
  # legacy deferred *_stereo techniques predate ofx_viewIndex and do not read it,
  # so they are outside this rule by construction rather than by permission.
  # The token may be followed by a shader-suffix or variant tail (the C++ lookup is
  # "<base>_ST" + _shader_suffix, e.g. FWD_CT_NM_RI_NI_ST_V4, FWD_CV_..._ST_ALPHA),
  # so what is pinned is the TOKEN, not the string ending.
  st_token = re.compile(r"_ST(?:_|$)")
  for name, stages in sorted(teks.items()):
    if any(VIEW_READ.search(bodies.get(s, "")) for s in stages):
      if not st_token.search(name):
        fails.append("technique %s performs a per-view read but carries no _ST token "
                     "-- the name IS the lookup key" % name)
  return fails


################################################################################
# CHILD: legs (b) through (e). Runs in its own process.
################################################################################


def _render(outdir):
  from orkengine import core
  from orkengine import lev2
  from ork.testing import headless_app, ensure_parent_dir
  import numpy
  from PIL import Image

  tokens = core.CrcStringProxy()
  results = {"resolved": {}, "unresolved": []}

  def _yawed_target(eye, yaw_deg):
    a = math.radians(yaw_deg)
    return core.vec3(eye[0] + math.sin(a), eye[1], eye[2] - math.cos(a))

  def _camera(eye, tgt):
    cam = lev2.CameraData()
    cam.perspective(NEAR, FAR, FOVY)
    cam.lookAt(core.vec3(eye[0], eye[1], eye[2]), tgt, core.vec3(0, 1, 0))
    return cam

  def _cammtx(cam):
    cm = lev2.CameraMatrices()
    cm.setCustomView(cam.vMatrix())
    cm.setCustomProjection(cam.pMatrix(ASPECT))
    return cm

  def _write_png(capbuf, path):
    arr = numpy.array(capbuf, dtype=numpy.uint8).reshape(capbuf.height, capbuf.width, 4)
    ensure_parent_dir(path)
    Image.fromarray(arr[..., :3]).save(path)
    return path

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx = app.ctx
    print("sky_stereo: supports_multiview=%s max_views=%d"
          % (ctx.supports_multiview, ctx.max_multiview_views), flush=True)

    ############################################################
    # LEG (b) -- RESOLVE, out of the REAL shader. Also the JIT-compile proof for
    # the authored fragment: a peer whose text does not compile never gets here.
    ############################################################
    pbr = lev2.FreestyleMaterial()
    pbr.gpuInit(ctx, "orkshader://pbr")
    for name in (SKY_MONO_TEK, SKY_STEREO_TEK):
      tek = pbr.technique(name)
      results["resolved"][name] = bool(tek)
      if not tek:
        results["unresolved"].append("%s did not resolve out of orkshader://pbr" % name)
    print("sky_stereo: resolved %s" % results["resolved"], flush=True)

    ############################################################
    # LEGS (c)(d)(e) -- the probe.
    ############################################################
    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(ctx, "spvr_skyprobe", PROBE_SHADER)
    tek = {n: mtl.technique(n) for n in
           ("GEO_MO", "GEO_ST", "SKY_MO", "SKY_ST", "SKY_ST_DRAG")}
    for n, t in tek.items():
      assert t, "probe technique %s absent" % n

    par_m = mtl.param("m")
    par_mvp = mtl.param("mvp")
    par_ivp = mtl.param("sky_inv_vp")
    par_tint = mtl.param("Tint")
    identity = core.mtx4()

    camT = [_camera(EYES_T[0], core.vec3(EYES_T[0][0], EYES_T[0][1], 0.0)),
            _camera(EYES_T[1], core.vec3(EYES_T[1][0], EYES_T[1][1], 0.0))]
    camA = [_camera(EYE_A, _yawed_target(EYE_A, -YAW_A_DEG)),
            _camera(EYE_A, _yawed_target(EYE_A, +YAW_A_DEG))]
    vpT = [c.vpMatrix(ASPECT) for c in camT]
    vpA = [c.vpMatrix(ASPECT) for c in camA]
    cmT = [_cammtx(c) for c in camT]
    cmA = [_cammtx(c) for c in camA]

    def _make_rtg(name, layers):
      rtg = lev2.RtGroup(ctx, W, H)
      rtg.name = name
      if layers > 1:
        # both BEFORE the first createBuffer: the layer count is copied into
        # each RtBuffer at construction.
        rtg.numLayers = layers
        rtg.multiview = True
      rtb = rtg.createBuffer(tokens.RGBA8, tokens.color)
      rtb.clearColor = core.vec4(0, 0, 0, 1)
      rtg.createDepthBuffer(tokens.Z32F, False)
      return rtg

    def _draw_geo(rcfd, mvp, stereo=None, force_mono=False):
      for (ci, (cx, cy, half, z)) in enumerate(QUADS):
        if force_mono:
          mtl.begin(tek["GEO_MO"], rcfd)
        else:
          mtl.begin(tek["GEO_MO"], tek["GEO_ST"], rcfd)
        if (stereo is not None) and (not force_mono):
          mtl.publishStereoBlock(rcfd, stereo[0], stereo[1])
        if par_mvp:
          mtl.bindParamMatrix4(par_mvp, mvp)
        if par_m:
          mtl.bindParamMatrix4(par_m, identity)
        if par_tint:
          mtl.bindParamVec4(par_tint, core.vec4(TINTS[ci][0], TINTS[ci][1], TINTS[ci][2], 1.0))
        ctx.DWI.quad2D(core.vec4(cx - half, cy - half, half * 2.0, half * 2.0),
                       core.vec4(0, 0, 1, 1), core.vec4(0, 0, 0, 0), z)
        mtl.end(rcfd)

    def _draw_sky(rcfd, ivp, stereo=None, mode="stereo"):
      if mode == "mono":
        mtl.begin(tek["SKY_MO"], rcfd)
      elif mode == "drag":
        mtl.begin(tek["SKY_MO"], tek["SKY_ST_DRAG"], rcfd)
      else:
        mtl.begin(tek["SKY_MO"], tek["SKY_ST"], rcfd)
      if (stereo is not None) and (mode != "mono"):
        mtl.publishStereoBlock(rcfd, stereo[0], stereo[1])
      if par_ivp:
        mtl.bindParamMatrix4(par_ivp, ivp)
      if par_m:
        mtl.bindParamMatrix4(par_m, identity)
      if par_mvp:
        mtl.bindParamMatrix4(par_mvp, identity)
      if par_tint:
        mtl.bindParamVec4(par_tint, core.vec4(1, 1, 1, 1))
      # the FULLSCREEN NDC quad: vs_sky passes position straight through, exactly
      # as the shipping skybox vertex stage does.
      ctx.DWI.quad2D(core.vec4(-1, -1, 2, 2),
                     core.vec4(0, 0, 1, 1), core.vec4(0, 0, 0, 0), 0.5)
      mtl.end(rcfd)

    rtgs = {
        "geoT_mono0": _make_rtg("SkyGeoMono0", 1),
        "geoT_mono1": _make_rtg("SkyGeoMono1", 1),
        "geoT_st": _make_rtg("SkyGeoSt", 2),
        "geoT_tooth": _make_rtg("SkyGeoTooth", 2),
        "skyT_st": _make_rtg("SkySkyTSt", 2),
        "skyT_drag": _make_rtg("SkySkyTDrag", 2),
        "skyA_mono0": _make_rtg("SkySkyAMono0", 1),
        "skyA_mono1": _make_rtg("SkySkyAMono1", 1),
        "skyA_st": _make_rtg("SkySkyASt", 2),
        "skyA_tooth": _make_rtg("SkySkyATooth", 2),
    }
    print("sky_stereo: stereo rtg viewMask=0x%x numLayers=%d"
          % (rtgs["geoT_st"].viewMask, rtgs["geoT_st"].numLayers), flush=True)

    def _mono_pass(rtg, body):
      rcfd = lev2.RenderContextFrameData(ctx)
      ctx.FBI.rtGroupPush(rtg)
      ctx.FBI.rtGroupClear(rtg)
      body(rcfd)
      ctx.FBI.rtGroupPop()

    def _stereo_pass(rtg, body):
      rcfd = lev2.RenderContextFrameData(ctx)
      cimpl = lev2.CompositingImpl(lev2.CompositingData())
      rcfd.pushCompositor(cimpl)
      cpd = lev2.CompositingPassData()
      cpd.setSinglePassStereo(True)
      cimpl.pushCPD(cpd)
      ctx.FBI.rtGroupPush(rtg)
      ctx.FBI.rtGroupClear(rtg)
      body(rcfd)
      ctx.FBI.rtGroupPop()
      cimpl.popCPD()
      rcfd.popCompositor()

    caps = {}

    def _capture_frame(keys):
      """ONE FRAME, and everything in it read back before the next begins.
      ublk_stereo is a SINGLE shared buffer written host-side during command
      RECORDING, so every stereo draw recorded in a frame ends up reading the
      LAST view state published in that frame. The two camera configurations
      here therefore cannot share a frame -- measured: config T's passes came
      back rendered with config A's matrices. Production never sees this (one
      frame is one head pose), but a test that varies the pose must not."""
      plan = []
      for key in keys:
        rtg = rtgs[key]
        if rtg.numLayers > 1:
          plan += [(key + "_0", rtg, 0), (key + "_1", rtg, 1)]
        else:
          plan += [(key, rtg, 0)]
      futures = []
      for (name, rtg, layer) in plan:
        cb = lev2.CaptureBuffer()
        cb.capture_layer = layer
        caps[name] = cb
        futures.append(ctx.FBI.captureAsFormat(rtg.buffer(0), cb, "RGBA8"))
      ctx.endFrame()
      for (name, _rtg, _layer), fut in zip(plan, futures):
        ok = fut.wait(caps[name])
        assert ok, "sky_stereo: capture never landed for %s" % name

    ############################################################
    # FRAME 1 -- CONFIG T: legs (c) and (d), plus teeth T1 and T4.
    ############################################################
    ctx.beginFrame()
    for i in (0, 1):
      _mono_pass(rtgs["geoT_mono%d" % i],
                 lambda rcfd, i=i: _draw_geo(rcfd, vpT[i]))
    _stereo_pass(rtgs["geoT_st"],
                 lambda rcfd: _draw_geo(rcfd, vpT[0], stereo=cmT))
    _stereo_pass(rtgs["geoT_tooth"],   # T1
                 lambda rcfd: _draw_geo(rcfd, vpT[0], stereo=cmT, force_mono=True))
    _stereo_pass(rtgs["skyT_st"],
                 lambda rcfd: _draw_sky(rcfd, vpT[0].inverse, stereo=cmT))
    _stereo_pass(rtgs["skyT_drag"],    # T4
                 lambda rcfd: _draw_sky(rcfd, vpT[0].inverse, stereo=cmT, mode="drag"))
    _capture_frame(["geoT_mono0", "geoT_mono1", "geoT_st", "geoT_tooth",
                    "skyT_st", "skyT_drag"])

    ############################################################
    # FRAME 2 -- CONFIG A: leg (e), plus teeth T2 and T3.
    ############################################################
    ctx.beginFrame()
    for i in (0, 1):
      _mono_pass(rtgs["skyA_mono%d" % i],
                 lambda rcfd, i=i: _draw_sky(rcfd, vpA[i].inverse, mode="mono"))
    _stereo_pass(rtgs["skyA_st"],
                 lambda rcfd: _draw_sky(rcfd, vpA[0].inverse, stereo=cmA))
    _stereo_pass(rtgs["skyA_tooth"],   # T3
                 lambda rcfd: _draw_sky(rcfd, vpA[0].inverse, mode="mono"))
    _capture_frame(["skyA_mono0", "skyA_mono1", "skyA_st", "skyA_tooth"])

    for name in caps:
      _write_png(caps[name], os.path.join(outdir, name + ".png"))

    results["validation_armed"] = bool(ctx.validation_armed)
    results["validation_errors"] = int(ctx.validation_errors)

  with open(os.path.join(outdir, "child.json"), "w") as f:
    json.dump(results, f, indent=1)
  return 0


################################################################################
# DRIVER
################################################################################


def _luma(path):
  import numpy
  from PIL import Image
  a = numpy.asarray(Image.open(path).convert("RGB"), dtype=numpy.float64) / 255.0
  return 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]


def _metrics(a, b):
  import numpy
  d = numpy.abs(a - b)
  return float(d.mean()), float((d > DIFF_EPS).mean())


def main():
  outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
      tempfile.gettempdir(), "spvr_sky_stereo")
  os.makedirs(outdir, exist_ok=True)
  t0 = time.time()
  fails = []

  # ---- leg (a)
  fail_a = leg_source()
  print("LEG_SOURCE failures=%d" % len(fail_a), flush=True)
  for f in fail_a:
    print("  LEG_SOURCE FAIL: %s" % f, flush=True)
  fails += fail_a

  # ---- child
  env = dict(os.environ)
  env["ORKID_VULKAN_VALIDATE"] = "2"
  argv = [sys.executable, os.path.abspath(__file__), "--render", outdir]
  proc = subprocess.run(argv, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=600)
  out = proc.stdout.decode("utf-8", "replace")
  print(out, flush=True)
  if proc.returncode != 0:
    fails.append("child render process exited rc=%d" % proc.returncode)

  # ---- T5: the shadlang duplicate-declaration signature is otherwise silent.
  n_dup = out.lower().count(DUP_SIGNATURE)
  print("TOOTH_T5 duplicate_named_object=%d" % n_dup, flush=True)
  if n_dup:
    fails.append("TOOTH T5: shader compilation reported %d '%s' -- a duplicate "
                 "technique/stage name is first-definition-wins and otherwise silent"
                 % (n_dup, DUP_SIGNATURE))

  child = {}
  cjson = os.path.join(outdir, "child.json")
  if os.path.exists(cjson):
    child = json.load(open(cjson))

  # ---- leg (b)
  unresolved = child.get("unresolved", [])
  print("LEG_RESOLVE resolved=%s failures=%d"
        % (child.get("resolved", {}), len(unresolved)), flush=True)
  for f in unresolved:
    print("  LEG_RESOLVE FAIL: %s" % f, flush=True)
  fails += unresolved
  if not child.get("resolved"):
    fails.append("LEG_RESOLVE produced no results at all")

  # ---- image legs
  def L(name):
    return _luma(os.path.join(outdir, name + ".png"))

  try:
    img = {k: L(k) for k in (
        "geoT_mono0", "geoT_mono1", "geoT_st_0", "geoT_st_1",
        "geoT_tooth_0", "geoT_tooth_1",
        "skyT_st_0", "skyT_st_1", "skyT_drag_0", "skyT_drag_1",
        "skyA_mono0", "skyA_mono1", "skyA_st_0", "skyA_st_1",
        "skyA_tooth_0", "skyA_tooth_1")}
  except Exception as ex:
    fails.append("captures unreadable (%s)" % ex)
    img = None

  if img is not None:
    for nm, im in img.items():
      rng = float(im.max() - im.min())
      if rng < RANGE_FLOOR:
        fails.append("%s is degenerate (range %.4f < %.4f) -- a flat capture "
                     "matches any other flat capture" % (nm, rng, RANGE_FLOOR))

    # ---- leg (c): GEOMETRY parallax, CONFIG T
    g_self0, _ = _metrics(img["geoT_st_0"], img["geoT_mono0"])
    g_self1, _ = _metrics(img["geoT_st_1"], img["geoT_mono1"])
    g_cross, _ = _metrics(img["geoT_st_0"], img["geoT_mono1"])
    _, g_views = _metrics(img["geoT_st_0"], img["geoT_st_1"])
    _, g_tooth = _metrics(img["geoT_tooth_0"], img["geoT_tooth_1"])
    print("LEG_GEOMETRY self0=%.5f self1=%.5f cross=%.5f viewdiff=%.4f toothdiff=%.4f"
          % (g_self0, g_self1, g_cross, g_views, g_tooth), flush=True)
    if g_self0 > MATCH_MEAN_CEIL:
      fails.append("LEG_GEOMETRY: layer0 does not match its own mono reference "
                   "(%.5f > %.5f)" % (g_self0, MATCH_MEAN_CEIL))
    if g_self1 > MATCH_MEAN_CEIL:
      fails.append("LEG_GEOMETRY: layer1 does not match its own mono reference "
                   "(%.5f > %.5f)" % (g_self1, MATCH_MEAN_CEIL))
    if g_views < DIFF_FRAC_FLOOR:
      fails.append("LEG_GEOMETRY: the two layers are the SAME image (%.4f < %.4f) "
                   "-- zero parallax on finite geometry" % (g_views, DIFF_FRAC_FLOOR))
    if g_cross <= MATCH_MEAN_CEIL:
      fails.append("LEG_GEOMETRY: cross-pairing matched (%.5f) -- the metric cannot "
                   "tell the two views apart" % g_cross)
    if g_tooth >= DIFF_FRAC_FLOOR:
      fails.append("TOOTH T1: the MONO technique produced DIFFERENT layers (%.4f) "
                   "-- leg (c)'s difference is not the view index" % g_tooth)

    # ---- leg (d): SKY AGREE, CONFIG T (+ T4)
    s_mean, s_frac = _metrics(img["skyT_st_0"], img["skyT_st_1"])
    d_mean, d_frac = _metrics(img["skyT_drag_0"], img["skyT_drag_1"])
    print("LEG_SKY_AGREE mean=%.6f frac=%.5f | TOOTH_T4 dragmean=%.6f dragfrac=%.5f"
          % (s_mean, s_frac, d_mean, d_frac), flush=True)
    if s_mean > AGREE_MEAN_CEIL or s_frac > AGREE_FRAC_CEIL:
      fails.append("LEG_SKY_AGREE: the sky DIFFERS between eyes under a pure eye "
                   "TRANSLATION (mean %.6f > %.6f or frac %.5f > %.5f) -- the ray is "
                   "picking up the eye position; a sky at infinity must not"
                   % (s_mean, AGREE_MEAN_CEIL, s_frac, AGREE_FRAC_CEIL))
    if d_frac < DIFF_FRAC_FLOOR:
      fails.append("TOOTH T4: the injected translation-dragged sky did NOT break the "
                   "agree bar (frac %.5f) -- leg (d) is a bar nothing can fail" % d_frac)

    # ---- leg (e): SKY DIVERGE, CONFIG A (+ T2, T3)
    a_self0, _ = _metrics(img["skyA_st_0"], img["skyA_mono0"])
    a_self1, _ = _metrics(img["skyA_st_1"], img["skyA_mono1"])
    a_cross, _ = _metrics(img["skyA_st_0"], img["skyA_mono1"])
    _, a_views = _metrics(img["skyA_st_0"], img["skyA_st_1"])
    _, a_tooth = _metrics(img["skyA_tooth_0"], img["skyA_tooth_1"])
    print("LEG_SKY_DIVERGE self0=%.5f self1=%.5f cross=%.5f viewdiff=%.4f toothdiff=%.4f"
          % (a_self0, a_self1, a_cross, a_views, a_tooth), flush=True)
    if a_self0 > MATCH_MEAN_CEIL:
      fails.append("LEG_SKY_DIVERGE: layer0's sky does not match its own eye's "
                   "reference (%.5f > %.5f)" % (a_self0, MATCH_MEAN_CEIL))
    if a_self1 > MATCH_MEAN_CEIL:
      fails.append("LEG_SKY_DIVERGE: layer1's sky does not match its own eye's "
                   "reference (%.5f > %.5f) -- the classic wrong-view-index result"
                   % (a_self1, MATCH_MEAN_CEIL))
    if a_views < DIFF_FRAC_FLOOR:
      fails.append("LEG_SKY_DIVERGE: the two layers carry the SAME sky (%.4f < %.4f) "
                   "-- one matrix served both views" % (a_views, DIFF_FRAC_FLOOR))
    if a_cross <= MATCH_MEAN_CEIL:
      fails.append("TOOTH T2: cross-pairing matched (%.5f <= %.5f) -- the eyes are "
                   "indistinguishable and LEG_SKY_DIVERGE proves nothing"
                   % (a_cross, MATCH_MEAN_CEIL))
    if a_tooth >= AGREE_FRAC_CEIL:
      fails.append("TOOTH T3: the MONO sky technique produced DIFFERENT layers "
                   "(%.4f) -- leg (e)'s difference is not the view index" % a_tooth)

  # ---- validation (a warning is a failure)
  armed = child.get("validation_armed", False)
  verrs = int(child.get("validation_errors", -1))
  print("VALIDATION armed=%s errors=%d" % (armed, verrs), flush=True)
  if not armed:
    fails.append("validation layer was not armed -- a zero error count proves nothing")
  elif verrs != 0:
    fails.append("validation reported %d error(s)" % verrs)

  dt = time.time() - t0
  if fails:
    print("TESTVERDICT FAIL (%d): %s" % (len(fails), "; ".join(fails[:6])), flush=True)
    print("test_spvr_sky_stereo: FAIL in %.1fs" % dt, flush=True)
    return 1
  print("TESTVERDICT PASS -- %s authored, compiled and per-view; finite geometry "
        "parallax-positive; the sky AGREES across a pure eye translation and "
        "DIVERGES per-eye under yawed views, with all five teeth green (%.1fs)"
        % (SKY_STEREO_TEK, dt), flush=True)
  return 0


if __name__ == "__main__":
  if len(sys.argv) > 1 and sys.argv[1] == "--render":
    sys.exit(_render(sys.argv[2]))
  sys.exit(main())
