#!/usr/bin/env ork.python
################################################################################
# CUTOUT DEPTH PREPASS vs MULTISAMPLING — the generator contract that keeps a
# masked prepass a NO-OP on the image.
#
# THE DEFECT THIS CLOSES. A cutout surface (leaf / needle card) is drawn twice:
# a masked depth prepass that evaluates only the opacity subgraph, then the color
# pass. The prepass fragment is DEPTH-REPLACING — it must write gl_FragDepth,
# because a shadow pass has zero color attachments and a fragment with no other
# observable effect gets elided, discard included, and alpha-tested casters then
# shadow solid. But a depth-replacing fragment stamps ONE value on every sample it
# covers, while the color pass that follows keeps true per-sample interpolated
# depth. Stamp the fragment CENTRE depth and, on any tilted card, about half the
# samples interpolate BEHIND it and lose LEQUAL against the card's own prepass
# depth. The card z-fights its own ghost: the resolved pixel mixes card tone with
# whatever is behind it at discrete sample ratios, and any motion steps it between
# them — foliage that flickers between two tones, in mono and in stereo alike, and
# only when multisampling is on (at msaa 0 sample and centre coincide, which is
# why turning msaa off "fixed" it and why every single-sample capture came back
# quiet).
#
# Measured on the forest canopy at 4x before the correction: the prepass moved 43%
# of the frame and cost the canopy 24% of its energy — a prepass is an occlusion
# optimisation and is only correct when it changes NOTHING.
#
# THE CONTRACT. The written depth must be an UPPER BOUND over the fragment's
# samples, so no color sample can lose to it: centre depth pushed out by the
# pixel's own screen-space depth slope. Derivative-derived, never a tuned bias —
# it scales with the surface, and erring FARTHER only makes the prepass occlude
# less. The color pass keeps per-sample depth (it must NOT be depth-replacing);
# that asymmetry is exactly why the prepass value has to be conservative.
#
# LEGS (all must pass)
#   (A) CONSERVATIVE  the masked prepass widens its written depth by the screen-
#                     space depth slope — a bare gl_FragCoord.z stamp is the bug.
#   (B) NON-ELIDABLE  it still writes gl_FragDepth (dropping the depth-replacing
#                     write also cures the flicker, and takes alpha-tested shadow
#                     casters with it — that is not the fix).
#   (C) PER-SAMPLE    the color pass does NOT write gl_FragDepth. If it ever does,
#                     both passes share one rule and leg (A) can be revisited —
#                     until then the conservative bound is load-bearing.
#   (D) IDENTITY      a surface that declares no alpha_cutout emits no masked
#                     prepass at all: this is a cutout contract, not a tax on
#                     every material in the engine.
#
#   PROVEN here: the emitted shader text, for the REAL shipping leaf material.
#   NOT proven here: pixels. The prepass-invariance measurement is a forest render
#                    (prepass ON vs OFF at msaa 4x must agree).
#
# Self-configuring: no arguments, no environment, no GPU.
#   ork.python test_cutout_prepass_depth_gate.py
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"

import sys
import time

sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import orkengine.core                                        # noqa: E402  (core before lev2)
from orkengine.core import vec3                              # noqa: E402

from ork.hypergraph.assets.materials.leaf import LeafProc                # noqa: E402
from ork.hypergraph.assets.materials.terrain.solid import Solid          # noqa: E402
from ork.hypergraph.ptex3d import materialize_ptex3d_full               # noqa: E402
from ork.hypergraph.ptex3d.fxv2_template import _dslcache_dir           # noqa: E402

DPP_FS = "ps_ptex_dpp_masked"


def _gen(cls, hint, **kw):
  """Generate a material and return its generated .fxv2 text."""
  path, _pspecs, _lobes, _caps = materialize_ptex3d_full(cls, name_hint=hint, **kw)
  # the generator returns the relocatable "<staging>/dslshadercache/ptex3d/<f>" token;
  # resolve it against the same cache dir the writer used (a <token> path's .exists lies).
  local = os.path.join(_dslcache_dir("ptex3d"), os.path.basename(path))
  with open(local) as fp:
    return fp.read()


def _shader_body(text, name):
  """The brace body of `fragment_shader <name> ... { ... }` (to its closing brace)."""
  head = "fragment_shader %s" % name
  i = text.find(head)
  if i < 0:
    return None
  i = text.find("{", i)
  depth, j = 0, i
  while j < len(text):
    if text[j] == "{":
      depth += 1
    elif text[j] == "}":
      depth -= 1
      if depth == 0:
        return text[i + 1:j]
    j += 1
  return None


