#!/usr/bin/env ork.python
################################################################################
# DMVR capture gate: headless per-eye pixel capture of the DualMonoVr (FWDPBRVRDM)
# compositor. Closes SKYLIGHT known-issue 6 (DMVR observables were owner-in-HMD
# only) by making the two per-eye downsampled buffers machine-gateable.
#
# SPVR PROGRAM EXTENSION (deliverable 1, the GATE HARNESS): the gate now drives
# TWO output-node slots (A and B), captures per-eye from BOTH, and diffs
# EYE-FOR-EYE. Today both slots are DMVR — a SELF-PARITY BOOTSTRAP whose purpose
# is to prove the harness (capture wiring, parity metric, noise reference, teeth)
# BEFORE the SPVR output node exists. When the SPVR node lands, slot B swaps to
# it by env (ORKID_SPVR_GATE_NODE_B=SPVR) and nothing else in this file changes.
#
# Why the slot swap needs no capture-code change:
#   - onBeginAssemble/onEndAssemble are pybound on the OutputCompositingNode BASE
#     class (pyext_gfx_compositor.cpp:418-436), not on DualMonoVrOutputNode.
#   - downsampledEyeRtGroup(bool) is bound per derived node (DMVR:
#     pyext_gfx_compositor.cpp:780-783); the SPVR node owes the SAME accessor name
#     and the same per-eye buffer shape (SPVR.md R6 — slice-extract preserves the
#     accessor so the OpenXR handoff and desktop mirror are untouched). This file
#     calls it duck-typed, so an equally-named binding on the SPVR node suffices.
# The gate does NOT use ORKID_FORCE_DMVR: that env is read only by the C++ player
# (ork.ecs/examples/c++/player/main.cpp:426), never by pyext. The in-process gate
# selects nodes by building a scene per slot with that slot's rendermodel preset.
#
# Seam: DualMonoVrOutputNode renders both eyes into private per-eye downsample
# RtGroups (_ssaadownsamplebufferL/R). A read accessor
# (outputnode.downsampledEyeRtGroup(left)) exposes each eye's FINAL downsampled
# RtGroup — the exact buffers the desktop mirror blit and the XR runtime handoff
# read.
#
# Headless drive: NoVR device (orkidvr.novr_device(), nonzero IPD -> real stereo
# disparity) + a per-slot rendermodel via createSceneGraph (-> ezapp.createScene
# -> renderOnContext). Each scene renders through its OWN compositor output node.
# No HMD / OpenXR runtime needed; the compositor's dual-eye render + downsample
# run regardless of who owns HMD presentation.
#
# Two structural facts drive the (non-obvious) capture wiring — see the JUL23
# recon and the lane report:
#  1. A SceneGraphViewport swaps the scene's output node for its own
#     RtGroupOutputCompositingNode during render (viewport_scenegraph.cpp
#     DoRePaintSurface: comptek->_outputNode = _outputnode), so the SGVP path
#     structurally BYPASSES the DMVR node. Hence NO StandardSceneGraphComponent
#     here — the scene is driven via ezapp.createScene / renderOnContext.
#  2. In the createScene/renderOnContext path, onGpuPostFrame is NEVER invoked
#     (that hook fires only from the topwidget UI-draw path), but the compositor
#     DOES render continuously (onBeginAssemble/onEndAssemble fire every frame).
#     So this gate drives its frame machine from the pybound onBeginAssemble hook.
#  3. Capturing the downsample buffers INSIDE the SAME frame that produced them
#     (onEndAssemble, just before composite) leaves them in a host-read layout and
#     the immediately-following composite asserts (wrong layout). So the readback
#     runs at onBeginAssemble of the NEXT frame — the prior frame's eye buffers
#     are fully rendered+composited and get freshly re-rendered this frame, so the
#     capture's layout transition is harmless. onEndAssemble is still registered
#     purely to PROVE the pybound hook reaches the eye buffers (the slice's intent).
#  4. TWO scenes in one process, rendered SEQUENTIALLY (never both in one frame).
#     ezapp.createScene binds ITS scene as the app's draw callback (pyext_ezapp.cpp
#     :97-105 bind_scene), and the LAST createScene wins — so slot B's scene is
#     built LAZILY, from _onGpuUpdate, at the moment slot A's legs are finished.
#     The alternative (this app defining its own onDraw and dispatching) is barred:
#     topWidget.enableUiDraw() ASSERTS _userSpecifiedOnDraw == false
#     (pyext_ezapp.cpp:1153), and ComponentizedApplication.createEzApp always calls
#     it. _onGpuUpdate is the sanctioned build point: GPU thread, before the frame,
#     no render pass open (the same reasoning test_output_dither_gate records).
#     Sequential (not concurrent) rendering is what makes the frame-time leg per
#     node honest: in a given frame exactly one node's assemble runs.
#
# Scene: a lit ground grid + a cluster of near balls with strong horizontal
# parallax (close to the head) so the two eye views differ visibly. _buildSceneContent
# is the SCENE SEAM: it dispatches to the A4 synthetic occluder rig (below) when the
# rig is armed, and the capture / parity / timing machinery never sees the difference.
#
# SPVR DELIVERABLE 2 — THE A4 OCCLUDER RIG + THE CULL ORACLE. ORKID_SPVR_GATE_RIG=1
# swaps _buildSceneContent's body for the rig (occluding box + occlusion-flipping
# box on a script-owned tick schedule + particles + terrain — A4 plus A2's two extra
# material families). The tiny D1 scene stays the DEFAULT so the self-parity bootstrap
# path is untouched. Everything below the capture line is shared by both paths.
# The rig adds four legs; see the RIG block and _cullOracle for their derivations:
#   (h) CULL ORACLE (SPVR.md ruling 2 / GATE 1 leg D): the cross-eye wrong-cull
#       artifact, measured as an EYE-DIFFERENTIAL at the occlusion boundary against
#       what IPD parallax alone predicts. Reports ORACLE_GREEN / ORACLE_RED /
#       ORACLE_BLIND (blind = the harness cannot discriminate = loud FAIL, never a
#       silent pass). Its two ARMED negative controls run in-run, every run.
#   (i) A2 PARTICLE REGION: per-slot eyes-differ INSIDE the particle ROI. This is the
#       regional form the whole-frame eye-delta cannot provide: a node that misses the
#       particle stereo binding site renders both eye layers identically THERE while
#       PBR parallax keeps the whole-frame delta healthy (SPVR.md A2 family iii).
#   (j) A2 GROUND BAND: the same regional eyes-differ over the terrain band (family ii,
#       terrain's own third stereo naming scheme).
#   (k) BASE STATE (A1/Q10): the sha the gate ran at + whether the deferred-fence race
#       fix is an ancestor — "measured against whatever base it lands on, stated
#       explicitly in the gate report".
#
# SPVR RESHAPED ORACLE (deliverable 3). D2's structural finding stands and is now the
# ADJUDICATED mechanism: the cull runs ONCE PER FRAME against ONE camera, so a per-eye
# cull asymmetry is structurally impossible; the artifact the owner saw was TEMPORAL —
# the mono cull consuming a STALE or CORRUPTED pyramid. Two engine fixes closed that
# family (e83f63345 strictly-earlier-frame HZB seed guard; f11a0f21b deferred-fence
# depth-sample race), and this file's job changes accordingly: prove the family DEAD on
# the current tip, and STAY ARMED against its return. Three legs carry that:
#   (l) TEMPORAL STABILITY — the live one. At a PINNED rig pose (nothing in the scene
#       moves but the particles, which are excluded), a sequence of captures spanning
#       many full pyramid build+consume cycles must be STABLE: no drawable may appear
#       or disappear frame to frame in the oracle ROIs. A stale/corrupted pyramid
#       feeding the mono cull shows up exactly here, as visibility FLICKER. Run at BOTH
#       the boundary tick and the occluded control tick (a flip at either is damning).
#       In-run teeth: an already-captured contrasting-pose frame is spliced into the
#       sequence and must read ORACLE_TEMPORAL_RED.
#   (m) PYRAMID PROVENANCE — skip-gated. The direct form of the guard's own invariant:
#       the pyramid the cull consumes must have been built from a STRICTLY EARLIER
#       frame's depth. Needs an engine binding that does not exist yet (filed ask:
#       HZBBuilder::_sourceDepthFrame exposed as Scene.hzb.sourceDepthFrame); until it
#       lands the leg emits a VISIBLE SKIP sentinel, never a silent pass.
#   (n) GUARD-REVERT NEGATIVE — skip-gated on the SAME surface probe as (m). The two
#       engine asks (the provenance property and the ORKID_HZB_ALLOW_SAMEFRAME revert
#       knob) were filed together and land together, so one probe gates both; that
#       coupling is deliberate, and stated here because the knob itself — being an env
#       var read inside C++ — is not directly probeable from python. When armed, the
#       gate re-execs itself with the knob set and REQUIRES (l) or (m) to read RED;
#       knob-set-but-still-green is ORACLE_BLIND and fails the run, because either the
#       revert did not bite or this oracle cannot see the thing it exists to see.
# GREEN on (l) + RED under (n) is what "the cull-bug family is dead" means here. SPVR's
# layered depth later removes the mechanism CLASS outright; these legs are the guard
# for the interval, and the regression net after.
#
# Oracle (verdict-first, BEFORE teardown per defect D1) — one sentinel line per leg
# ("SPVRGATE_LEG <name> <status> ..."), because rc=0 only means "ran":
#   (a) BOTH eye captures nonblack + non-flat (mean floor + dynamic range floor),
#       per slot.
#   (b) EYE DELTA: per-pixel mean-abs-diff(L,R) ABOVE a floor (proves true stereo,
#       left != right) AND below a sanity ceiling (proves it is the SAME scene
#       from two eyes, not two unrelated frames), per slot.
#   (c) onEndAssemble hook reached the eye buffers (numBuffers>0) from Python.
#   (d) NOISE FLOOR: slot A is captured TWICE at the same config, frames apart. The
#       A-vs-A diff IS the bench noise floor of this rig (temporal jitter, async
#       residency, readback quantization). It is measured, never assumed.
#   (e) EYE-FOR-EYE PARITY: A.L vs B.L and A.R vs B.R must land INSIDE the
#       noise-referenced bar (below). This is the leg the SPVR node must survive.
#   (f) FRAME TIME (SPVR.md ruling 1 / A10): p50 frame interval + MIN-SUSTAINED,
#       per node, over a bounded timed window. Definitions at MIN_SUSTAINED_WINDOW.
#   (g) SUBMIT COUNT (A10): per-frame vkQueueSubmit count, sampled per node across the
#       SAME timed window as (f) so the timing comparison is visibly fair. See the
#       SUBMIT COUNT block below.
#
# PASS BAR for (e), stated where it is used, NEVER loosened to make a run pass
# (SPVR.md L347): bar = max(measured_noise * NOISE_SLACK, EPS_<metric>). The noise
# term makes the bar reference THIS bench; the EPS term keeps the bar meaningful
# when the rig is bit-deterministic (noise 0 -> bar EPS, i.e. near-identity). A
# DMVR-vs-DMVR parity failure outside the noise floor is a REAL FINDING (an
# unmodelled per-frame dependency in the node), never a threshold to retune.
#
# TEETH, evaluated on every run from already-captured frames (no extra render):
#   - SAMEEYE: feeding the LEFT capture as both eyes must FAIL the eyes-differ
#     check. (The whole-run form still exists: ORKID_DMVR_GATE_SAMEEYE=1 captures
#     LEFT into both outputs and the gate must FAIL end-to-end.)
#   - L/R SWAP: parity of A.L-vs-B.R and A.R-vs-B.L must FAIL the parity bar. If a
#     swapped pair passed, the parity leg would be blind to a swapped-eye node.
#   Both teeth ASSERT THEIR FAILURE: a tooth that stops failing fails the gate.
#
# Lifecycle: ComponentizedApplication; machine verdict via ork.testing.verdict
# emitted before teardown; Watchdog arms the run.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
os.environ.pop("ORKID_VR_DRIVER", None)   # ambient openxr trap -> headless NoVR
# SELF-ARMED, before any engine object exists: this gate runs AS-IS from the OBT env
# with nothing exported by the caller. Full Vulkan validation is a standing SPVR
# requirement (SPVR.md principle 3 / R8: always-on, every gate, every phase), so the
# gate arms it itself rather than trusting a runner to. setdefault, not assignment: a
# runner deliberately raising or lowering it still wins.
os.environ.setdefault("ORKID_VULKAN_VALIDATE", "2")
import sys
import time
import numpy as np

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "bin"))

from orkengine.core import (vec3, vec4, quat, mtx4, CrcStringProxy,
                            Path as CorePath, VarMap, lev2_pyexdir)
from orkengine import lev2
from ork.app.application import ComponentizedApplication
import _ork_vet_common as vet          # ssim_map: the shared SSIM instrument

lev2_pyexdir.addToSysPath()
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

OUTDIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.environ.get("TMPDIR", "/tmp"), "dmvr_capture_gate")

# capture the SAME (left) eye into both outputs -> negative/detection run.
SAME_EYE = os.environ.get("ORKID_DMVR_GATE_SAMEEYE", "0") == "1"

# ---------------------------------------------------------------- node slots --
# Slot B is the pluggable one. "DMVR" (default) = self-parity bootstrap; "SPVR" =
# the phase-1 node once the engine lane lands presetForwardPBRSPVR (SPVR.md A6).
# An unknown preset fails LOUDLY inside createScene — never silently falls back.
NODE_A_KIND = os.environ.get("ORKID_SPVR_GATE_NODE_A", "DMVR").upper()
NODE_B_KIND = os.environ.get("ORKID_SPVR_GATE_NODE_B", "DMVR").upper()
KIND_PRESET = {
  "DMVR": "FWDPBRVRDM",     # DualMonoVrOutputNode — today's per-eye node
  "SPVR": "FWDPBRSPVR",     # single-pass layered node (SPVR.md L327-331, A6)
}
SELF_PARITY = (NODE_A_KIND == NODE_B_KIND)

