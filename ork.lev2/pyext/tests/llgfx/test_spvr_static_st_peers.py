#!/usr/bin/env ork.python
################################################################################
# SPVR — the STATIC _ST TECHNIQUE PEERS (linux/NV, offscreen).
#
# THE QUESTION: for every shader family that can be drawn in a single-pass-stereo
# pass, does a per-view technique EXIST, COMPILE, and actually READ the view
# index — or does the pass quietly fall back to the mono technique?
#
# WHY THIS TEST EXISTS. A missing _ST peer is not a crash and not a validation
# error. The stereo pass takes the mono technique, both eye layers receive the
# SAME image, and the result renders clean, validates clean, and passes any
# oracle that looks at one frame at a time. Zero parallax is the failure mode,
# so every leg below is built to see it.
#
# LEGS (all must pass)
#   (a) SOURCE      every authored _ST technique has a mono peer, and the stage
#                   it names reads spvr_vp / spvr_inv_vp indexed by ofx_viewIndex
#   (b) RESOLVE     every expected _ST technique resolves non-null out of the
#                   real compiled shader (pbr / terrain / particle) -- this is
#                   also the JIT-compile proof for the authored text
#   (c) EMITTED     each _ST stage's emitted GLSL contains gl_ViewIndex and its
#                   _MO twin's does NOT. Leg (a) reads what was authored; this
#                   reads what the compiler actually produced, which is what the
#                   GPU runs. A peer that compiled down to its mono twin passes
#                   (a) and fails here.
#   (d) PARALLAX    the authored per-view clip idiom, rendered into a 2-layer
#                   multiview target through the production ublk_stereo writer,
#                   produces two DIFFERENT layers, each matching its OWN mono
#                   reference, with the cross-pairing NOT matching. This is the
#                   only leg that MEASURES parallax rather than inferring it;
#                   see the PROBE_SHADER note for its exact scope.
#   (e) EXCLUSION   every asset the GPU legs skip must still be unloadable
#
# IN-RUN TEETH (every run, no environment)
#   T1  the same stereo pass driven with the MONO technique must produce two
#       IDENTICAL layers -- if it does not, leg (d)'s difference metric is
#       measuring something other than the view index and (d) proves nothing.
#   T2  the cross-pairing (layer0 vs mono_ref_1) must land OUTSIDE the match bar
#       that the straight pairing lands inside; two views that a metric cannot
#       tell apart would make leg (d) vacuous.
#
# HONEST BOUNDS, stated rather than implied:
#   - Leg (d) renders the IDIOM, not each peer. No shipping _ST technique can be
#     driven below the compositor (their fragments want the forward lighting
#     chain; the depth pair writes only depth), and terrain/particle peers have
#     no per-view producer at all yet -- that ublk_stereo bind is C++ arriving
#     with the SPVR node. So per-peer parallax is proven structurally, by (a)
#     and (c), and the idiom those peers use is proven to parallax by (d). Do
#     not read (c) as a rendered parallax proof for any individual peer.
#   - Leg (c) reads the compiler's GLSL dumps, which land in $OBT_STAGE/tempdir
#     on every compile. A warm shader cache serves compiled blobs and writes no
#     dumps, so the leg self-arms with ORKID_DISABLE_SHADER_CACHE=1 in the child
#     and SKIPs loudly (never silently) if the dumps are absent anyway.
#   - terrain.fxv2 DOES NOT PARSE on this base, and did not before this test
#     existed: `struct` inside a libblock (lib_terrain_vtx, the TerOut decl) is
#     not accepted by the fxv2 front end, and ps_terrain additionally references
#     an undefined `ddd1`. Verified differentially -- the pristine file fails the
#     identical parse error at the identical construct. So the terrain _ST peer
#     is covered by leg (a) only, and leg (e) below PINS that exclusion: it
#     asserts terrain still fails to load, so the day someone repairs the file
#     this test fails and forces terrain back into the GPU legs. An exclusion
#     that cannot expire is how coverage rots.
#
# Self-configuring: ORKID_VULKAN_VALIDATE=2 (continue mode -- =1 traps with no
# printed evidence, which would false-negative any validation grep). Default
# invocation needs no arguments and no environment.
#
#   ork.python test_spvr_static_st_peers.py [outdir]
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ.setdefault("ORKID_VULKAN_VALIDATE", "2")