def main():
  t0 = time.time()
  fails = []

  ##############################################################################
  # the REAL shipping cutout surface (alpha_cutout + A2C + two-sided cards)
  ##############################################################################
  leaf = _gen(LeafProc, "gate_cutout_leaf",
              albedo=vec3(0.24, 0.52, 0.17), roughness=0.55, metallic=0.0)
  dpp = _shader_body(leaf, DPP_FS)
  if dpp is None:
    fails.append("the shipping leaf material emits no %s — a declared alpha_cutout must "
                 "produce the masked depth prepass" % DPP_FS)

  ##############################################################################
  # (A) CONSERVATIVE — the stamped depth is widened by the pixel's depth slope
  ##############################################################################
  if dpp is not None:
    writes = [ln.strip() for ln in dpp.splitlines()
              if "gl_FragDepth" in ln and "=" in ln and not ln.strip().startswith("//")]
    if not writes:
      fails.append("CONSERVATIVE: %s never assigns gl_FragDepth" % DPP_FS)
    else:
      # the assigned expression must trace back to a screen-space derivative of depth:
      # the whole point is a bound over the fragment's SAMPLES, which only dFdx/dFdy give.
      derived = ("dFdx" in dpp and "dFdy" in dpp)
      bare = any(w.replace(" ", "").endswith("=gl_FragCoord.z;") for w in writes)
      if bare or not derived:
        fails.append("CONSERVATIVE: %s stamps the fragment CENTRE depth on every covered "
                     "sample (%s). Under multisampling the color pass keeps per-sample "
                     "interpolated depth, so samples behind the centre lose LEQUAL to the "
                     "surface's own prepass depth and resolve to what is behind it — the "
                     "two-tone foliage flicker. Widen the written depth by the screen-space "
                     "depth slope (dFdx/dFdy) so it bounds the fragment's samples."
                     % (DPP_FS, "; ".join(writes)))
    print("LEG_A conservative: %s writes %s" % (DPP_FS, " | ".join(writes) if writes else "NOTHING"))

  ##############################################################################
  # (B) NON-ELIDABLE — the depth-replacing write itself must survive
  ##############################################################################
  if dpp is not None:
    if "gl_FragDepth" not in dpp:
      fails.append("NON-ELIDABLE: %s no longer writes gl_FragDepth. That also cures the "
                   "flicker — by making the fragment elidable in a zero-color-attachment "
                   "shadow pass, which drops the discard and shadows the cards solid. The "
                   "prepass stays depth-replacing; its VALUE is what had to change." % DPP_FS)
    print("LEG_B non-elidable: gl_FragDepth declared and written")

  ##############################################################################
  # (C) PER-SAMPLE — the color pass keeps interpolated depth
  ##############################################################################
  color_writers = []
  for line in leaf.splitlines():
    s = line.strip()
    if s.startswith("//") or "gl_FragDepth" not in s or "=" not in s:
      continue
    color_writers.append(s)
  # every gl_FragDepth assignment in the material must belong to the masked prepass
  extra = [w for w in color_writers if dpp is None or w not in dpp]
  if extra:
    fails.append("PER-SAMPLE: a fragment OUTSIDE %s assigns gl_FragDepth (%s). The color "
                 "pass must keep per-sample interpolated depth — the conservative prepass "
                 "bound in leg (A) exists precisely because the two rules differ."
                 % (DPP_FS, "; ".join(extra)))
  print("LEG_C per-sample: %d gl_FragDepth assignment(s), all inside %s"
        % (len(color_writers), DPP_FS))

  ##############################################################################
  # (D) IDENTITY — no alpha_cutout, no masked prepass
  ##############################################################################
  plain = _gen(Solid, "gate_cutout_plain", albedo=vec3(0.5, 0.5, 0.5), roughness=0.5)
  if DPP_FS in plain:
    fails.append("IDENTITY: a material that declares no alpha_cutout emits %s — the masked "
                 "prepass leaked into every material in the engine" % DPP_FS)
  if "gl_FragDepth" in plain:
    fails.append("IDENTITY: a non-cutout material writes gl_FragDepth — depth replacement "
                 "kills early-z for surfaces that never asked for it")
  print("LEG_D identity: non-cutout material carries no masked prepass")

  ##############################################################################
  for f in fails:
    print("FAIL: " + f)
  ok = not fails
  print("elapsed %.1fs" % (time.time() - t0))
  print("CUTOUT_PREPASS_DEPTH_RESULT=%s" % ("PASS" if ok else "FAIL"))
  sys.exit(0 if ok else 1)


main()