# A4 SYNTHETIC OCCLUDER RIG (deliverable 2). Off by default: the tiny D1 scene is the
# self-parity bootstrap scene and its legs must stay exactly as measured.
RIG = os.environ.get("ORKID_SPVR_GATE_RIG", "0") == "1"
# manual single-pose inspection (the authoring rig's SPVR_RIG_FREEZE_TICK): pins the
# flipper for the WHOLE run. The gate's own capture legs freeze per-step regardless —
# this env exists to eyeball one pose, and it makes the oracle legs meaningless (all
# three "ticks" render the same pose), which they then say out loud.
_FRZ = os.environ.get("ORKID_SPVR_RIG_FREEZE_TICK", "").strip()
RIG_FREEZE_TICK = int(_FRZ) if _FRZ != "" else None

# eye buffer: the rig needs the flipper resolved well enough that a per-eye AREA
# difference is a graded measurement and not a 1-bit one. The flipper subtends
# RIG_FLIP_ANG rad (below); at 512/90deg that is ~19px across and its per-row reveal
# only ~7px tall, so the differential is a handful of pixels. 1024 doubles both.
EYE_W, EYE_H = (1024, 1024) if RIG else (512, 512)
SETTLE_FRAMES = 90      # compositor frames AFTER env residency, before capture
SETTLE_SECONDS_MIN = 1.0  # wall-time floor on settle (frames alone are ~free here)
ENV_WAIT_SECONDS = 60.0   # residency deadline per slot -> fail LOUD, never capture
                          #  an unsettled frame (an A/B residency skew would read as
                          #  a parity failure that has nothing to do with the nodes)
WRITE_FRAMES  = 40      # frames allowed for an async capture to land
TIMED_SECONDS = 3.0     # frame-time window per node (3x the 1s min-sustained win)
MIN_SUSTAINED_WINDOW = 1.0   # see _minSustainedFps

# head pose (static): eye a few meters back, looking at the ball cluster.
HEAD_EYE = vec3(0, 2.2, 6.0)
HEAD_TGT = vec3(0, 1.0, 0)
HEAD_UP  = vec3(0, 1, 0)
IPD_M    = 0.065   # metres. ONE constant: the device is configured from it and the
                   #  rig's parallax predictions are derived from it.

# oracle thresholds (0..255 luminance space); real delta measured ~7.9.
MEAN_FLOOR   = 5.0      # per-eye mean brightness proving nonblack
RANGE_FLOOR  = 12.0     # per-eye (max-min) proving non-flat content
DELTA_FLOOR  = 0.30     # mean|L-R| proving true stereo (identical -> 0 -> FAIL)
DELTA_CEIL   = 45.0     # mean|L-R| ceiling proving the SAME scene, two eyes

# parity bar (see header): bar = max(noise * NOISE_SLACK, EPS). NOISE_SLACK is
# headroom over the MEASURED bench noise, not a tuning knob — raising either of
# these to make a run pass is the exact move SPVR.md L347 forbids.
NOISE_SLACK      = 2.0
EPS_PARITY_MEAN  = 0.05   # 0..255 mean|a-b|; 8-bit LSB is 1.0, so this is tight
EPS_PARITY_MAX   = 2.0    # 0..255 worst single pixel
EPS_PARITY_SSIM  = 0.005  # 1 - min(block SSIM); identical frames -> exactly 0
FRAMETIME_DIVERGENCE_WARN = 0.10   # >10% p50 spread between nodes -> WARN line

# ------------------------------------------------------------- SUBMIT COUNT --
# A10 requires submit count logged beside timing so the DMVR-vs-SPVR fairness is
# VISIBLE, not assumed. The engine lane landed the counter: Context::submitCount()
# tallies every vkQueueSubmit (graphics, compute, capture, external composite — they
# all funnel through VkThreadedQueue::queueSubmit) and RESETS at _doBeginFrame, so the
# value read at a frame boundary is that frame's DELTA, not a running total. It is
# pybound as the readonly property ctx.submitCount, and test_submit_counter.py is the
# committed oracle for the property itself (nonzero, and NOT accumulating).
#
# This gate therefore samples it ONCE PER FRAME across the SAME timed window that
# produces the frame-time numbers, and reports p50 + total per node. The comparison
# BETWEEN slots stays non-gating INFO for the same reason the frame-time divergence
# line is: two nodes submitting differently is a fact to see, not a bar to pass, until
# the real DMVR-vs-SPVR ruling lands.
#
# Fail-loud fallback: an install predating the binding has no such property. Reading it
# with a default and calling that "0 submits" would be exactly the silent zero A10
# forbids, so an absent property yields None and the leg says UNAVAILABLE — and FAILS
# outright once the two slots are DIFFERENT nodes, because A10's fairness claim cannot
# be made without it. In self-parity (both slots the same kind) fairness is trivial, so
# it stays loud-but-non-gating.
def _submitCount(ctx):
  """this frame's submit delta, or None when the install has no counter."""
  if not hasattr(ctx, "submitCount"):
    return None
  return int(ctx.submitCount)


################################################################################
# A4 SYNTHETIC OCCLUDER RIG — geometry, schedule, oracle constants.
#
# PORTED, not re-authored. The reference rig (authored + measured as a Tier-3 DSL
# scene driven by the C++ player) is the source of the CONTENT, the PROPORTIONS and
# the TICK SCHEDULE; this file re-hosts them in the gate's in-process scenegraph and
# drives them from the gate's own frame loop. Constraints carried over from the rig's
# authoring, NOT rediscovered here: HypermeshComponent ignores entity transforms (so
# occluder+flipper are MODEL drawables, one box model at two scales), and the terrain
# is the raw chunked-terrain pattern, not a scene-mixin terrain (whose extent is
# authored for kilometre scenes).
#
# CAMERA: the gate's own static head pose (HEAD_EYE/HEAD_TGT). The rig framed itself
# TO the player's static camera; the same trick is applied here in reverse — every rig
# point is expressed in ITS camera frame as (depth, right, up) metres, then replayed
# in the gate camera's frame scaled by RIG_K. A UNIFORM scale about the eye preserves
# EVERY ANGLE in the rig, so the measured tick schedule (below) transfers exactly and
# no assumption about the box model's metric size is needed.
#
#   rig camera C=(20,8,20) T=(0,2,0): u=(-.69171,-.20751,-.69171)
#                                     R=( .70711, 0     ,-.70711)  (screen right)
#                                     U=(-.14673, .97824,-.14673)
#   rig point            (depth,  right,   up)      -> scale by RIG_K
#     flipper @tick0     (31.265, 0.000,  2.543)     occluder sits ON this ray
#     occluder box       (14.936, 0.000,  1.216)     t=0.478 along camera->flipper
#     particles          (21.478, 5.657,  0.978)     straddles the boundary edge
#     ground centre      (29.329, 0.000, -1.957)     -> the terrain's y offset
#
# A uniform scale changes NO angle, so RIG_K does not affect framing at all — it is
# purely the ORACLE's sensitivity knob, and the one number the port has to get right.
# The eye-to-eye reveal difference at the boundary is IPD*(1/d_box - 1/d_flip) rad
# while the flipper subtends RIG_FLIP_ANG rad, so
#     SEPARATION = flipper_width / parallax_difference = RIG_FLIP_ANG*d_flip / 0.0711
# is the ratio between "one eye is missing the whole object" and "the eyes differ by
# parallax alone" — the oracle's entire dynamic range. d_flip=18m puts it near 15x, so
# the pass band sits a factor of ~7 clear of a whole-object excursion while the
# legitimate differential is still 2.57px of edge shift (35px of area, measured) —
# well above the rig's resolution floor. Smaller d_flip collapses the two readings
# together; larger sinks the legitimate signal into pixel quantization.
################################################################################

RIG_D_FLIP   = 18.0                       # metres, gate frame
RIG_K        = RIG_D_FLIP / 31.265        # uniform scale of the whole rig about the eye

RIG_FLIP_ANG = 0.0583                     # rad; the flipper's angular width, MEASURED
                                          #  on this port's own renders (38px of a
                                          #  1024px/90deg eye). The reference rig's px
                                          #  figure is not portable — its frame size is
                                          #  not recorded with it.
RIG_D_BOX    = 14.936 * RIG_K             # occluder distance (the 0.478 ray fraction)

# schedule — verbatim from the reference rig's behaviour script (_occluder_flip.py).
# The tick is the GATE FRAME INDEX the capture legs pin, never wall clock.
RIG_TICKS_TOTAL = 240
RIG_SLIDE_MAX   = 9.5 * RIG_K             # metres along screen-right; linear ramp, clamped
# THE FLIP BAND IS RE-MEASURED ON THIS HOST, not inherited. The reference rig's table
# (onset t40, saturated t128+) does not reproduce here — its pixel figures carry no
# frame size, so they cannot be re-derived, and its own scale ratios (occluder
# silhouette 5.2x the flipper's) put clearance much later than t128. MEASURED on this
# port's captures: at t90 the reveal is 59% of the cleared area AND its left edge
# still carries the occluder's disparity; by t130 it is 98% and the left edge has
# relaxed to the flipper's own — the boundary is gone. So the controls and the
# boundary tick below are this port's, confirmed against its own renders.
def _rigTickEnv(name, dflt):
  v = os.environ.get("ORKID_SPVR_RIG_TICK_" + name, "").strip()
  return int(v) if v else dflt

RIG_TICK_OCC = _rigTickEnv("OCC", 20)    # control: unambiguously occluded
RIG_TICK_BND = _rigTickEnv("BND", 90)    # THE BOUNDARY TICK. Chosen by measuring the
                                         #  reveal's own per-row LEFT EDGE disparity
                                         #  across ticks: at t90 it reads -4.04px, the
                                         #  OCCLUDER's disparity, while the right edge
                                         #  reads -2.33px, the flipper's own (predicted
                                         #  -2.35) — i.e. the reveal is genuinely
                                         #  bounded by the silhouette. By t130 the left
                                         #  edge has relaxed to -2.00px: the flipper has
                                         #  cleared the box entirely and there is no
                                         #  boundary left to measure. t90 sits at 59%
                                         #  of the cleared area: graded both ways.
RIG_TICK_CLR = _rigTickEnv("CLR", 200)   # control: unambiguously clear, past saturation
RIG_TICK_HOLD_FRAMES  = 24     # frames held after a tick change before capturing: the pose
RIG_TICK_HOLD_SECONDS = 0.30   #  crosses update-thread -> drawable-buffer -> render

# oracle thresholds. NOT tuned to make a run pass — each is derived here.
RIG_FLIP_THR   = 12.0    # 0..255 luma change that counts as "the flipper appeared" —
                         #  the reference measurement's 0.06 of unit range. Swept 3..32
                         #  on this port's captures: the reveal areas move ~5% across
                         #  the whole sweep, so the metric is not threshold-perched.
RIG_PTC_MARGIN = 24      # px dilation of the measured particle-jitter bbox
RIG_ROI_MARGIN = 10      # px dilation of the flipper's reveal bbox
RIG_CTRL_AREA_MIN   = 40   # px: the clear-control reveal must be a real object, per eye,
                           #  else the rig is mis-framed and the oracle says BLIND
RIG_DISCRIM_AREA    = 15   # px: the PREDICTED eye-differential area must exceed this or
                           #  the rig cannot resolve the effect it is built to see (BLIND)
# THE POSE PRECONDITION. The oracle only means anything while the flipper is PARTIALLY
# occluded — once it clears the box there is no silhouette bounding its reveal and the
# eye-differential decays to the flipper's own parallax (measured: the reveal's left
# edge relaxes from the occluder's disparity to the flipper's between t90 and t130).
# Checked on the FIRST-RENDERED eye alone, from the flipper's own kinematics, so it
# cannot be perturbed by whatever the second eye does.
RIG_POSE_FRAC_LO = 0.25    # emerged area / cleared area, first eye
RIG_POSE_FRAC_HI = 0.75
RIG_DISPARITY_TOL   = 0.50 # measured flipper disparity must land within 50% (or 2px) of
                           #  IPD/d_flip — the harness proving its own geometric model
                           #  matches the render BEFORE it claims anything is "beyond
                           #  what parallax explains"
# The parallax-explained band, in units of the predicted eye-shift. A healthy render
# reads 1.0. The prediction is a thin-edge first-order model (it ignores the box's
# silhouette curvature, the flipper's finite width and its shading), so it is trusted
# to a factor of a few — its job is to catch a COLLAPSE (the second eye losing a reveal
# it should have = the wrong-cull signature) or a BLOWUP (a whole object culled in one
# eye), not to validate a projection to percent. The band's top sits between 1.0 and
# the SEPARATION above (~15 for a fully absent flipper): the measured healthy reading
# is 0.87 and the armed whole-object controls land an order of magnitude past the top,
# so both stay a wide factor clear of the boundary.
RIG_RATIO_LO = 0.35
RIG_RATIO_HI = 2.00