import sys
import re
import glob
import time
import json
import shutil
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

# eyes: distinct positions AND distinct heights, both looking at the origin, so
# the disparity is depth-dependent rather than a uniform slide that a broken
# index-select could imitate.
EYES = ((-0.75, 0.20, 6.0), (0.75, -0.20, 6.0))

# world-space quads (center_x, center_y, half_extent, world_z), FAR to NEAR.
# The z spread is what makes the disparity depth-dependent.
QUADS = (
    (-0.90, 0.50, 2.20, -6.0),
    (1.10, -0.60, 1.60, -2.5),
    (-0.40, -0.20, 1.00, 0.5),
    (0.60, 0.90, 0.55, 2.5),
)
# per-quad tint, so a layer that mixed up its draw order is visible rather than
# averaging out into a plausible-looking grey.
TINTS = (
    (1.00, 0.30, 0.30),
    (0.30, 1.00, 0.40),
    (0.40, 0.50, 1.00),
    (1.00, 0.95, 0.35),
)

# leg (d) bars, in 0..1 luma.
MATCH_MEAN_CEIL = 2.0 / 255.0   # a layer vs its OWN reference: at most one code
DIFF_FRAC_FLOOR = 0.02          # a layer vs the OTHER view: this fraction must differ
DIFF_EPS = 2.0 / 255.0          # one 8-bit code either way is not a difference
RANGE_FLOOR = 0.05              # a flat/black capture must not be believed

################################################################################
# THE EXPECTED PEER SET.
#
# Each entry is (shader asset, mono technique, stereo technique). A name that is
# in this table and NOT in the shader tree fails leg (a): the table is the
# durable record of which families are supposed to have a per-view path, so a
# family that regresses to mono-only cannot do it quietly.
#
# The GBU_* (deferred) family is deliberately absent: the deferred path is not
# in the single-pass-stereo program.
################################################################################

PEERS = (
    # pbr.fxv2 -- forward main view
    ("pbr", "FWD_CT_NM_RI_NI_MO", "FWD_CT_NM_RI_NI_ST"),
    ("pbr", "FWD_CT_NM_RI_IN_MO", "FWD_CT_NM_RI_IN_ST"),
    ("pbr", "FWD_CT_NM_SK_NI_MO", "FWD_CT_NM_SK_NI_ST"),
    ("pbr", "FWD_CT_NM_IM_NI_MO", "FWD_CT_NM_IM_NI_ST"),
    # ...vertex-color variants
    ("pbr", "FWD_CV_NM_RI_NI_MO", "FWD_CV_NM_RI_NI_ST"),
    ("pbr", "FWD_CV_NM_RI_NI_MO_ALPHA", "FWD_CV_NM_RI_NI_ST_ALPHA"),
    ("pbr", "FWD_CV_NM_RI_IN_MO", "FWD_CV_NM_RI_IN_ST"),
    ("pbr", "FWD_CV_NM_RI_IN_MO_ALPHA", "FWD_CV_NM_RI_IN_ST_ALPHA"),
    # ...depth prepass, plain and masked
    ("pbr", "FWD_DEPTHPREPASS_RI_NI_MO", "FWD_DEPTHPREPASS_RI_NI_ST"),
    ("pbr", "FWD_DEPTHPREPASS_SK_NI_MO", "FWD_DEPTHPREPASS_SK_NI_ST"),
    ("pbr", "FWD_DEPTHPREPASS_RI_IN_MO", "FWD_DEPTHPREPASS_RI_IN_ST"),
    ("pbr", "FWD_DEPTHPREPASS_MASKED_RI_NI_MO", "FWD_DEPTHPREPASS_MASKED_RI_NI_ST"),
    ("pbr", "FWD_DEPTHPREPASS_MASKED_SK_NI_MO", "FWD_DEPTHPREPASS_MASKED_SK_NI_ST"),
    ("pbr", "FWD_DEPTHPREPASS_MASKED_RI_IN_MO", "FWD_DEPTHPREPASS_MASKED_RI_IN_ST"),
    # ...skybox: the ONE peer whose per-view state is in the fragment stage
    ("pbr", "FWD_SKYBOX_MO", "FWD_SKYBOX_ST"),
    # terrain classic (A2 site ii)
    ("terrain", "terrain", "terrain_ST"),
    ("terrain", "terrain_gbuf1", "terrain_gbuf1_ST"),
    # particles (A2 site iii -- the silent one)
    ("particle", "tflatparticle_sprites", "tflatparticle_sprites_ST"),
    ("particle", "tgradparticle_sprites", "tgradparticle_sprites_ST"),
    ("particle", "tgradatlasparticle_sprites", "tgradatlasparticle_sprites_ST"),
    ("particle", "ttexparticle_sprites", "ttexparticle_sprites_ST"),
    ("particle", "ttexgridparticle_sprites", "ttexgridparticle_sprites_ST"),
    ("particle", "tfreestyleparticle_sprites", "tfreestyleparticle_sprites_ST"),
    ("particle", "tflatparticle_streaks", "tflatparticle_streaks_ST"),
    ("particle", "tgradparticle_streaks", "tgradparticle_streaks_ST"),
    ("particle", "tgradatlasparticle_streaks", "tgradatlasparticle_streaks_ST"),
    ("particle", "ttexparticle_streaks", "ttexparticle_streaks_ST"),
    ("particle", "tfreestyleparticle_streaks", "tfreestyleparticle_streaks_ST"),
)

# the per-view read every _ST stage must perform. Index-select only: a branch on
# the view index is a rasterization hazard on tile hardware, so the form is
# pinned here rather than left to the author.
VIEW_READ = re.compile(r"spvr_(?:vp|inv_vp|eyepos)\s*\[\s*ofx_viewIndex\s*\]")

# Assets the GPU legs cannot load, with the reason. Leg (e) asserts each one is
# STILL unloadable, so the exclusion expires by failing rather than by rotting.
GPU_UNLOADABLE = {
    "terrain": "fxv2 front end rejects `struct` inside a libblock "
               "(lib_terrain_vtx / TerOut); pre-existing, differential-verified",
}


################################################################################
# LEG (d)'s shader, and what it does and does not prove.
#
# The shipping _ST stages cannot be driven below the compositor: their fragment
# stages want the whole forward lighting chain (light SSBO, sampler sets, env
# probe), and the depth-prepass pair writes only depth. So leg (d) renders a
# self-contained pair whose stereo stage carries THE SAME per-view clip
# expression the authored peers carry -- and _assert_idiom_matches_authored()
# below refuses to run unless that expression is literally the one in
# pbrtools.i2. If an author changes the peers' idiom without changing this
# shader, the test fails rather than silently measuring a stale form.
#
# So: leg (d) proves the IDIOM produces true depth-dependent parallax through
# the production ublk_stereo writer on this device. Legs (a) and (c) prove every
# authored peer uses exactly that idiom, in source and in emitted GLSL. Neither
# alone is the whole claim; together they are.
################################################################################

# the clip expression under test, in the exact form pbrtools.i2 authors it.
AUTHORED_IDIOM = "(spvr_vp[ofx_viewIndex] * m) * position"

PROBE_SHADER = """
fxconfig fxcfg_default {
  glsl_version = "330";
}
uniform_block ub_stprobe (descriptor_set 0) {
  mat4 mvp;
  mat4 m;
  vec4 Tint;
}
uniform_block ublk_stereo (descriptor_set 0) {
  mat4 spvr_vp[2];
  mat4 spvr_inv_vp[2];
  vec4 spvr_eyepos[2];
}
vertex_interface vif_stprobe {
  inputs {
    vec4 position : POSITION;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec2 frg_uv0;
  }
}
fragment_interface fif_stprobe : vif_stprobe {
  outputs {
    layout(location = 0) vec4 out_clr;
  }
}
vertex_shader vs_stprobe_mono : vif_stprobe : ub_stprobe {
  gl_Position = mvp * position;
  frg_uv0     = uv0;
}
vertex_shader vs_stprobe_stereo : vif_stprobe : ub_stprobe : ublk_stereo {
  gl_Position = (spvr_vp[ofx_viewIndex] * m) * position;
  frg_uv0     = uv0;
}
fragment_shader ps_stprobe : fif_stprobe : ub_stprobe {
  vec2 cell     = floor(frg_uv0 * 8.0);
  float checker = mod(cell.x + cell.y, 2.0);
  out_clr       = vec4(Tint.rgb * (0.30 + 0.70 * checker), 1.0);
}
state_block sb_stprobe : default {
  BlendMode = OFF;
  DepthTest = LEQUALS;
  CullTest  = OFF;
}
technique STPROBE_MO {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_stprobe_mono;
    fragment_shader = ps_stprobe;
    state_block     = sb_stprobe;
  }
}
technique STPROBE_ST {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_stprobe_stereo;
    fragment_shader = ps_stprobe;
    state_block     = sb_stprobe;
  }
}
"""