# --- D3 TEMPORAL ORACLE -------------------------------------------------------
# N, JUSTIFIED. The hazard is a cull consuming a pyramid built from the WRONG frame's
# depth, so the cycle that has to be spanned is one HZB build+consume: the pyramid is
# built at frame f from depth recorded at f-1 and the cull fan-out consumes it inside f
# — a 2-frame cycle. Each temporal take costs a capture step (1 frame) plus a drain step
# (>=1 frame, and in practice 2-4 while the async readback lands), so ONE take already
# spans a full cycle and 10 takes span >=20 engine frames = >=10 cycles. The gate does
# not assume that: every take records the engine's own ctx.frameIndex and the leg REPORTS
# the measured frame span, so the claim "several full cycles" is a number in the log and
# not a hope. 10 also keeps the two sequences x two slots inside the runtime budget
# (measured cost is stated in the lane report).
RIG_TEMPORAL_N = int(os.environ.get("ORKID_SPVR_RIG_TEMPORAL_N", "").strip() or 10)
# the pinned-pose sequence must be STABLE, and "stable" is bounded two ways:
#  - VISIBILITY: the reveal AREA (px the flipper occupies against the contrasting-pose
#    reference) may not move by more than this across the whole sequence. A wrong cull
#    removes a whole DRAWABLE — the flipper reveal is hundreds of px and a terrain chunk
#    far more — so a 2% band sits ~50x under the smallest artifact worth catching while
#    staying above any legitimate resample wobble. The absolute floor keeps a small
#    reference area from collapsing the band to nothing.
RIG_TEMPORAL_AREA_FRAC = 0.02
RIG_TEMPORAL_AREA_MIN  = 8       # px
#  - IDENTITY: mean|frame_i - frame_0| must stay under the SAME ratified parity bar the
#    A/B legs use, measured REGIONALLY (the flipper ROI and the terrain band) and never
#    frame-wide: a whole-frame mean dilutes a flipper-sized flip (~600px of ~30 luma over
#    1024^2 = 0.017) to BELOW the bar, i.e. the frame-wide form is structurally blind to
#    the very event this leg exists to see. The frame-wide number is still reported, as
#    INFO, because a large one is worth a human look.
RIG_GROUND_BAND = (0, int(EYE_H * 2 // 3), EYE_W, EYE_H)   # terrain family's own region

# --- D3 PROVENANCE / GUARD-REVERT SURFACE ------------------------------------
# Both legs ride ONE probe (see header (n)): the engine asks were filed together and the
# revert knob — an env var read in C++ — cannot be probed from python at all, so the
# provenance property is the observable that says "the D3 engine surface is present".
HZB_REVERT_ENV = "ORKID_HZB_ALLOW_SAMEFRAME"
# recursion guard AND the child's own marker: the child must not spawn a grandchild, and
# it inverts the temporal leg's gating (under the reverted guard, RED is the PASS).
REVERT_CHILD = os.environ.get("ORKID_SPVR_GATE_REVERTCHILD", "0") == "1"
REVERT_CHILD_N = 4          # takes per sequence in the child — enough cycles to catch a
                            #  per-frame hazard, short enough to keep the armed run cheap
REVERT_CHILD_TIMEOUT = 240.0

RIG_TERRA_DIM    = 256
RIG_TERRA_EXTENT = 96.0 * RIG_K     # metres
RIG_TERRA_RELIEF = 0.80             # metres, and SHORT-WAVELENGTH (see _rigHeights).
                                    #  The reference rig's smooth 0.6m-over-96m ground
                                    #  renders as a FEATURELESS plane at this extent:
                                    #  measured 0.007 eye-delta over the whole ground
                                    #  band, i.e. A2's terrain family would have been
                                    #  gated by a metric that is structurally zero. A
                                    #  matte terrain's only visible signal is its own
                                    #  shading, so the ground needs relief at a
                                    #  wavelength the eye separation can resolve.
RIG_TERRA_CHUNK  = 8                # 32x32 chunks. The reference rig's chunk=64 gives
                                    #  4 chunks a side — a chunk an order of magnitude
                                    #  LARGER than the occluder, so terrain (the only
                                    #  HZB-cull consumer in the scene) could never
                                    #  produce a cull event at the boundary at all.
RIG_TERRA_Y      = -1.957 * RIG_K   # ground centre height in the gate frame

RIG_PTC_POOL = 4096


def _rigFrame(eye, tgt, up):
  """the (depth,right,up) basis of a lookAt camera, as the rig's own frame comment."""
  u = (tgt - eye).normalized
  r = u.cross(up).normalized
  v = r.cross(u).normalized
  return u, r, v


def _rigSlide(tick):
  """s(tick): linear ramp, clamped both ends. Pure function of the tick index — the
  reference behaviour script's schedule, scaled by RIG_K."""
  f = float(tick) / float(RIG_TICKS_TOTAL)
  f = 0.0 if f < 0.0 else (1.0 if f > 1.0 else f)
  return RIG_SLIDE_MAX * f


_RIG_U, _RIG_R, _RIG_V = _rigFrame(HEAD_EYE, HEAD_TGT, HEAD_UP)


def _rigPlace(depth, right, up):
  """a reference-rig camera-frame point, replayed in the gate camera's frame at RIG_K."""
  return HEAD_EYE + (_RIG_U * (depth * RIG_K)
                     + _RIG_R * (right * RIG_K)
                     + _RIG_V * (up * RIG_K))


RIG_FLIP_ORIGIN = _rigPlace(31.265, 0.0, 2.543)
RIG_BOX_POS     = _rigPlace(14.936, 0.0, 1.216)
# particles: the reference rig hung them ON the occlusion boundary (up=0.978) so one
# region carried both evidences. Measured here, that lands the cloud ON TOP of the
# flipper's reveal — and since the cloud is the one region the identity metrics must
# EXCLUDE, it ate the oracle's own ROI (measured: the whole boundary reveal masked
# away). Lifted clear of the flipper's row band; still straddling the boundary edge
# horizontally. Cost, stated: the "particles at the boundary" secondary manifestation
# is no longer in the same pixels as the flipper's — the oracle rests on the flipper
# reveal alone, and the particle region serves A2's stereo-binding check.
RIG_PTC_POS     = _rigPlace(21.478, 5.657, 7.500)
RIG_PTC_RADIUS_M = 1.75   # DECLARED cloud radius (emission velocity x lifespan, times
                          #  a factor for dispersion + sprite size). The excluded region
                          #  is this projected disc, NOT the measured jitter: two takes
                          #  seconds apart differ over a much smaller footprint than the
                          #  cloud actually covers, and a mask fitted to the short
                          #  baseline leaves cloud pixels inside the identity metrics
                          #  (measured: parity max 75/255 from ~90 uncovered pixels).
                          #  The measured jitter is used the other way round — as a
                          #  CONTAINMENT check on this declaration.
RIG_SLIDE_AXIS  = _RIG_R                      # camera screen-right, in world space
RIG_BOX_SCALE   = 3.0 * RIG_K                 # reference rig scales, uniformly ported
RIG_FLIP_SCALE  = 1.2 * RIG_K
RIG_BOX_MODEL   = "data://tests/misc_gltf_samples/art_and_sculpture/obox.glb"

# radians per pixel of the eye buffer (FOVD is the device's full field, EYE_W wide).
RIG_RAD_PER_PX = (90.0 * 3.14159265358979 / 180.0) / float(EYE_W)


def _rigProject(p):
  """world point -> (x,y,depth) in the HEAD's eye image. The two eyes differ by half an
  IPD, ~2px here — absorbed by the margins of everything this is used for."""
  v = p - HEAD_EYE
  depth = v.dot(_RIG_U)
  x = EYE_W * 0.5 + (v.dot(_RIG_R) / depth) / RIG_RAD_PER_PX
  y = EYE_H * 0.5 - (v.dot(_RIG_V) / depth) / RIG_RAD_PER_PX
  return x, y, depth


def _rigParticleRect():
  """the DECLARED particle exclusion rect: the projected cloud disc."""
  x, y, depth = _rigProject(RIG_PTC_POS)
  r = (RIG_PTC_RADIUS_M / depth) / RIG_RAD_PER_PX + RIG_PTC_MARGIN
  return (max(0, int(x - r)), max(0, int(y - r)),
          min(EYE_W, int(x + r) + 1), min(EYE_H, int(y + r) + 1))


################################################################################

def _minSustainedFps(dts, window_s=MIN_SUSTAINED_WINDOW):
  """MIN-SUSTAINED, defined: the MINIMUM 1-second rolling-average frame RATE over
  the timed window. For every frame i, take the shortest run of consecutive frames
  starting at i that spans >= window_s of wall time, and score that run at
  frames/elapsed fps; min-sustained is the smallest such score. It answers "what
  is the worst full second this node delivered", which a p50 cannot see.
  Returns None when the window is shorter than window_s (stated, never faked)."""
  if len(dts) < 2:
    return None
  t = np.concatenate([[0.0], np.cumsum(np.asarray(dts, dtype=np.float64))])
  if t[-1] < window_s:
    return None
  starts = np.arange(len(t) - 1)
  ends = np.searchsorted(t, t[:-1] + window_s, side="left")
  valid = ends < len(t)
  if not valid.any():
    return None
  s, e = starts[valid], ends[valid]
  return float(((e - s) / (t[e] - t[s])).min())


def _parityMetrics(a, b):
  """idiff + SSIM between two same-shape luma frames (0..255).

  float64 is NOT optional: ssim_map forms cov as E[ab]-E[a]E[b], which in float32
  cancels hard enough on a high-variance block to score ~5e-4 BELOW 1.0 for two
  byte-identical frames — instrument noise that would have set the parity bar."""
  a = a.astype(np.float64)
  b = b.astype(np.float64)
  d = np.abs(a - b)
  ss = vet.ssim_map(a / 255.0, b / 255.0, win=8)
  return {
    "mean": float(d.mean()),
    "max": float(d.max()),
    "ssim_worst": float(1.0 - ss.min()),
  }


def _parityBars(noise):
  """noise-referenced pass bar; see header. noise is a metrics dict (or None)."""
  n = noise or {"mean": 0.0, "max": 0.0, "ssim_worst": 0.0}
  return {
    "mean": max(n["mean"] * NOISE_SLACK, EPS_PARITY_MEAN),
    "max": max(n["max"] * NOISE_SLACK, EPS_PARITY_MAX),
    "ssim_worst": max(n["ssim_worst"] * NOISE_SLACK, EPS_PARITY_SSIM),
  }


def _parityWithin(m, bars):
  return (m["mean"] <= bars["mean"] and m["max"] <= bars["max"]
          and m["ssim_worst"] <= bars["ssim_worst"])


def _eyesDiffer(L, R):
  """the existing stereo oracle, as a pure function so the teeth can call it."""
  delta = float(np.abs(L - R).mean())
  return delta, (delta >= DELTA_FLOOR and delta <= DELTA_CEIL)


################################################################################
# rig image algebra: rectangles-as-(x0,y0,x1,y1)-half-open, masks as bool arrays.
################################################################################

def _bboxOf(mask, margin=0):
  ys, xs = np.nonzero(mask)
  if xs.size == 0:
    return None
  h, w = mask.shape
  return (max(0, int(xs.min()) - margin), max(0, int(ys.min()) - margin),
          min(w, int(xs.max()) + 1 + margin), min(h, int(ys.max()) + 1 + margin))


def _rectMask(shape, rect):
  m = np.zeros(shape, dtype=bool)
  if rect is not None:
    x0, y0, x1, y1 = rect
    m[y0:y1, x0:x1] = True
  return m


def _rectArea(rect):
  return 0 if rect is None else (rect[2] - rect[0]) * (rect[3] - rect[1])


RACE_FIX_SHA = "f11a0f21b"    # the deferred-fence race fix (perf lane, SPVR.md A1)


def _baseState():
  """A1/Q10: the cull-bug verdict is measured against WHATEVER base it lands on, and
  the base must be stated in the gate report. The harness cannot see it in pixels, so
  it reads it from git (or from ORKID_SPVR_GATE_BASE_SHA when a runner knows better)."""
  import subprocess

  def _git(*args, **kw):
    return subprocess.run(("git", "-C", _ROOT) + args, capture_output=True,
                          text=True, timeout=20, **kw)

  sha = os.environ.get("ORKID_SPVR_GATE_BASE_SHA", "").strip()
  src, dirty = "env", ""
  if not sha:
    try:
      sha = _git("rev-parse", "HEAD").stdout.strip() or "UNKNOWN"
      src = "git"
      dirty = " +local-edits" if _git("status", "--porcelain").stdout.strip() else ""
    except Exception as e:                       # noqa: BLE001 — reported, never raised
      sha, src = "UNKNOWN", "unavailable(%s)" % e
  try:
    if _git("rev-parse", "--verify", "--quiet", RACE_FIX_SHA + "^{commit}").returncode:
      race = "commit-not-in-this-repo"
    else:
      race = ("present" if _git("merge-base", "--is-ancestor",
                                RACE_FIX_SHA, "HEAD").returncode == 0 else "ABSENT")
  except Exception as e:                         # noqa: BLE001
    race = "unknown(%s)" % e
  return "sha=%s%s (%s) | race_fix %s: %s" % (sha[:12], dirty, src, RACE_FIX_SHA, race)


def _zeroRect(a, rect):
  """copy with rect zeroed — how a non-identity region is taken OUT of an identity
  metric. Zeroing BOTH sides of a comparison contributes exactly 0 diff and SSIM 1."""
  if rect is None:
    return a
  b = a.copy()
  x0, y0, x1, y1 = rect
  b[y0:y1, x0:x1] = 0.0
  return b


def _regionalDelta(L, R, rect, exclude=None):
  """mean |L-R| inside rect (minus exclude). The A2 regional oracle: a node that
  misses ONE family's stereo binding site renders that family identically into both
  eye layers while every other family keeps its parallax — invisible to a whole-frame
  delta, visible here. L and R come from the SAME captured frame, so a non-reproducible
  family (particles) is still a fair comparison."""
  sel = _rectMask(L.shape, rect)
  if exclude is not None:
    sel &= ~_rectMask(L.shape, exclude)
  n = int(sel.sum())
  if n == 0:
    return None, 0
  return float(np.abs(L - R)[sel].mean()), n


################################################################################
# THE CULL ORACLE (SPVR.md ruling 2 / GATE 1 leg D).
#
# WHAT THE ARTIFACT IS: the HZB record/build runs per eye against ONE shared depth
# image and `_hzb_seeded_rtgs` (fwdnode_impl.h:228) is erased on RTG RESIZE ONLY
# (fwdnode_impl_top.cpp:477, insert :620, consumed :503) keyed on `_node->_bufferKey`,
# which is NOT eye-indexed. The FIRST eye's insert makes `depth_impl_ready` true for
# the SECOND eye's same-frame HZB build, so the occlusion pyramid the cull consumes
# was built from the FIRST eye's depth. Geometry the second eye can genuinely see
# around a near occluder's silhouette is occluded in the first eye's depth — so it
# gets culled where it should have been drawn. Per-frame mechanical: a STATIC camera
# and a frozen pose reproduce it; no motion is required of the observer.
#
# WHAT THE OBSERVABLE MUST BE. Eyes legitimately differ — at the occlusion boundary
# they differ the MOST, because the near occluder's silhouette shifts against the far
# flipper by the differential parallax IPD*(1/d_box - 1/d_flip). So a raw eye-delta
# proves nothing. The oracle instead measures the eye-differential IN THE REVEAL and
# compares it to what parallax alone predicts:
#
#   reveal_e   = pixels the flipper occupies at the boundary tick that it did NOT
#                occupy at the occluded control tick, in eye e, inside the ROI
#   h_eff      = that reveal's MEAN COLUMN HEIGHT (area / occupied columns) — the
#                honest area<->edge-shift conversion for a strip that is not a
#                rectangle (the model is a rounded solid, so its bbox height would
#                systematically over-divide)
#   measured   = (reveal_R - reveal_L) / h_eff                  [px of edge shift]
#   predicted  = IPD*(1/d_box - 1/d_flip) / rad_per_px          [px of edge shift]
#
# DMVR renders LEFT first, so the RIGHT eye is the one that culls against foreign
# depth: the wrong cull SUPPRESSES the reveal the right eye should have had and the
# ratio measured/predicted COLLAPSES toward (or below) zero. The opposite excursion —
# a whole object missing from one eye — blows the ratio up. Both are "beyond what
# geometry parallax explains", so the band is two-sided.
#
# The oracle refuses to answer rather than answer blindly. It BLINDs (a loud FAIL,
# never a silent pass) when its own preconditions are unmet: the controls did not
# move, the reveal is too small to resolve, or its geometric model disagrees with the
# render (checked against the flipper's OWN measured disparity, IPD/d_flip).
################################################################################

def _cullOracle(occ, bnd, clr, pmask, check_pose=True):
  """occ/bnd/clr: {"L": frame, "R": frame} at the three ticks. Returns (status, detail,
  metrics) with status in ORACLE_GREEN / ORACLE_RED / ORACLE_BLIND.

  check_pose=False is for the ARMED CONTROLS only: they substitute a boundary frame to
  prove the DISCRIMINATOR fires, and the rig-pose precondition (which the real captures
  satisfied a moment earlier) would otherwise reject the substitution itself."""
  m = {}
  frames = [f for d in (occ, bnd, clr) for f in (d.get("L"), d.get("R"))]
  if any(f is None for f in frames) or len({f.shape for f in frames}) != 1:
    return "ORACLE_BLIND", "missing or mismatched captures", m
  shape = frames[0].shape
  keep = ~_rectMask(shape, pmask)

  clrmask, area_clr, cx = {}, {}, {}
  for eye in ("L", "R"):
    cm = (np.abs(clr[eye] - occ[eye]) > RIG_FLIP_THR) & keep
    clrmask[eye] = cm
    area_clr[eye] = int(cm.sum())
    xs = np.nonzero(cm)[1]
    cx[eye] = float(xs.mean()) if xs.size else None
  m["area_clr_L"], m["area_clr_R"] = area_clr["L"], area_clr["R"]
  if min(area_clr.values()) < RIG_CTRL_AREA_MIN:
    return ("ORACLE_BLIND",
            "clear-control reveal too small (L=%d R=%d px, need >=%d each): the flipper "
            "never became visible — rig framing or the tick hold is wrong, NOT a verdict "
            "about culling" % (area_clr["L"], area_clr["R"], RIG_CTRL_AREA_MIN), m)

  # geometric self-check: the flipper's OWN disparity must read as IPD/d_flip.
  dx_meas = cx["R"] - cx["L"]
  dx_pred = -IPD_M / RIG_D_FLIP / RIG_RAD_PER_PX      # right eye sees it shifted LEFT
  m["disparity_meas_px"], m["disparity_pred_px"] = dx_meas, dx_pred
  if abs(dx_meas - dx_pred) > max(RIG_DISPARITY_TOL * abs(dx_pred), 2.0):
    return ("ORACLE_BLIND",
            "flipper disparity %.2fpx vs predicted %.2fpx: the harness's geometric "
            "model does not match the render, so no claim about 'beyond parallax' can "
            "be made" % (dx_meas, dx_pred), m)

  # the flipper SLIDES, so the boundary footprint is NOT the clear-control footprint:
  #  the ROI is the union of both (dilated), and the areas are counted inside it.
  bndmask = {e: (np.abs(bnd[e] - occ[e]) > RIG_FLIP_THR) & keep for e in ("L", "R")}
  roi = _bboxOf(bndmask["L"] | bndmask["R"] | clrmask["L"] | clrmask["R"],
                RIG_ROI_MARGIN)
  roimask = _rectMask(shape, roi)
  m["roi"] = roi
  area, cols = {}, {}
  for eye in ("L", "R"):
    mk = bndmask[eye] & roimask
    area[eye] = int(mk.sum())
    cols[eye] = int(mk.any(axis=0).sum())
  m["area_bnd_L"], m["area_bnd_R"] = area["L"], area["R"]
  # h_eff: the reveal's MEAN COLUMN HEIGHT. The model is a rounded solid, so its bbox
  #  height would systematically over-divide the area difference.
  hs = [area[e] / float(cols[e]) for e in ("L", "R") if cols[e] > 0]
  h_eff = float(np.mean(hs)) if hs else 0.0
  m["h_eff_px"] = h_eff
  m["bnd_bbox"] = _bboxOf(bndmask["L"] | bndmask["R"])

  pred_shift = IPD_M * (1.0 / RIG_D_BOX - 1.0 / RIG_D_FLIP) / RIG_RAD_PER_PX
  m["pred_shift_px"] = pred_shift
  if (area["L"] + area["R"]) == 0 or h_eff <= 0.0:
    return ("ORACLE_BLIND",
            "no reveal in EITHER eye at the boundary tick (t%d): the tick is outside "
            "this rig's flip band, not a cull verdict" % RIG_TICK_BND, m)
  frac = area["L"] / float(max(area_clr["L"], 1))
  m["pose_frac"] = frac
  if check_pose and not (RIG_POSE_FRAC_LO <= frac <= RIG_POSE_FRAC_HI):
    return ("ORACLE_BLIND",
            "first eye's reveal is %.0f%% of its cleared area (need %.0f-%.0f%%): the "
            "flipper is not straddling the occluder at t%d, so there is no occlusion "
            "boundary to measure — a rig-pose problem, not a cull verdict"
            % (100 * frac, 100 * RIG_POSE_FRAC_LO, 100 * RIG_POSE_FRAC_HI,
               RIG_TICK_BND), m)
  if pred_shift * h_eff < RIG_DISCRIM_AREA:
    return ("ORACLE_BLIND",
            "predicted eye-differential %.1f px of area (shift %.2fpx x h_eff %.1fpx) "
            "is under the %d px resolution floor: this rig scale cannot see the effect "
            "it exists to see"
            % (pred_shift * h_eff, pred_shift, h_eff, RIG_DISCRIM_AREA), m)

  meas_shift = (area["R"] - area["L"]) / h_eff
  ratio = meas_shift / pred_shift
  m["meas_shift_px"], m["ratio"] = meas_shift, ratio
  detail = ("reveal L=%d R=%d px (%.0f%% of cleared), h_eff=%.1fpx, flip disparity "
            "%.2fpx (pred %.2f) -> eye-shift %.2fpx vs parallax-predicted %.2fpx, "
            "ratio=%.2f (band %.2f..%.2f)"
            % (area["L"], area["R"], 100 * frac, h_eff, dx_meas, dx_pred,
               meas_shift, pred_shift, ratio, RIG_RATIO_LO, RIG_RATIO_HI))
  if RIG_RATIO_LO <= ratio <= RIG_RATIO_HI:
    return "ORACLE_GREEN", detail, m
  side = ("SUPPRESSED (second eye lost reveal it should have — the wrong-cull "
          "signature)" if ratio < RIG_RATIO_LO else
          "AMPLIFIED (an eye is missing geometry wholesale)")
  return "ORACLE_RED", detail + " -> " + side, m


################################################################################
# THE TEMPORAL ORACLE (deliverable 3, leg (l)).
#
# WHAT THE ARTIFACT IS, RESHAPED. D2 established — from the code, not from its own null
# result — that the cull fan-out runs ONCE PER FRAME against ONE camera (scenegraph.cpp
# :204-223, scenegraph_render.cpp:236/260), so "one eye culled differently from the
# other" cannot happen. What CAN happen, and is what the owner actually saw, is the mono
# cull consuming a pyramid that is STALE (built from a frame whose depth was never
# submitted — e83f63345) or CORRUPTED (read while the previous frame's depth writes were
# still in flight — f11a0f21b). Both are per-FRAME hazards, and both express as the same
# observable: with the camera and every drawable HELD ABSOLUTELY STILL, geometry blinks.
#
# WHAT THE OBSERVABLE MUST BE. At a pinned rig tick nothing in this scene moves except
# the particles (whose region is excluded everywhere, as it is for every identity-class
# metric here). So the correct render is a sequence of IDENTICAL frames, and any
# departure is either a wrong cull or a defect of the same family. Two bounded readings,
# both REGIONAL (see RIG_TEMPORAL_AREA_FRAC for why frame-wide would be blind):
#
#   area_i     = px the flipper occupies against a CONTRASTING-POSE reference frame,
#                inside the ROI — the drawable-VISIBILITY reading. Spread across the
#                sequence must stay inside the area band.
#   ident_i    = mean|frame_i - frame_0| over the ROI, and separately over the terrain
#                band — the same ratified parity bar the A/B legs use.
#
# The reference is a frame of the OTHER pose (boundary sequence -> the occluded control;
# occluded sequence -> the cleared control), which makes the mask populated and the area
# reading meaningful in both directions. That same frame is what the in-run TEETH splice
# into the sequence: a spliced contrasting frame is a synthetic visibility flip of known
# large size, and it must read ORACLE_TEMPORAL_RED every run.
#
# Frames are streamed from disk one at a time (only frame_0 and the reference are held),
# because N takes x 2 eyes x 2 sequences x 2 slots of 1024^2 float arrays would otherwise
# be hundreds of MB resident purely to compute a few scalars.
################################################################################

def _loadFrame(path):
  """one capture as a float32 luma array, or None when it never landed."""
  from PIL import Image
  if not (path and os.path.isfile(path) and os.path.getsize(path) > 0):
    return None
  return np.asarray(Image.open(path).convert("RGB"), dtype=np.float32).mean(axis=2)


def _temporalOracle(paths, ref, pmask, bar_mean, splice=None):
  """paths: ordered [{"L": path, "R": path}, ...] — one pinned-pose take each, in
  capture order. ref: {"L": arr, "R": arr} at the CONTRASTING pose. splice: an extra
  {"L": arr, "R": arr} appended to the sequence (the teeth) — must force RED.

  Returns (status, detail, metrics) with status in ORACLE_TEMPORAL_GREEN /
  ORACLE_TEMPORAL_RED / ORACLE_TEMPORAL_BLIND (blind = a harness precondition failed,
  which is a loud FAIL and never a silent pass)."""
  m = {}
  if len(paths) < 2:
    return "ORACLE_TEMPORAL_BLIND", "sequence has %d takes (need >=2)" % len(paths), m
  if any(ref.get(e) is None for e in ("L", "R")):
    return "ORACLE_TEMPORAL_BLIND", "contrasting-pose reference missing", m

  first = {e: _loadFrame(paths[0][e]) for e in ("L", "R")}
  if any(first[e] is None for e in ("L", "R")):
    return "ORACLE_TEMPORAL_BLIND", "first take of the sequence never landed", m
  shape = first["L"].shape
  if any(f.shape != shape for f in (first["R"], ref["L"], ref["R"])):
    return "ORACLE_TEMPORAL_BLIND", "shape mismatch across sequence/reference", m
  keep = ~_rectMask(shape, pmask)

  # ROI: where the pose difference actually lives, dilated. Derived from THIS sequence's
  #  own first take against its reference, so the leg is self-contained and does not
  #  inherit a ROI from whatever the cull oracle happened to conclude.
  masks = {e: (np.abs(first[e] - ref[e]) > RIG_FLIP_THR) & keep for e in ("L", "R")}
  roi = _bboxOf(masks["L"] | masks["R"], RIG_ROI_MARGIN)
  m["roi"] = roi
  if roi is None:
    return ("ORACLE_TEMPORAL_BLIND",
            "no pose difference anywhere against the contrasting reference: the tick "
            "hold did not move the flipper, so there is no visibility to watch — a rig "
            "problem, not a stability verdict", m)
  roimask = _rectMask(shape, roi) & keep
  gndmask = _rectMask(shape, RIG_GROUND_BAND) & keep
  m["roi_px"], m["gnd_px"] = int(roimask.sum()), int(gndmask.sum())

  seq = [(i, {e: _loadFrame(paths[i][e]) for e in ("L", "R")})
         for i in range(1, len(paths))]
  if any(f[e] is None for _, f in seq for e in ("L", "R")):
    return "ORACLE_TEMPORAL_BLIND", "a take in the sequence never landed", m
  seq = [(0, first)] + seq
  if splice is not None:
    seq.append(("splice", splice))

  area, roi_id, gnd_id, full_id = {}, {}, {}, {}
  for eye in ("L", "R"):
    a, ri, gi, fi = [], [], [], []
    for _, fr in seq:
      f = fr[eye]
      a.append(int((((np.abs(f - ref[eye]) > RIG_FLIP_THR) & roimask)).sum()))
      d = np.abs(f - first[eye])
      ri.append(float(d[roimask].mean()) if m["roi_px"] else 0.0)
      gi.append(float(d[gndmask].mean()) if m["gnd_px"] else 0.0)
      fi.append(float(d[keep].mean()))
    area[eye], roi_id[eye], gnd_id[eye], full_id[eye] = a, ri, gi, fi

  # the area band: a fraction of the reference reveal, floored. Reported so the number
  #  the verdict turned on is in the log, not only in this file.
  base = float(np.mean([area["L"][0], area["R"][0]]))
  area_bar = max(RIG_TEMPORAL_AREA_MIN, RIG_TEMPORAL_AREA_FRAC * base)
  m["area_base_px"], m["area_bar_px"], m["ident_bar"] = base, area_bar, bar_mean
  if base < RIG_CTRL_AREA_MIN:
    return ("ORACLE_TEMPORAL_BLIND",
            "reference reveal is only %.0f px (need >=%d): too little visible geometry "
            "to detect its disappearance" % (base, RIG_CTRL_AREA_MIN), m)

  bad = []
  for eye in ("L", "R"):
    spread = max(area[eye]) - min(area[eye])
    m["area_spread_" + eye] = spread
    m["ident_roi_" + eye] = max(roi_id[eye])
    m["ident_gnd_" + eye] = max(gnd_id[eye])
    m["ident_full_" + eye] = max(full_id[eye])
    if spread > area_bar:
      bad.append("eye %s reveal area swings %d px across the hold (bar %.0f)"
                 % (eye, spread, area_bar))
    if max(roi_id[eye]) > bar_mean:
      bad.append("eye %s ROI identity %.4f > %.4f" % (eye, max(roi_id[eye]), bar_mean))
    if max(gnd_id[eye]) > bar_mean:
      bad.append("eye %s terrain-band identity %.4f > %.4f"
                 % (eye, max(gnd_id[eye]), bar_mean))

  detail = ("%d takes, ref reveal %.0fpx | area spread L=%d R=%d (bar %.0f) | identity "
            "max roi L=%.4f R=%.4f, ground L=%.4f R=%.4f, frame L=%.4f R=%.4f "
            "(bar %.4f, frame-wide is INFO)"
            % (len(seq), base, m["area_spread_L"], m["area_spread_R"], area_bar,
               m["ident_roi_L"], m["ident_roi_R"], m["ident_gnd_L"], m["ident_gnd_R"],
               m["ident_full_L"], m["ident_full_R"], bar_mean))
  if bad:
    return ("ORACLE_TEMPORAL_RED",
            detail + " -> FLICKER: " + "; ".join(bad)
            + " (geometry changed visibility while NOTHING in the scene moved — the "
              "stale/corrupted-pyramid signature)", m)
  return "ORACLE_TEMPORAL_GREEN", detail, m


################################################################################
# PYRAMID PROVENANCE (leg (m)) + the GUARD-REVERT surface (leg (n)).
#
# The guard being tested is fwdnode_impl_top.cpp:524-529: the HZB may only be built from
# depth whose recording frame is STRICTLY EARLIER than the current frame, because a
# same-frame seed (second eye / second compositor pass) has not reached the queue yet.
# The direct observable is the pyramid's own source frame, which no binding exposes; the
# filed engine ask is HZBBuilder::_sourceDepthFrame surfaced as Scene.hzb.sourceDepthFrame,
# to be read against the already-pybound ctx.frameIndex (= Context::GetTargetFrame, which
# is literally the guard's own comparand). Probed by attribute, never by version: an
# install either has the surface or it does not.
################################################################################

def _hzbProvenance(scene):
  """(present, source_depth_frame). Absent surface -> (False, None), which the leg turns
  into a VISIBLE skip sentinel — never a pass."""
  hzb = getattr(scene, "hzb", None)
  if hzb is None:
    return False, None
  src = getattr(hzb, "sourceDepthFrame", None)
  if src is None:
    return False, None
  return True, int(src)


################################################################################

class NodeSlot:
  """One output node under test: its scene, its output node, its captures, its
  frame timings. Slot identity is (name, kind) — kind picks the rendermodel
  preset, and NOTHING else in the harness branches on it."""

  def __init__(self, name, kind):
    self.name = name
    self.kind = kind
    self.preset = KIND_PRESET.get(kind)
    if self.preset is None:
      raise RuntimeError("unknown output-node kind '%s' (known: %s)"
                         % (kind, ",".join(sorted(KIND_PRESET))))
    self.scene = None
    self.outputnode = None
    self.layers = None
    self.layer1 = None
    self.caps = {}            # (take, eye) -> path
    # temporal takes live SEPARATE from caps: they are streamed from disk by the
    #  temporal leg, never bulk-loaded with the rest (see _temporalOracle's header).
    self.tcaps = {}           # tag -> ordered [{"L": path, "R": path}, ...]
    self.tframes = {}         # tag -> ordered [(gate_frame, ctx.frameIndex, hzb_src)]
    self.inflight = []        # CaptureAsync futures pending
    self.drain_ready = {}     # take -> bool (futures reported ready in budget)
    self.submits = []         # per-frame ctx.submitCount over the timed window (A10)
    self.hzb_surface = None   # provenance binding present? (None until probed)
    self.oea_fired = False
    self.oea_reachable = False
    self.env_ready = False
    self.env_ready_frame = 0
    self.env_ready_t = 0.0
    self.frame_dts = []
    self.timed_seconds = 0.0
    self.node_repr = ""

  def eyeRtGroup(self, left):
    return self.outputnode.downsampledEyeRtGroup(left)


class GateApp(ComponentizedApplication):

  def __init__(self, outdir):
    super().__init__()
    self._outdir = outdir
    self._frame = 0          # counts onBeginAssemble (== compositor frames)
    self._done = False
    self._t_prev = None
    self.cameralut = lev2.CameraDataLut()
    self._slots = [NodeSlot("A", NODE_A_KIND), NodeSlot("B", NODE_B_KIND)]
    self._active = None
    self._pending = None     # slot awaiting its lazy build (see header note 4)
    # per-slot step script. Slot A carries the extra take that MEASURES the bench
    # noise floor the parity bar references; slot B needs only one take.
    self._steps = {
      "A": [("settle", SETTLE_FRAMES), ("capture", "t1"), ("drain", "t1"),
            ("capture", "t2"), ("drain", "t2"), ("time", TIMED_SECONDS)],
      "B": [("settle", SETTLE_FRAMES), ("capture", "t1"), ("drain", "t1"),
            ("time", TIMED_SECONDS)],
    }
    self._rig_tick = 0
    if RIG:
      # the rig's takes: the two CONTROLS the reference rig defines (unambiguously
      #  occluded / unambiguously clear) bracket the BOUNDARY take. t1/t2 keep their
      #  D1 meaning — main take and its same-config repeat (the noise reference) —
      #  and are both frozen at the boundary tick, so every downstream leg (parity,
      #  bars, swap tooth) reads the boundary pose without knowing the rig exists.
      # D3: each control tick also carries a TEMPORAL HOLD — RIG_TEMPORAL_N takes at the
      #  UNCHANGED pinned pose, back to back, spanning many HZB build+consume cycles.
      #  Placed immediately after that tick's own control capture so the pose is already
      #  held and no extra tick step is spent. Both sequences run on BOTH slots: the leg
      #  is a property of the NODE, and when slot B becomes the SPVR node it is the one
      #  that most needs watching.
      n_temporal = REVERT_CHILD_N if REVERT_CHILD else RIG_TEMPORAL_N

      def _hold(tag):
        s = []
        for i in range(n_temporal):
          s += [("tcap", (tag, i)), ("tdrain", (tag, i))]
        return s

      def _rigsteps(noise):
        s = [("settle", SETTLE_FRAMES),
             ("tick", RIG_TICK_OCC), ("capture", "occ"), ("drain", "occ")]
        s += _hold("occ")
        s += [("tick", RIG_TICK_BND), ("capture", "t1"), ("drain", "t1")]
        if noise:
          s += [("capture", "t2"), ("drain", "t2")]
        s += _hold("bnd")
        s += [("tick", RIG_TICK_CLR), ("capture", "clr"), ("drain", "clr"),
              ("time", TIMED_SECONDS)]
        return s
      self._steps = {"A": _rigsteps(True), "B": _rigsteps(False)}
    self._step_i = 0
    self._step_f0 = 0
    self._step_t0 = 0.0
    self.createEzApp(width=640, height=480, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ctx = ctx
    self.ezapp.topWidget.enableUiDraw()

    # NoVR device: nonzero IPD -> genuine per-eye disparity. Root/world placement is
    #  left at identity (device.camera == "") and the view comes from a static hmd
    #  pose -> the two eyes are that pose, IPD-separated. Deterministic, no DB camera
    #  lookup dependency. ONE device drives BOTH slots — identical eye poses are a
    #  precondition of an eye-for-eye parity diff.
    self.vrdev = lev2.orkidvr.novr_device()
    self.vrdev.width  = EYE_W
    self.vrdev.height = EYE_H
    self.vrdev.FOVD   = 90.0
    self.vrdev.IPD    = IPD_M
    self.vrdev.near   = 0.1
    self.vrdev.far    = 1e4
    self.vrdev.setPoseMatrix("hmd", mtx4.lookAt(HEAD_EYE, HEAD_TGT, HEAD_UP))

    if RIG:
      # THE CULL FAN-OUT'S CAMERA. Scene::preRender — the per-view hook that runs the
      #  GPU culls (terrain chunk cull, instance cull) and hands them the HZB — is
      #  reached from _renderIMPL only if a camera resolves: in VR from the tracked
      #  head, otherwise from the compositor's camera name falling back to "spawncam"
      #  (scenegraph_render.cpp). A NoVrDevice is neither tracked nor XR-presenting, so
      #  without a "spawncam" in the lut the fan-out NEVER RUNS and the scene's only
      #  HZB consumer is inert — a cull oracle over a dead cull path would be a silent
      #  green. Registered for the rig path only; the D1 scene has no cull consumer.
      from lev2utils.cameras import setupUiCameraX
      self._cullcam, self._cullui = setupUiCameraX(
          near=0.1, far=1e4, fov_deg=90.0, cameralut=self.cameralut,
          camname="spawncam", eye=HEAD_EYE, tgt=HEAD_TGT, up=HEAD_UP)

    # shared assets: loaded once, instanced per scene.
    self._ball_model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self._white_tex = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    self._norm_tex  = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

    # only slot A is built here; slot B is built at the switch (header note 4).
    self._buildSlot(ctx, self._slots[0])
    self._active = self._slots[0]
    self.scene = self._active.scene
    self._step_i = 0
    self._step_f0 = 0
    self._step_t0 = time.perf_counter()
    print("[spvrgate] slots: %s" % ", ".join(
        "%s=%s(%s)" % (s.name, s.kind, s.preset) for s in self._slots), flush=True)

  ##############################################################

  def _buildSlot(self, ctx, slot):
    vars = VarMap()
    vars.SkyboxIntensity = 1.0
    vars.DiffuseIntensity = 1.0
    vars.SpecularIntensity = 1.0
    vars.AmbientLevel = vec3(0.12)
    vars.SkyboxTexPathStr = "<ork_envmaps2>/cold4k.xir"

    # renders through the scene's OWN output node (see header note 1). layer_name is
    #  passed explicitly so an unknown-to-lev2utils preset (the future SPVR one) does
    #  not trip createSceneGraph's rendermodel->layer table.
    createSceneGraph(app=self, rendermodel=slot.preset, vars=vars,
                     layer_name="std_forward")
    slot.scene = self.scene
    slot.outputnode = self.scene.compositoroutputnode
    slot.layers = list(self.std_layers)
    slot.layer1 = self.layer1
    slot.node_repr = repr(slot.outputnode)

    if hasattr(self.scene, "pbr_common") and self.scene.pbr_common is not None:
      self.scene.pbr_common.enable_skybox = True

    # onEndAssemble fires after BOTH eyes assemble+downsample. PROVE the pybound
    #  hook can reach the per-eye buffers from Python (reachability only — the
    #  readback happens at onBeginAssemble of the next frame, see header note 3).
    #  Bound on the OutputCompositingNode BASE class, so this is node-kind agnostic.
    def _on_end_assemble(cdd, slot=slot):
      slot.oea_fired = True
      rtgL = slot.eyeRtGroup(True)
      rtgR = slot.eyeRtGroup(False)
      slot.oea_reachable = bool(
          rtgL is not None and rtgR is not None
          and rtgL.numBuffers > 0 and rtgR.numBuffers > 0)
    slot.outputnode.onEndAssemble(_on_end_assemble)

    # onBeginAssemble drives the frame machine + the safe readback of the PRIOR
    #  frame's completed eye buffers (header note 3). Only the ACTIVE slot renders,
    #  so only the active slot's hook fires.
    slot.outputnode.onBeginAssemble(lambda cdd, slot=slot: self._onBeginAssemble(slot))

    self._buildSceneContent(ctx, slot)
    self.scene.lightingmanager.gpuInit(ctx)

  ##############################################################
  # THE SCENE SEAM. Everything below the capture line is scene-agnostic: it only ever
  # reads downsampledEyeRtGroup(). Either body satisfies the same slot contract at the
  # same call site, with no change to capture, parity, teeth or timing. Both slots MUST
  # get identical content, or the parity leg measures the scene instead of the node.
  ##############################################################

  def _buildSceneContent(self, ctx, slot):
    if RIG:
      return self._buildRigContent(ctx, slot)
    # ground grid
    grid_data = createGridData(extent=100.0)
    grid_data.shader_suffix = "_V4"
    grid_node = slot.layer1.createDrawableNodeFromData("grid", grid_data)
    grid_node.sortkey = 1
    slot.grid_data = grid_data

    # near balls with strong horizontal parallax + a couple farther for depth range.
    balls = [
      (vec3(-1.2, 1.0,  2.6), vec4(0.9, 0.2, 0.2, 1), 0.45),
      (vec3( 1.1, 1.2,  2.2), vec4(0.2, 0.8, 0.3, 1), 0.40),
      (vec3( 0.0, 0.8,  1.4), vec4(0.2, 0.4, 0.95, 1), 0.35),
      (vec3(-0.6, 1.6, -1.0), vec4(0.9, 0.8, 0.2, 1), 0.55),
      (vec3( 1.6, 1.0, -2.4), vec4(0.8, 0.4, 0.9, 1), 0.60),
    ]
    for i, (pos, col, scl) in enumerate(balls):
      self._make_ball(ctx, slot, "ball%d" % i, pos, col, scl)

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 8.0
    sun.lookAt(vec3(-8, 12, 6), vec3(0, 0, 0), vec3(0, 1, 0))
    slot.sun = sun
    slot.sun_node = slot.layer1.createLightNode("sun", sun)

  def _make_ball(self, ctx, slot, name, pos, color, scale):
    drawable = self._ball_model.createDrawable()
    modelinst = drawable.modelinst
    for subinst in modelinst.submeshinsts:
      mtl = subinst.material.clone()
      mtl.assignImages(ctx, color=self._white_tex, normal=self._norm_tex,
                       mtlruf=self._white_tex, doConform=True)
      mtl.baseColor = color
      mtl.metallicFactor = 0.0
      mtl.roughnessFactor = 0.5
      subinst.overrideMaterial(mtl)
    node = slot.scene.createDrawableNodeOnLayers(slot.layers, name, drawable)
    node.worldTransform.translation = pos
    node.worldTransform.scale = scale
    return node

  ##############################################################
  # A4 RIG CONTENT — the seam's other body. Three material families in one scene
  # (A2): PBR models (occluder + flipper), TERRAIN, PARTICLES. Lit by the skybox
  # IBL alone, exactly as the reference rig: a directional light would add SHADOW
  # MAPS, a second per-eye code path, into the cull evidence.
  ##############################################################

  def _buildRigContent(self, ctx, slot):
    self._buildRigTerrain(ctx, slot)

    # occluder + flipper: ONE box model at two scales (the reference rig's route —
    #  HypermeshComponent ignores entity transforms, so the model-component path is
    #  the only one that places two instances apart).
    if not hasattr(self, "_box_model"):
      self._box_model = lev2.XgmModel(RIG_BOX_MODEL)
    slot.box_node = self._makeRigBox(ctx, slot, "occluder", RIG_BOX_POS, RIG_BOX_SCALE)
    slot.flip_node = self._makeRigBox(ctx, slot, "flipper",
                                      RIG_FLIP_ORIGIN, RIG_FLIP_SCALE)
    self._applyRigTick(slot)

    self._buildRigParticles(ctx, slot)

  def _makeRigBox(self, ctx, slot, name, pos, scale):
    drawable = self._box_model.createDrawable()
    node = slot.scene.createDrawableNodeOnLayers(slot.layers, name, drawable)
    node.worldTransform.translation = pos
    node.worldTransform.scale = scale
    return node

  def _applyRigTick(self, slot):
    """the flipper's pose is a pure function of the tick — never of wall clock. Called
    from the update thread each frame; idempotent."""
    node = getattr(slot, "flip_node", None)
    if node is None:
      return
    tick = RIG_FREEZE_TICK if RIG_FREEZE_TICK is not None else self._rig_tick
    node.worldTransform.translation = RIG_FLIP_ORIGIN + RIG_SLIDE_AXIS * _rigSlide(tick)

  ##############################################################

  def _buildRigTerrain(self, ctx, slot):
    """A2 family (ii). The GPU-chunked terrain: heightfield SSBO + a generated ptex3d
    material with the SSBO-pull vertex side delegated to TerrainChunkVertexSource. This
    is also the scene's ONLY HZB-cull consumer — the per-chunk occlusion test in the
    cull compute is what a wrong HZB seeding can actually mis-cull."""
    from ork.hypergraph.ecs.scene.assets import Ptex3d as Ptex3dAsset
    from ork.hypergraph.assets.materials.terrain.solid import Solid
    from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource

    vs = TerrainChunkVertexSource(dim=RIG_TERRA_DIM, extent_m=RIG_TERRA_EXTENT,
                                  chunk=RIG_TERRA_CHUNK)
    if not hasattr(self, "_terra_heights"):
      self._terra_heights = self._rigHeights()
    heights = self._terra_heights

    mat_asset = Ptex3dAsset(dsl_class=Solid, vertex_source=vs,
                            albedo=vec3(0.33, 0.36, 0.30), roughness=0.9)
    mat_asset._ctx = ctx
    pbrmat = mat_asset.as_gfx_material
    fs = pbrmat.freestyle
    sif = fs.storage("sif_ptex_vtx")

    FXI = ctx.FXI
    ssbo = FXI.createShaderStorageBufferWithLength(vs.TOTAL)
    FXI.copyDataIntoShaderStorageBuffer(heights, ssbo, vs.HEIGHTS_OFF)
    # u_dim: the cull derives its chunk count from it; left 0 every chunk culls
    #  (invisible terrain). u_ybounds: the frustum cull's CONSERVATIVE global vertical
    #  box — heights are true metres, so an unwritten [0,0] box would reject every
    #  chunk of a terrain authored below y=0 (this rig's ground sits at RIG_TERRA_Y).
    vs.upload_dim(FXI, ssbo)
    FXI.copyDataIntoShaderStorageBuffer(
        np.asarray([float(heights.min()) - 1.0, float(heights.max()) + 1.0],
                   dtype=np.float32), ssbo, vs.YB_OFF)

    cdd = lev2.ComputeDrawableData()
    cdd.material = pbrmat
    cdd.addGraphicsStorage(sif, ssbo)
    cdd.setCameraParams(ssbo, vs.CAM_OFF)
    for (name, gx, gy, gz) in vs.compute_passes():
      cdd.addComputePass(fs.computeShader(name), [(sif, ssbo)], gx, gy, gz)
    cdd.setIndirect(args=ssbo, args_offset=vs.ARGS_OFF, primtype=tokens.TRIANGLES)

    slot.terra_ssbo = ssbo          # keep alive
    slot.terra_mat = pbrmat
    slot.terra_vs = vs
    slot.terra_node = slot.layer1.createDrawableNodeFromData("terrain", cdd)

  def _rigHeights(self):
    """a fixed, closed-form field — no noise DSL, no seed to drift. Metres, absolute
    (the chunk terrain's heights[] is true world Y), biased to the rig's ground plane."""
    n = RIG_TERRA_DIM
    xs = (np.arange(n, dtype=np.float64) / n - 0.5)
    X, Z = np.meshgrid(xs, xs)
    # wavelengths ~3m and ~1.5m at this extent (13 and 6 texels of the 256 grid — well
    # sampled, no aliasing) so the ground carries shading DETAIL, the only thing a
    # parallax metric can see on an untextured matte surface.
    h = (0.50 + 0.28 * np.sin(X * 120.0) * np.cos(Z * 95.0)
              + 0.16 * np.sin((X + Z) * 210.0))
    h = np.clip(h, 0.0, 1.0) * RIG_TERRA_RELIEF + RIG_TERRA_Y
    return h.astype(np.float32).reshape(-1)

  ##############################################################

  def _buildRigParticles(self, ctx, slot):
    """A2 family (iii): renderer_materials.cpp has NO stereo branch at its MatMVP
    sites, so a node that misses that binding renders IDENTICAL images into both eye
    layers here — zero parallax, invisible to validation and to a whole-frame delta.
    Placed straddling the occlusion boundary so one region carries both evidences.
    NOT frame-deterministic (dt-driven, no seed exposed): its ROI is measured and
    taken out of the identity-class metrics — see the particle-mask block."""
    from orkengine.core import dataflow as dflow
    ptc = lev2.particles
    gd = dflow.GraphData.createShared()
    pool = gd.create("POOL", ptc.Pool)
    emit = gd.create("EMITN", ptc.NozzleEmitter)
    spri = gd.create("SPRI", ptc.SpriteRenderer)
    gd.connect(emit.inputs.pool, pool.outputs.pool)
    gd.connect(spri.inputs.pool, emit.outputs.pool)
    pool.pool_size = RIG_PTC_POOL
    # a COMPACT cloud: its region is excluded from the identity metrics, so every pixel
    #  it covers is a pixel the parity leg cannot see. Sized to stay a diagnostic
    #  region, not a hole in the frame.
    emit.inputs.LifeSpan = 3.0
    emit.inputs.EmissionRate = 400
    emit.inputs.EmissionVelocity = 0.12
    emit.inputs.DispersionAngle = 50.0
    emit.inputs.Offset = vec3(0, 0, 0)
    spri.inputs.Size = 0.12 * (RIG_D_FLIP / 18.0)
    spri.inputs.GradientIntensity = 1.0

    mtl = ptc.GradientMaterial.createShared()
    mtl.blending = tokens.ADDITIVE
    mtl.depthtest = tokens.LEQUALS
    # NOT saturated: a clipped-white cloud reads 255 in both eyes and its regional
    #  eye-delta collapses to 0 for a reason that has nothing to do with stereo.
    mtl.colorIntensity = 0.8
    mtl.gradient.setColorStops({0.0: vec4(1, 1, 1, 1),
                                0.5: vec4(1, 0.7, 0.3, 1),
                                1.0: vec4(0, 0, 0, 1)})
    mtl.modulation_texture = lev2.Texture.load("src://effect_textures/knob2")
    spri.material = mtl

    dd = lev2.ParticlesDrawableData()
    dd.graphdata = gd
    node = slot.layer1.createDrawableNode("particles", dd.createDrawable())
    node.worldTransform.translation = RIG_PTC_POS
    slot.ptc_graph = gd
    slot.ptc_data = dd
    slot.ptc_mtl = mtl
    slot.ptc_node = node

  ##############################################################

  def _onGpuUpdate(self, ctx):
    # GPU thread, before the frame, no render pass open — the one safe point to
    #  build the next slot's scene. createSceneGraph rebinds the app's draw
    #  callback to the new scene, which IS the slot switch (header note 4).
    slot = self._pending
    if slot is None or self._done or self._shutting_down:
      return
    self._pending = None
    self._buildSlot(ctx, slot)
    self._active = slot
    self.scene = slot.scene
    self._step_i = 0
    self._step_f0 = self._frame
    self._step_t0 = time.perf_counter()
    self._t_prev = None
    print("[spvrgate] slot %s active (%s / %s) at frame %d"
          % (slot.name, slot.kind, slot.preset, self._frame), flush=True)

  def _onUpdate(self, updinfo):
    if self._shutting_down:
      return
    slot = self._active
    if slot is None:
      return
    if RIG:
      self._applyRigTick(slot)
    try:
      slot.scene.updateScene(self.cameralut)
    except RuntimeError:
      self._shutting_down = True

  ##############################################################
  # capture
  ##############################################################

  def _capPath(self, slot, take, eye):
    # slot A / take t1 keeps the ORIGINAL artifact names — downstream consumers of
    #  this gate's images (and the pre-SPVR checks) see no rename.
    if RIG:
      return os.path.join(self._outdir, "rig_node%s_%s_%s.png" % (slot.name, take, eye))
    if slot.name == "A" and take == "t1":
      return os.path.join(self._outdir, "dmvr_eye_%s.png" % eye)
    return os.path.join(self._outdir, "node%s_%s_%s.png" % (slot.name, take, eye))

  def _capture(self, slot, take):
    left_rtg = slot.eyeRtGroup(True)
    # negative/detection run captures the LEFT eye into BOTH outputs.
    right_rtg = left_rtg if SAME_EYE else slot.eyeRtGroup(False)
    slot.inflight = []
    for eye, rtg in (("L", left_rtg), ("R", right_rtg)):
      path = self._capPath(slot, take, eye)
      fut = self.ctx.FBI.captureToFile(rtg.buffer(0), CorePath(path))
      slot.caps[(take, eye)] = path
      slot.inflight.append(fut)

  def _tcapture(self, slot, tag, idx):
    """one take of a temporal hold: the same capture, filed OUTSIDE slot.caps, and
    stamped with the ENGINE's own frame index so the leg can state the frame span it
    actually covered instead of asserting one. The provenance probe is sampled here
    too — same instant, same frame, so the two legs describe one timeline."""
    left_rtg = slot.eyeRtGroup(True)
    right_rtg = left_rtg if SAME_EYE else slot.eyeRtGroup(False)
    slot.inflight = []
    entry = {}
    for eye, rtg in (("L", left_rtg), ("R", right_rtg)):
      path = os.path.join(self._outdir, "rig_node%s_tmp_%s_%02d_%s.png"
                          % (slot.name, tag, idx, eye))
      fut = self.ctx.FBI.captureToFile(rtg.buffer(0), CorePath(path))
      entry[eye] = path
      slot.inflight.append(fut)
    slot.tcaps.setdefault(tag, []).append(entry)
    present, src = _hzbProvenance(slot.scene)
    slot.hzb_surface = present
    slot.tframes.setdefault(tag, []).append(
        (self._frame, int(self.ctx.frameIndex) if hasattr(self.ctx, "frameIndex")
         else None, src))

  ##############################################################
  # frame machine: a per-slot step script, advanced from onBeginAssemble.
  ##############################################################

  def _advance(self):
    self._step_i += 1
    self._step_f0 = self._frame
    self._step_t0 = time.perf_counter()
    self._t_prev = None
    steps = self._steps[self._active.name]
    if self._step_i < len(steps):
      return
    # slot exhausted -> queue the next slot's build (it becomes active there), or
    #  emit the verdict. The current scene keeps rendering idle frames until the
    #  build lands, which is one GPU update away.
    idx = self._slots.index(self._active) + 1
    if idx < len(self._slots):
      self._pending = self._slots[idx]
    else:
      self._verdict()
      self._done = True
      self.ezapp.signalExit()

  def _bail(self, msg):
    """a precondition the harness cannot satisfy: verdict FALSE, before teardown."""
    from ork.testing import verdict
    print("SPVRGATE_LEG %-22s %-11s %s" % ("precondition", "FAIL", msg), flush=True)
    self._verdict_code = verdict(False, "spvr gate harness | %s" % msg)
    self._done = True
    self.ezapp.signalExit()

  def _onBeginAssemble(self, slot):
    # fires at the START of every compositor frame of the ACTIVE slot; the prior
    #  frame's eye buffers are complete here and safe to read back.
    self._frame += 1
    if self._done or slot is not self._active:
      return
    steps = self._steps[slot.name]
    if self._step_i >= len(steps):
      return          # slot finished; idling until its successor is built
    kind, arg = steps[self._step_i]
    now = time.perf_counter()

    if kind == "settle":
      # RESIDENCY FIRST, then frames, then a wall-time floor. The baked env map
      #  streams in asynchronously while this offscreen context runs frames at
      #  full tilt, so a pure frame count would let slot A capture pre-residency
      #  and slot B post-residency — a parity difference caused by the BENCH, not
      #  by the nodes. Missing residency inside the deadline fails LOUD.
      if not slot.env_ready:
        maps = None
        pbrc = getattr(slot.scene, "pbr_common", None)
        if pbrc is not None:
          maps = pbrc.RadianceMaps
        if (maps is None) or (maps.specular is None):
          if (now - self._step_t0) > ENV_WAIT_SECONDS:
            self._bail("slot %s (%s): baked env map never became resident in %.0fs"
                       % (slot.name, slot.kind, ENV_WAIT_SECONDS))
          return
        slot.env_ready = True
        slot.env_ready_frame = self._frame
        slot.env_ready_t = now
        return
      if ((self._frame - slot.env_ready_frame) >= arg
          and (now - slot.env_ready_t) >= SETTLE_SECONDS_MIN):
        self._advance()

    elif kind == "tick":
      # FREEZE the rig at a named tick, then hold. Both slots and both eyes must be
      #  compared at the SAME pose, so the tick is pinned (never free-running) for the
      #  whole capture. The hold covers the update-thread -> drawable-buffer -> render
      #  hand-off; the oracle independently proves the pose actually moved (its
      #  controls must differ), so a too-short hold cannot pass silently.
      self._rig_tick = arg
      if ((self._frame - self._step_f0) >= RIG_TICK_HOLD_FRAMES
          and (now - self._step_t0) >= RIG_TICK_HOLD_SECONDS):
        self._advance()

    elif kind == "capture":
      self._capture(slot, arg)
      self._advance()

    elif kind == "drain":
      ready = all(bool(f.is_ready) for f in slot.inflight)
      spent = self._frame - self._step_f0
      if ready or spent >= WRITE_FRAMES:
        slot.drain_ready[arg] = ready
        slot.inflight = []
        self._advance()

    elif kind == "tcap":
      self._tcapture(slot, arg[0], arg[1])
      self._advance()

    elif kind == "tdrain":
      # a torn (undrained) temporal take would read as a visibility flip that no cull
      #  produced, so it is recorded per take and gated by the leg, never smoothed over.
      ready = all(bool(f.is_ready) for f in slot.inflight)
      spent = self._frame - self._step_f0
      if ready or spent >= WRITE_FRAMES:
        slot.drain_ready["tmp_%s_%02d" % arg] = ready
        slot.inflight = []
        self._advance()

    elif kind == "time":
      if self._t_prev is not None:
        slot.frame_dts.append(now - self._t_prev)
        # A10: the submit count for the frame that just ended, sampled in lockstep with
        #  its own interval so the two series describe the SAME frames.
        sc = _submitCount(self.ctx)
        if sc is not None:
          slot.submits.append(sc)
      self._t_prev = now
      if (now - self._step_t0) >= arg:
        slot.timed_seconds = now - self._step_t0
        self._advance()

  ##############################################################
  # verdict — every leg emits its OWN sentinel line before teardown.
  ##############################################################

  def _leg(self, name, status, detail):
    self._legs.append((name, status, detail))
    print("SPVRGATE_LEG %-22s %-11s %s" % (name, status, detail), flush=True)
    return status

  def _loadCaps(self, slot):
    return {k: _loadFrame(p) for k, p in slot.caps.items()}

  def _guardRevertLeg(self, surface):
    """(n): prove the oracle can SEE the defect by putting the defect back.

    ORKID_HZB_ALLOW_SAMEFRAME is read inside C++, so python cannot probe for it — the
    leg is gated on the PROVENANCE surface instead, because the two engine asks were
    filed together and land together (header (n)). When armed, this re-execs the gate
    with the knob set and a short hold, and REQUIRES the child's temporal or provenance
    leg to read RED. Knob set and still green = ORACLE_BLIND and the run fails: either
    the revert did not bite or this oracle cannot see what it exists to see, and both
    of those are findings, not passes."""
    if REVERT_CHILD:
      self._leg("guard_revert", "SKIP",
                "this process IS the revert child (%s=%s) — it does not spawn a "
                "grandchild" % (HZB_REVERT_ENV, os.environ.get(HZB_REVERT_ENV)))
      return True
    if not surface:
      self._leg("guard_revert", "SKIP",
                "LEG_GUARDREVERT=SKIP pending engine knob — gated on the same probe as "
                "leg (m) (the %s knob is C++-side env and not probeable from python; "
                "the two engine asks land together). NOT a pass: leg (l)'s ability to "
                "read RED is proven only by its in-run splice teeth meanwhile"
                % HZB_REVERT_ENV)
      return True

    import subprocess
    env = dict(os.environ)
    env[HZB_REVERT_ENV] = "1"
    env["ORKID_SPVR_GATE_REVERTCHILD"] = "1"
    env["ORKID_SPVR_GATE_RIG"] = "1"
    outdir = os.path.join(self._outdir, "guard_revert_child")
    os.makedirs(outdir, exist_ok=True)
    try:
      r = subprocess.run([sys.executable, os.path.abspath(__file__), outdir],
                         env=env, capture_output=True, text=True,
                         timeout=REVERT_CHILD_TIMEOUT)
    except Exception as e:                       # noqa: BLE001 — reported, never raised
      self._leg("guard_revert", "FAIL",
                "revert child could not be run (%s): the negative control is the only "
                "proof this oracle is not blind, so an unrunnable child is a failure" % e)
      return False
    out = (r.stdout or "") + (r.stderr or "")
    reds = [ln for ln in out.splitlines()
            if "ORACLE_TEMPORAL_RED" in ln and ln.startswith("SPVRGATE_LEG temporal_")
            and "teeth" not in ln]
    prov_red = [ln for ln in out.splitlines()
                if ln.startswith("SPVRGATE_LEG provenance") and "ORACLE_RED" in ln]
    fired = bool(reds or prov_red)
    self._leg("guard_revert", "PASS" if fired else "ORACLE_BLIND",
              "%s=1 child rc=%d: %d temporal RED + %d provenance RED (>=1 required; "
              "green under a reverted guard means the revert did not bite or the "
              "oracle is blind)%s"
              % (HZB_REVERT_ENV, r.returncode, len(reds), len(prov_red),
                 "" if fired else " | child tail: "
                 + " / ".join(out.strip().splitlines()[-3:])))
    return fired

  def _verdict(self):
    from ork.testing import verdict
    self._legs = []
    ok = True
    arrs = {}

    print("=== SPVR gate harness: %s (A=%s B=%s) ==="
          % ("SELF-PARITY BOOTSTRAP" if SELF_PARITY else "CROSS-NODE PARITY",
             NODE_A_KIND, NODE_B_KIND), flush=True)
    self._leg("validation_env", "INFO",
              "ORKID_VULKAN_VALIDATE=%s"
              % os.environ.get("ORKID_VULKAN_VALIDATE", "<unset>"))
    self._leg("base_state", "INFO", _baseState())
    self._leg("scene", "INFO",
              "%s eye=%dx%d" % ("A4 occluder rig (box+flipper+particles+terrain)"
                                if RIG else "D1 tiny scene (grid+balls)", EYE_W, EYE_H))

    # ---- per-slot: capture health, stereo, hook reachability -----------------
    for slot in self._slots:
      arrs[slot.name] = self._loadCaps(slot)
      a = arrs[slot.name]
      self._leg("node_%s_identity" % slot.name, "INFO",
                "kind=%s preset=%s node=%s" % (slot.kind, slot.preset, slot.node_repr))

      det, this_ok = [], True
      for eye in ("L", "R"):
        arr = a.get(("t1", eye))
        if arr is None:
          det.append("eye=%s MISSING" % eye); this_ok = False; continue
        m = float(arr.mean()); rng = float(arr.max() - arr.min())
        eye_ok = (m >= MEAN_FLOOR) and (rng >= RANGE_FLOOR)
        this_ok = this_ok and eye_ok
        det.append("eye=%s mean=%.2f range=%.1f" % (eye, m, rng))
      ok = ok and this_ok
      self._leg("capture_%s" % slot.name, "PASS" if this_ok else "FAIL",
                "%s (mean>=%.1f range>=%.1f) drained=%s"
                % (" ".join(det), MEAN_FLOOR, RANGE_FLOOR, slot.drain_ready))

      L, R = a.get(("t1", "L")), a.get(("t1", "R"))
      if L is not None and R is not None and L.shape == R.shape:
        delta, d_ok = _eyesDiffer(L, R)
        ok = ok and d_ok
        self._leg("eyes_differ_%s" % slot.name, "PASS" if d_ok else "FAIL",
                  "eyedelta=%.3f (floor=%.2f ceil=%.1f)"
                  % (delta, DELTA_FLOOR, DELTA_CEIL))
      else:
        ok = False
        self._leg("eyes_differ_%s" % slot.name, "FAIL", "UNCOMPUTABLE")

      oea_ok = slot.oea_fired and slot.oea_reachable
      ok = ok and oea_ok
      self._leg("assemble_hooks_%s" % slot.name, "PASS" if oea_ok else "FAIL",
                "onEndAssemble fired=%d reachable=%d"
                % (int(slot.oea_fired), int(slot.oea_reachable)))

    A, B = arrs["A"], arrs["B"]

    # ---- PARTICLE ROI (rig only) --------------------------------------------
    # The particle cloud advances on the engine's dt with no seed or fixed-step knob
    #  exposed, so it is NOT reproducible take-to-take — measured on the reference rig
    #  as the ONLY region that differs between two same-pose renders. It is therefore
    #  taken OUT of the identity-class metrics (noise, parity, swap tooth, oracle
    #  areas) and gated instead by a NON-IDENTITY metric that particle jitter cannot
    #  spoof: regional eyes-differ, L vs R of the SAME captured frame (both eye buffers
    #  are read back in one hook call, so they share one particle state exactly).
    # The ROI is MEASURED, not declared: the bbox of the slot-A same-config repeat's
    #  differing pixels, dilated. Consequence, stated: the residual noise floor is then
    #  ~0 by construction and the parity bar resolves to its EPS floors — the same
    #  regime D1 measured on the bit-deterministic tiny scene.
    pmask = None
    pbox = None
    if RIG:
      pmask = _rigParticleRect()
      jit = None
      for eye in ("L", "R"):
        x, y = A.get(("t1", eye)), A.get(("t2", eye))
        if x is not None and y is not None and x.shape == y.shape:
          # sub-LSB threshold: this looks for ANY non-reproducibility, not for a
          #  visible change, so it maps the cloud's real footprint.
          d = np.abs(x - y) > 1.0
          jit = d if jit is None else (jit | d)
      jbox = _bboxOf(jit) if jit is not None else None
      pbox = jbox
      frac = 100.0 * _rectArea(pmask) / float(EYE_W * EYE_H)
      if jbox is None:
        ok = False
        self._leg("particle_roi", "FAIL",
                  "no take-to-take difference anywhere: either the particles are not "
                  "rendering (rig broken) or they became frame-deterministic (the mask "
                  "premise changed) — both need a human, neither is a pass")
      else:
        inside = (jbox[0] >= pmask[0] and jbox[1] >= pmask[1]
                  and jbox[2] <= pmask[2] and jbox[3] <= pmask[3])
        good = inside and frac < 25.0
        ok = ok and good
        self._leg("particle_roi", "PASS" if good else "FAIL",
                  "declared rect=%s (%.1f%% of frame, must be <25%%); measured "
                  "non-reproducible bbox=%s %s the declaration"
                  % (str(pmask), frac, str(jbox),
                     "INSIDE" if inside else "ESCAPED"))

    def _pm(x):
      return _zeroRect(x, pmask) if (RIG and x is not None) else x

    # ---- noise floor: slot A, same config, two takes -------------------------
    noise = None
    n_per_eye = {}
    for eye in ("L", "R"):
      x, y = A.get(("t1", eye)), A.get(("t2", eye))
      if x is not None and y is not None and x.shape == y.shape:
        n_per_eye[eye] = _parityMetrics(_pm(x), _pm(y))
    if len(n_per_eye) == 2:
      noise = {k: max(n_per_eye["L"][k], n_per_eye["R"][k])
               for k in ("mean", "max", "ssim_worst")}
      self._leg("noise_floor", "PASS",
                "repeat-render of node A: mean=%.4f max=%.3f ssim_worst=%.5f"
                % (noise["mean"], noise["max"], noise["ssim_worst"]))
    else:
      ok = False
      self._leg("noise_floor", "FAIL",
                "repeat takes missing -> parity bar has no reference")

    bars = _parityBars(noise)
    self._leg("parity_bars", "INFO",
              "mean<=%.4f max<=%.3f ssim_worst<=%.5f (noise x%.1f, eps %.3f/%.1f/%.4f)"
              % (bars["mean"], bars["max"], bars["ssim_worst"], NOISE_SLACK,
                 EPS_PARITY_MEAN, EPS_PARITY_MAX, EPS_PARITY_SSIM))

    # ---- EYE-FOR-EYE PARITY: A.L vs B.L, A.R vs B.R --------------------------
    straight = {}
    for eye in ("L", "R"):
      x, y = A.get(("t1", eye)), B.get(("t1", eye))
      if x is None or y is None or x.shape != y.shape:
        ok = False
        self._leg("parity_%s%s" % (eye, eye), "FAIL", "UNCOMPUTABLE (missing/shape)")
        continue
      m = _parityMetrics(_pm(x), _pm(y))
      straight[eye] = m
      p_ok = _parityWithin(m, bars)
      ok = ok and p_ok
      self._leg("parity_%s%s" % (eye, eye), "PASS" if p_ok else "FAIL",
                "mean=%.4f max=%.3f ssim_worst=%.5f" % (m["mean"], m["max"], m["ssim_worst"]))

    # ---- TEETH ---------------------------------------------------------------
    # (a) SAMEEYE: the eyes-differ oracle fed one eye twice must FAIL.
    ref = A.get(("t1", "L"))
    if ref is None:
      ok = False
      self._leg("teeth_sameeye", "FAIL", "no reference frame")
    else:
      delta, d_ok = _eyesDiffer(ref, ref)
      tooth_ok = not d_ok
      ok = ok and tooth_ok
      self._leg("teeth_sameeye", "PASS" if tooth_ok else "FAIL",
                "oracle on (L,L) delta=%.3f -> %s (must be FAIL)"
                % (delta, "PASS" if d_ok else "FAIL"))

    # (b) L/R SWAP: parity of a swapped node pair must FAIL the same bar.
    if SAME_EYE:
      self._leg("teeth_swap", "SKIP",
                "SAMEEYE run: L==R by construction, swap is not discriminable")
    else:
      swapped = []
      for eye, other in (("L", "R"), ("R", "L")):
        x, y = A.get(("t1", eye)), B.get(("t1", other))
        if x is None or y is None or x.shape != y.shape:
          swapped = None
          break
        swapped.append((("%s%s" % (eye, other)), _parityMetrics(_pm(x), _pm(y))))
      if not swapped:
        ok = False
        self._leg("teeth_swap", "FAIL", "UNCOMPUTABLE (missing/shape)")
      else:
        each = [(tag, _parityWithin(m, bars), m) for tag, m in swapped]
        tooth_ok = all(not w for _, w, _ in each)
        ok = ok and tooth_ok
        self._leg("teeth_swap", "PASS" if tooth_ok else "FAIL",
                  "swapped pairs %s (each must be OUTSIDE the parity bar)"
                  % " ".join("%s:mean=%.4f,%s" % (t, m["mean"], "WITHIN" if w else "OUTSIDE")
                             for t, w, m in each))

    # ---- A4 RIG: regional stereo (A2) + THE CULL ORACLE ----------------------
    if RIG:
      for slot in self._slots:
        a = arrs[slot.name]
        L, R = a.get(("t1", "L")), a.get(("t1", "R"))
        if L is None or R is None or L.shape != R.shape:
          ok = False
          self._leg("a2_regions_%s" % slot.name, "FAIL", "UNCOMPUTABLE (missing/shape)")
          continue
        det, rok = [], True
        # (i) particle family — the region that carries A2's silent regression. Measured
        #     over the cloud's OWN footprint (the non-reproducible bbox), not the
        #     conservative exclusion rect: the rect is mostly sky, and averaging the
        #     cloud's parallax over it dilutes the very signal this leg exists to see.
        pd, pn = _regionalDelta(L, R, pbox or pmask)
        if pd is None:
          rok = False
          det.append("particles=NO-ROI")
        else:
          rok = rok and (pd >= DELTA_FLOOR)
          det.append("particles delta=%.3f n=%d" % (pd, pn))
        # (ii) terrain family — the ground band, particles excluded. The band is the
        #      bottom third of the eye buffer, which this static head pose fills with
        #      terrain (the horizon sits above frame centre).
        gd, gn = _regionalDelta(L, R, RIG_GROUND_BAND, exclude=pmask)
        if gd is None:
          rok = False
          det.append("ground=NO-ROI")
        else:
          rok = rok and (gd >= DELTA_FLOOR)
          det.append("ground delta=%.3f n=%d" % (gd, gn))
        ok = ok and rok
        self._leg("a2_regions_%s" % slot.name, "PASS" if rok else "FAIL",
                  "%s (each >= %.2f; a family rendered mono reads ~0 HERE while the "
                  "whole-frame delta stays healthy)" % (" ".join(det), DELTA_FLOOR))

      for slot in self._slots:
        a = arrs[slot.name]
        pick = lambda take: {e: a.get((take, e)) for e in ("L", "R")}  # noqa: E731
        occ, bnd, clr = pick("occ"), pick("t1"), pick("clr")
        status, detail, met = _cullOracle(occ, bnd, clr, pmask)
        # RED is the EXPECTED reading while the defect is live: this leg exists to go
        #  green when the cull fix lands, so it is reported, never silently gating.
        #  BLIND is a harness failure and DOES gate.
        if status == "ORACLE_BLIND":
          ok = False
        self._leg("cull_oracle_%s" % slot.name, status,
                  "%s [%s] %s" % (slot.kind, RIG_TICK_BND, detail))

        if slot.name != "A":
          continue
        # ARMED NEGATIVE CONTROLS — the plan's (c): prove the oracle SEES the artifact
        #  when it is present. Both use already-captured frames, no extra render, and
        #  both must trip; an oracle that stays green under a known visibility
        #  difference is blind and the gate says so.
        # Each arm substitutes ONLY the boundary frame, per eye, and leaves both
        #  controls per-eye intact — so the oracle's geometric self-check still runs
        #  and what is being proven is the DISCRIMINATOR, not a degenerate input.
        arms = [
          ("suppressed", occ, {"L": bnd["L"], "R": occ["R"]}, clr,
           "eye R's boundary frame replaced by its own occluded frame: the second eye "
           "has lost the reveal entirely — the wrong-cull's extreme"),
          ("amplified", occ, {"L": occ["L"], "R": clr["R"]}, clr,
           "eye L at the occluded tick vs eye R at the cleared tick: a known, large "
           "visibility difference through the oracle's own metric"),
        ]
        armed_ok, adet = True, []
        for tag, o, b, c, why in arms:
          st, dt, _ = _cullOracle(o, b, c, pmask, check_pose=False)
          fired = (st == "ORACLE_RED")
          armed_ok = armed_ok and fired
          adet.append("%s->%s" % (tag, st))
          if not fired:
            adet.append("(%s | %s)" % (why, dt))
        ok = ok and armed_ok
        self._leg("cull_oracle_armed", "PASS" if armed_ok else "FAIL",
                  "%s (each must read ORACLE_RED)" % " ".join(adet))

      # ---- (l) TEMPORAL STABILITY -------------------------------------------
      # The live D3 leg. GREEN on the current tip is the evidence that the cull-bug
      #  family is DEAD here; RED is a regression and GATES (unlike the D2 cull-oracle
      #  leg, whose RED was the expected reading of a live defect). Under the
      #  guard-revert child the polarity inverts: RED is what the reverted guard is
      #  supposed to produce, so the child does not fail on it.
      for slot in self._slots:
        a = arrs[slot.name]
        occ = {e: a.get(("occ", e)) for e in ("L", "R")}
        clr = {e: a.get(("clr", e)) for e in ("L", "R")}
        # each hold is watched against the OTHER pose (populated mask both ways), and
        #  that same frame is the teeth's spliced flip.
        holds = [("bnd", occ, RIG_TICK_BND), ("occ", clr, RIG_TICK_OCC)]
        for tag, ref, tick in holds:
          paths = slot.tcaps.get(tag, [])
          stamps = slot.tframes.get(tag, [])
          torn = [k for k, v in slot.drain_ready.items()
                  if k.startswith("tmp_%s_" % tag) and not v]
          span = ""
          eng = [s[1] for s in stamps if s[1] is not None]
          if len(eng) >= 2:
            span = " engine frames %d..%d (span %d = >=%d build+consume cycles)" % (
                eng[0], eng[-1], eng[-1] - eng[0], (eng[-1] - eng[0]) // 2)
          elif len(stamps) >= 2:
            span = " gate frames %d..%d" % (stamps[0][0], stamps[-1][0])
          if torn:
            ok = False
            self._leg("temporal_%s_%s" % (tag, slot.name), "ORACLE_TEMPORAL_BLIND",
                      "%d take(s) never drained inside %d frames (%s): a torn readback "
                      "would masquerade as a visibility flip"
                      % (len(torn), WRITE_FRAMES, ",".join(sorted(torn))))
            continue
          st, dt, tm = _temporalOracle(paths, ref, pmask, bars["mean"])
          if st == "ORACLE_TEMPORAL_BLIND":
            ok = False
          elif st == "ORACLE_TEMPORAL_RED":
            ok = ok and REVERT_CHILD
          self._leg("temporal_%s_%s" % (tag, slot.name), st,
                    "%s tick t%d: %s%s" % (slot.kind, tick, dt, span))

          # TEETH, in-run, from already-captured frames: splice the contrasting-pose
          #  frame into the SAME sequence. It is a synthetic visibility flip of known
          #  large size, so the leg must read RED — an oracle that stays green under a
          #  flip it can see cannot be trusted when it reports none.
          if slot.name != "A":
            continue
          tst, tdt, _ = _temporalOracle(paths, ref, pmask, bars["mean"], splice=ref)
          tooth_ok = (tst == "ORACLE_TEMPORAL_RED")
          ok = ok and tooth_ok
          self._leg("temporal_teeth_%s" % tag, "PASS" if tooth_ok else "FAIL",
                    "spliced a %s-pose frame into the %s hold -> %s (must be "
                    "ORACLE_TEMPORAL_RED)%s"
                    % ("occluded" if tag == "bnd" else "cleared", tag, tst,
                       "" if tooth_ok else " | " + tdt))

      # ---- (m) PYRAMID PROVENANCE (skip-gated) ------------------------------
      # The guard's own invariant, read directly: the pyramid the cull consumes must
      #  come from a STRICTLY EARLIER frame's depth. Equality or greater is the exact
      #  state e83f63345 forbids.
      surface = any(bool(s.hzb_surface) for s in self._slots)
      if not surface:
        self._leg("provenance", "SKIP",
                  "LEG_PROVENANCE=SKIP pending engine binding — no Scene.hzb / "
                  "sourceDepthFrame surface on this install (filed engine ask: expose "
                  "HZBBuilder::_sourceDepthFrame beside the already-pybound "
                  "ctx.frameIndex). NOT a pass: the invariant is UNMEASURED here and "
                  "leg (l) is what covers the family meanwhile")
      else:
        samples, bad = [], []
        for slot in self._slots:
          for tag, stamps in slot.tframes.items():
            for gframe, fidx, src in stamps:
              if fidx is None or src is None:
                continue
              samples.append((slot.name, tag, fidx, src))
              if src >= fidx:
                bad.append("%s/%s frame %d consumed a pyramid built from frame %d"
                           % (slot.name, tag, fidx, src))
        if not samples:
          ok = False
          self._leg("provenance", "FAIL",
                    "surface present but no (frameIndex, sourceDepthFrame) pair was "
                    "sampled — the probe is wired wrong, which is not a pass")
        else:
          good = not bad
          ok = ok and (good or REVERT_CHILD)
          self._leg("provenance", "ORACLE_GREEN" if good else "ORACLE_RED",
                    "%d sampled frames, sourceDepthFrame < frameIndex on %d%s"
                    % (len(samples), len(samples) - len(bad),
                       "" if good else " | VIOLATIONS: " + "; ".join(bad[:4])))

      # ---- (n) GUARD-REVERT NEGATIVE (skip-gated on the SAME surface) --------
      ok = ok and self._guardRevertLeg(surface)

    # ---- FRAME TIME (ruling 1 / A10) ----------------------------------------
    ft = {}
    for slot in self._slots:
      dts = slot.frame_dts
      if len(dts) < 2:
        ok = False
        self._leg("frametime_%s" % slot.name, "FAIL",
                  "no timed frames captured (window=%.2fs)" % slot.timed_seconds)
        continue
      arr = np.asarray(dts, dtype=np.float64)
      p50 = float(np.percentile(arr, 50)) * 1000.0
      p99 = float(np.percentile(arr, 99)) * 1000.0
      mins = _minSustainedFps(dts)
      ft[slot.name] = (p50, mins)
      self._leg("frametime_%s" % slot.name, "INFO",
                "kind=%s frames=%d window=%.2fs p50=%.3fms p99=%.3fms mean_fps=%.1f min_sustained=%s"
                % (slot.kind, len(dts), slot.timed_seconds, p50, p99,
                   len(dts) / max(slot.timed_seconds, 1e-9),
                   ("%.1f fps" % mins) if mins is not None else
                   "UNMEASURED(window<%.1fs)" % MIN_SUSTAINED_WINDOW))

    if len(ft) == 2:
      pa, pb = ft["A"][0], ft["B"][0]
      div = abs(pa - pb) / max(min(pa, pb), 1e-9)
      # self-parity: the two numbers SHOULD be statistically indistinguishable, so a
      #  large spread is worth a warning line — but frame time on a shared bench is
      #  not a failure signal by itself. The DMVR-vs-SPVR comparison bar (SPVR.md
      #  ruling 1: SPVR p50 AND min-sustained >= DMVR) lands with the SPVR node.
      # ASYMMETRY TO CLOSE BEFORE THAT BAR IS APPLIED: slot A is timed while slot B
      #  does not exist yet, slot B is timed with slot A's scene still resident (not
      #  rendered, but allocated). Self-parity measures the size of that bias — 0.4%
      #  to 0.8% on this bench — and it is well under the WARN band, but a real
      #  DMVR-vs-SPVR verdict should either time each node in its own process or
      #  tear the finished slot down first.
      warn = div > FRAMETIME_DIVERGENCE_WARN
      self._leg("frametime_divergence", "WARN" if warn else "INFO",
                "p50 A=%.3fms B=%.3fms spread=%.1f%% (warn>%.0f%%, non-gating%s)"
                % (pa, pb, div * 100.0, FRAMETIME_DIVERGENCE_WARN * 100.0,
                   "; slots are the same node kind" if SELF_PARITY else ""))

    # ---- SUBMIT COUNT (A10) — armed, per-frame over the timed window --------
    # One number per timed frame (the counter resets at _doBeginFrame, so each read IS
    #  a per-frame delta). p50 + total per node; the BETWEEN-node comparison stays
    #  non-gating INFO for the same reason the frame-time divergence line is.
    counts = []
    for s in self._slots:
      counts.append((s.name, None if not s.submits else s.submits))
    if any(c is None for _, c in counts):
      # gating rule: unavailable is tolerable ONLY while both slots are the same
      #  node kind (fairness is trivial); a real DMVR-vs-SPVR run cannot claim
      #  fairness without it.
      missing = ",".join(n for n, c in counts if c is None)
      why = ("ctx.submitCount is not bound on this install"
             if not hasattr(getattr(self, "ctx", None), "submitCount")
             else "no timed frames sampled it")
      if SELF_PARITY:
        self._leg("submit_count", "UNAVAILABLE",
                  "SUBMIT_COUNT=UNAVAILABLE for slot(s) %s — %s; non-gating while both "
                  "slots are %s" % (missing, why, NODE_A_KIND))
      else:
        ok = False
        self._leg("submit_count", "FAIL",
                  "SUBMIT_COUNT=UNAVAILABLE for slot(s) %s — %s; A10 fairness cannot be "
                  "claimed for a %s-vs-%s comparison without it"
                  % (missing, why, NODE_A_KIND, NODE_B_KIND))
    else:
      det = []
      for n, series in counts:
        arr = np.asarray(series, dtype=np.float64)
        det.append("%s: frames=%d p50=%.1f total=%d min=%d max=%d"
                   % (n, arr.size, float(np.percentile(arr, 50)), int(arr.sum()),
                      int(arr.min()), int(arr.max())))
      self._leg("submit_count", "INFO",
                "per-frame vkQueueSubmit over the timed window | %s (comparison between "
                "slots is INFO, not a bar)" % " | ".join(det))

    mode = "SAMEEYE(neg)" if SAME_EYE else "DUALEYE"
    summary = " | ".join("%s=%s" % (n, s) for n, s, _ in self._legs
                         if s not in ("INFO",))
    self._verdict_code = verdict(
        ok, "spvr gate harness [%s A=%s B=%s] | %s"
            % (mode, NODE_A_KIND, NODE_B_KIND, summary))


def main():
  from ork.testing import Watchdog
  os.makedirs(OUTDIR, exist_ok=True)
  wd = Watchdog(300.0, label="dmvr_capture_gate").arm()
  app = GateApp(os.path.abspath(OUTDIR))
  app.ezapp.mainThreadLoop()
  wd.disarm()
  code = getattr(app, "_verdict_code", None)
  if code is None:
    from ork.testing import verdict
    code = verdict(False, "loop exited before captures (no frame evidence)")
  app.ezapp.shutdown()
  sys.exit(code)


if __name__ == "__main__":
  main()