def _assert_idiom_matches_authored():
  """leg (d) is only evidence about the shipping peers while it renders the
  shipping peers' clip expression. Pin that here."""
  fails = []
  if AUTHORED_IDIOM not in PROBE_SHADER:
    fails.append("the probe shader no longer carries the authored idiom %r"
                 % AUTHORED_IDIOM)
  src = _read_shader_sources().get("pbrtools.i2", "")
  if AUTHORED_IDIOM not in src:
    fails.append("pbrtools.i2 no longer carries %r -- leg (d) would be measuring "
                 "a form the shipping peers have moved off of" % AUTHORED_IDIOM)
  return fails


################################################################################
# LEG (a) -- SOURCE. Pure text; no GPU, no engine.
################################################################################


def _read_shader_sources():
  """every .fxv2/.i2 in the shader dir, concatenated per-file."""
  rval = {}
  for path in sorted(glob.glob(os.path.join(SHADER_DIR, "*.fxv2")) +
                     glob.glob(os.path.join(SHADER_DIR, "*.i2"))):
    rval[os.path.basename(path)] = open(path).read()
  return rval


# a technique block: name, then the stage names it references. Both authoring
# spellings are live in this tree (`vf_pass={vs,ps,sb}` and a `pass p0 { ... }`
# body), so both are parsed rather than one being assumed.
_TEK_RE = re.compile(r"technique\s+([A-Za-z0-9_]+)\s*\{(.*?)\n\}", re.S)
_STAGE_RE = re.compile(r"(?:vertex_shader|fragment_shader)\s*=\s*([A-Za-z0-9_]+)")
_VFPASS_RE = re.compile(r"vf_pass\s*=\s*\{([^}]*)\}")


def _techniques(sources):
  """technique name -> list of stage names it names."""
  rval = {}
  for fname, text in sources.items():
    for m in _TEK_RE.finditer(text):
      name, body = m.group(1), m.group(2)
      stages = list(_STAGE_RE.findall(body))
      for vf in _VFPASS_RE.findall(body):
        stages += [t.strip() for t in vf.split(",") if t.strip()]
      rval[name] = stages
  return rval


_STAGE_DEF_RE = re.compile(
    r"^(?:vertex_shader|fragment_shader)\s+([A-Za-z0-9_]+)(.*?)^\}", re.S | re.M)


def _stage_bodies(sources):
  rval = {}
  for fname, text in sources.items():
    for m in _STAGE_DEF_RE.finditer(text):
      rval[m.group(1)] = m.group(2)
  return rval


def leg_source():
  sources = _read_shader_sources()
  teks = _techniques(sources)
  bodies = _stage_bodies(sources)
  failures = []
  checked = 0
  for (asset, mono, stereo) in PEERS:
    if stereo not in teks:
      failures.append("%s: _ST technique ABSENT (the mono-only regression)" % stereo)
      continue
    if mono not in teks:
      failures.append("%s: mono peer %s absent -- an _ST with no twin is not a peer"
                      % (stereo, mono))
      continue
    # at least one stage the _ST technique names must perform the per-view read,
    # and the mono peer's stages must NOT (or the two are the same shader).
    st_reads = any(VIEW_READ.search(bodies.get(s, "")) for s in teks[stereo])
    mo_reads = any(VIEW_READ.search(bodies.get(s, "")) for s in teks[mono])
    if not st_reads:
      failures.append("%s: names no stage that reads spvr_*[ofx_viewIndex] "
                      "-- both eye layers would be identical" % stereo)
    if mo_reads:
      failures.append("%s: the MONO peer performs a per-view read" % mono)
    # index-select, never a branch (the tile-hardware rasterization law)
    for s in teks[stereo]:
      body = bodies.get(s, "")
      if re.search(r"(?:if|switch)\s*\([^)]*ofx_viewIndex", body):
        failures.append("%s: stage %s BRANCHES on ofx_viewIndex (index-select only)"
                        % (stereo, s))
    checked += 1
  return checked, failures


################################################################################
# CHILD: the GPU legs (b), (c) and (d). Run in its own process.
################################################################################


def _render(outdir):
  from orkengine import core
  from orkengine import lev2
  from ork.testing import headless_app, ensure_parent_dir
  import numpy
  from PIL import Image

  tokens = core.CrcStringProxy()
  results = {"resolved": {}, "unresolved": [], "glsl_dir": None}

  def _camera(eye):
    cam = lev2.CameraData()
    cam.perspective(0.5, 60.0, 45.0)
    cam.lookAt(core.vec3(eye[0], eye[1], eye[2]), core.vec3(0, 0, 0), core.vec3(0, 1, 0))
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
    print("st_peers: supports_multiview=%s max_views=%d"
          % (ctx.supports_multiview, ctx.max_multiview_views), flush=True)

    ############################################################
    # LEG (b) -- RESOLVE. gpuInit each real shader and ask for every peer by
    # name. This is the JIT-compile proof too: a shader whose authored _ST text
    # does not compile never gets here.
    ############################################################
    assets = sorted(set(a for (a, _m, _s) in PEERS))
    mtls = {}
    for asset in assets:
      m = lev2.FreestyleMaterial()
      if asset in GPU_UNLOADABLE:
        # LEG (e): the exclusion must still be REAL. A load that unexpectedly
        # SUCCEEDS is reported as a failure so the exclusion gets removed.
        try:
          m.gpuInit(ctx, "orkshader://" + asset)
        except Exception as ex:
          results.setdefault("excluded_ok", []).append(asset)
          print("st_peers: orkshader://%s unloadable as expected (%s)"
                % (asset, GPU_UNLOADABLE[asset]), flush=True)
          continue
        results.setdefault("exclusion_stale", []).append(asset)
        print("st_peers: orkshader://%s NOW LOADS -- exclusion is stale" % asset, flush=True)
      else:
        m.gpuInit(ctx, "orkshader://" + asset)
      mtls[asset] = m
      print("st_peers: gpuInit orkshader://%s ok" % asset, flush=True)

    for (asset, mono, stereo) in PEERS:
      m = mtls.get(asset)
      if m is None:
        continue   # excluded asset; leg (a) is what covers it
      tek_mo = m.technique(mono)
      tek_st = m.technique(stereo)
      results["resolved"]["%s/%s" % (asset, stereo)] = bool(tek_st)
      if not tek_st:
        results["unresolved"].append("%s: %s did not resolve out of orkshader://%s"
                                     % (stereo, stereo, asset))
      if not tek_mo:
        results["unresolved"].append("%s: MONO peer %s did not resolve out of "
                                     "orkshader://%s" % (stereo, mono, asset))

    ############################################################
    # LEG (d) -- PARALLAX. See the PROBE_SHADER note for exactly what this leg
    # is and is not evidence for.
    ############################################################
    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(ctx, "spvr_stprobe", PROBE_SHADER)
    tek_dpp_mo = mtl.technique("STPROBE_MO")
    tek_dpp_st = mtl.technique("STPROBE_ST")
    assert tek_dpp_mo, "STPROBE_MO absent -- leg (d) has no control"
    assert tek_dpp_st, "STPROBE_ST absent -- leg (d) has no subject"

    par_m = mtl.param("m")
    par_mvp = mtl.param("mvp")
    par_tint = mtl.param("Tint")
    par_zbias = None
    identity = core.mtx4()

    cams = [_camera(e) for e in EYES]
    vps = [c.vpMatrix(ASPECT) for c in cams]
    cammtx = [_cammtx(c) for c in cams]

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

    def _draw(rcfd, mvp, stereo=None, force_mono=False):
      for (ci, (cx, cy, half, z)) in enumerate(QUADS):
        if force_mono:
          # T1's tooth: the SAME pass, driven with the mono technique.
          mtl.begin(tek_dpp_mo, rcfd)
        else:
          mtl.begin(tek_dpp_mo, tek_dpp_st, rcfd)
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

    rtg_mono = [_make_rtg("StPeersMono0", 1), _make_rtg("StPeersMono1", 1)]
    rtg_stereo = _make_rtg("StPeersStereo", 2)
    rtg_tooth = _make_rtg("StPeersTooth", 2)
    print("st_peers: stereo rtg viewMask=0x%x numLayers=%d"
          % (rtg_stereo.viewMask, rtg_stereo.numLayers), flush=True)

    keys = ("mono_ref_0", "mono_ref_1", "layer0", "layer1", "tooth0", "tooth1")
    caps = {k: lev2.CaptureBuffer() for k in keys}
    caps["layer0"].capture_layer = 0
    caps["layer1"].capture_layer = 1
    caps["tooth0"].capture_layer = 0
    caps["tooth1"].capture_layer = 1

    ctx.beginFrame()

    # mono references: ordinary non-multiview path, no stereo CPD anywhere
    for i in (0, 1):
      rcfd_mono = lev2.RenderContextFrameData(ctx)
      ctx.FBI.rtGroupPush(rtg_mono[i])
      ctx.FBI.rtGroupClear(rtg_mono[i])
      _draw(rcfd_mono, vps[i])
      ctx.FBI.rtGroupPop()

    # the subject: ONE multiview pass, both views, the authored _ST peer
    def _stereo_pass(rtg, force_mono):
      rcfd = lev2.RenderContextFrameData(ctx)
      cimpl = lev2.CompositingImpl(lev2.CompositingData())
      rcfd.pushCompositor(cimpl)
      cpd = lev2.CompositingPassData()
      cpd.setSinglePassStereo(True)
      cimpl.pushCPD(cpd)
      ctx.FBI.rtGroupPush(rtg)
      ctx.FBI.rtGroupClear(rtg)
      _draw(rcfd, vps[0], stereo=cammtx, force_mono=force_mono)
      ctx.FBI.rtGroupPop()
      cimpl.popCPD()
      rcfd.popCompositor()

    _stereo_pass(rtg_stereo, False)
    _stereo_pass(rtg_tooth, True)   # T1

    futures = [
        ctx.FBI.captureAsFormat(rtg_mono[0].buffer(0), caps["mono_ref_0"], "RGBA8"),
        ctx.FBI.captureAsFormat(rtg_mono[1].buffer(0), caps["mono_ref_1"], "RGBA8"),
        ctx.FBI.captureAsFormat(rtg_stereo.buffer(0), caps["layer0"], "RGBA8"),
        ctx.FBI.captureAsFormat(rtg_stereo.buffer(0), caps["layer1"], "RGBA8"),
        ctx.FBI.captureAsFormat(rtg_tooth.buffer(0), caps["tooth0"], "RGBA8"),
        ctx.FBI.captureAsFormat(rtg_tooth.buffer(0), caps["tooth1"], "RGBA8"),
    ]
    ctx.endFrame()

    for key, fut in zip(keys, futures):
      ok = fut.wait(caps[key])
      assert ok, "st_peers: capture never landed for %s" % key

    for key in keys:
      _write_png(caps[key], os.path.join(outdir, key + ".png"))

    results["validation_armed"] = bool(ctx.validation_armed)
    results["validation_errors"] = int(ctx.validation_errors)

  with open(os.path.join(outdir, "child.json"), "w") as f:
    json.dump(results, f, indent=1)
  return 0


################################################################################
# LEG (c) -- EMITTED. Read the compiler's own GLSL dumps.
################################################################################


def _emitted_glsl(tempdir, stage_names, since):
  """stage name -> emitted GLSL, from the per-compile dumps the SPIR-V backend
  writes for EVERY stage it compiles (shadlang_backend_spirv.cpp). Filenames are
  '<stage>.t<tid>.n<seq>.glsl'; the newest wins if a stage compiled twice, and
  anything older than THIS run is ignored -- a stale dump from a previous tree
  would be read as current evidence otherwise."""
  rval = {}
  for name in stage_names:
    hits = [p for p in glob.glob(os.path.join(tempdir, name + ".t*.glsl"))
            if os.path.getmtime(p) >= since]
    hits.sort(key=lambda p: os.path.getmtime(p))
    if hits:
      rval[name] = open(hits[-1], errors="replace").read()
  return rval


def leg_emitted(tempdir, since):
  sources = _read_shader_sources()
  teks = _techniques(sources)
  wanted = set()
  for (_a, mono, stereo) in PEERS:
    wanted.update(teks.get(mono, []))
    wanted.update(teks.get(stereo, []))
  emitted = _emitted_glsl(tempdir, wanted, since)
  if not emitted:
    return None, ["no GLSL dumps from this run under %s" % tempdir]
  failures = []
  checked = 0
  for (_a, mono, stereo) in PEERS:
    st_stages = [s for s in teks.get(stereo, []) if s in emitted]
    mo_stages = [s for s in teks.get(mono, []) if s in emitted]
    if not st_stages:
      continue   # not compiled in this run; leg (b) is what enforces existence
    if not any("gl_ViewIndex" in emitted[s] for s in st_stages):
      failures.append("%s: emitted GLSL contains no gl_ViewIndex -- the per-view "
                      "read did not survive compilation" % stereo)
    for s in mo_stages:
      if "gl_ViewIndex" in emitted[s]:
        failures.append("%s: MONO stage %s emitted gl_ViewIndex" % (mono, s))
    checked += 1
  return checked, failures


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
      tempfile.gettempdir(), "spvr_st_peers")
  os.makedirs(outdir, exist_ok=True)
  t0 = time.time()
  fails = []

  # ---- leg (d)'s precondition: the probe must still carry the shipping idiom
  fail_pin = _assert_idiom_matches_authored()
  print("LEG_IDIOM_PIN failures=%d" % len(fail_pin), flush=True)
  for f in fail_pin:
    print("  LEG_IDIOM_PIN FAIL: %s" % f, flush=True)
  fails += fail_pin

  # ---- leg (a)
  checked_a, fail_a = leg_source()
  print("LEG_SOURCE peers=%d failures=%d" % (checked_a, len(fail_a)), flush=True)
  for f in fail_a:
    print("  LEG_SOURCE FAIL: %s" % f, flush=True)
  fails += fail_a

  # ---- child: legs (b) + (d), and the dumps leg (c) reads
  # the SPIR-V backend dumps every compiled stage's GLSL into
  # file::Path::temp_dir(), which is $OBT_STAGE/tempdir -- not $TMPDIR, so it
  # cannot be redirected. Timestamp the boundary instead and read only dumps
  # this run produced.
  glsl_dir = os.path.join(os.environ.get("OBT_STAGE", "/nonexistent"), "tempdir")
  t_child = time.time() - 1.0
  env = dict(os.environ)
  env["ORKID_VULKAN_VALIDATE"] = "2"
  # a warm cache serves blobs and emits no GLSL at all -- leg (c) would have
  # nothing to read and would have to skip. Force the compile in the child only.
  env["ORKID_DISABLE_SHADER_CACHE"] = "1"
  argv = [sys.executable, os.path.abspath(__file__), "--render", outdir]
  proc = subprocess.run(argv, env=env, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=600)
  out = proc.stdout.decode("utf-8", "replace")
  print(out, flush=True)
  if proc.returncode != 0:
    fails.append("child render process exited rc=%d" % proc.returncode)

  child = {}
  cjson = os.path.join(outdir, "child.json")
  if os.path.exists(cjson):
    child = json.load(open(cjson))

  # ---- leg (b)
  unresolved = child.get("unresolved", [])
  resolved = child.get("resolved", {})
  print("LEG_RESOLVE peers=%d failures=%d" % (len(resolved), len(unresolved)), flush=True)
  for f in unresolved:
    print("  LEG_RESOLVE FAIL: %s" % f, flush=True)
  fails += unresolved
  if not resolved:
    fails.append("LEG_RESOLVE produced no results at all")

  # ---- leg (e): the GPU exclusions must still be real
  stale = child.get("exclusion_stale", [])
  for asset in stale:
    fails.append("LEG_EXCLUSION: orkshader://%s now LOADS -- remove it from "
                 "GPU_UNLOADABLE and give its _ST peers the GPU legs" % asset)
  print("LEG_EXCLUSION excluded=%s stale=%s"
        % (child.get("excluded_ok", []), stale), flush=True)

  # ---- leg (c)
  checked_c, fail_c = None, ["no GLSL dumps found"]
  if os.path.isdir(glsl_dir):
    checked_c, fail_c = leg_emitted(glsl_dir, t_child)
  if checked_c is None:
    # loud skip, never a silent pass: the leg had nothing to read.
    print("LEG_EMITTED SKIP: %s (dumps absent -- leg (a) and (b) still bind)"
          % fail_c[0], flush=True)
  else:
    print("LEG_EMITTED peers=%d failures=%d" % (checked_c, len(fail_c)), flush=True)
    for f in fail_c:
      print("  LEG_EMITTED FAIL: %s" % f, flush=True)
    fails += fail_c

  # ---- leg (d) + teeth
  try:
    ref0 = _luma(os.path.join(outdir, "mono_ref_0.png"))
    ref1 = _luma(os.path.join(outdir, "mono_ref_1.png"))
    lay0 = _luma(os.path.join(outdir, "layer0.png"))
    lay1 = _luma(os.path.join(outdir, "layer1.png"))
    th0 = _luma(os.path.join(outdir, "tooth0.png"))
    th1 = _luma(os.path.join(outdir, "tooth1.png"))
  except Exception as ex:
    fails.append("LEG_PARALLAX: captures unreadable (%s)" % ex)
  else:
    # non-degenerate first: a flat capture matches another flat capture.
    for nm, im in (("mono_ref_0", ref0), ("mono_ref_1", ref1),
                   ("layer0", lay0), ("layer1", lay1)):
      rng = float(im.max() - im.min())
      if rng < RANGE_FLOOR:
        fails.append("LEG_PARALLAX: %s is degenerate (range %.4f < %.4f)"
                     % (nm, rng, RANGE_FLOOR))

    m_self0, _ = _metrics(lay0, ref0)
    m_self1, _ = _metrics(lay1, ref1)
    m_cross0, _ = _metrics(lay0, ref1)
    _, frac_views = _metrics(lay0, lay1)
    _, frac_tooth = _metrics(th0, th1)

    print("LEG_PARALLAX self0=%.5f self1=%.5f cross0=%.5f viewdiff=%.4f toothdiff=%.4f"
          % (m_self0, m_self1, m_cross0, frac_views, frac_tooth), flush=True)

    if m_self0 > MATCH_MEAN_CEIL:
      fails.append("LEG_PARALLAX: layer0 does not match its own mono reference "
                   "(%.5f > %.5f)" % (m_self0, MATCH_MEAN_CEIL))
    if m_self1 > MATCH_MEAN_CEIL:
      fails.append("LEG_PARALLAX: layer1 does not match its own mono reference "
                   "(%.5f > %.5f)" % (m_self1, MATCH_MEAN_CEIL))
    if frac_views < DIFF_FRAC_FLOOR:
      fails.append("LEG_PARALLAX: the two layers are the SAME image (%.4f < %.4f) "
                   "-- zero parallax, the A2 silent regression"
                   % (frac_views, DIFF_FRAC_FLOOR))
    # T2: the metric must be able to tell the two views apart, or the match bar
    # above is satisfied by everything.
    if m_cross0 <= MATCH_MEAN_CEIL:
      fails.append("TOOTH T2: cross-pairing matched (%.5f <= %.5f) -- the views "
                   "are indistinguishable and LEG_PARALLAX proves nothing"
                   % (m_cross0, MATCH_MEAN_CEIL))
    # T1: the same pass on the mono technique must collapse the layers.
    if frac_tooth >= DIFF_FRAC_FLOOR:
      fails.append("TOOTH T1: the MONO technique produced DIFFERENT layers "
                   "(%.4f) -- LEG_PARALLAX's difference is not the view index"
                   % frac_tooth)

  # ---- validation (R8: a warning is a failure)
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
    print("test_spvr_static_st_peers: FAIL in %.1fs" % dt, flush=True)
    return 1
  print("TESTVERDICT PASS -- %d static _ST peers authored and per-view in source; "
        "%d resolved + per-view in emitted GLSL; the authored idiom measured "
        "parallax-positive with both teeth green (%.1fs)"
        % (len(PEERS), len(resolved), dt), flush=True)
  return 0


if __name__ == "__main__":
  if len(sys.argv) > 1 and sys.argv[1] == "--render":
    sys.exit(_render(sys.argv[2]))
  sys.exit(main())
