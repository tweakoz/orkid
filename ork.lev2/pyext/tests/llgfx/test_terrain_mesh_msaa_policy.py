#!/usr/bin/env ork.python
################################################################################
# TERRAIN MESH-MODE MSAA STEP-DOWN POLICY gate.
#
# THE POLICY (terrain::terrainMeshMsaaStepDown, called by TerrainChunkDrawableData's
# path selection ahead of the caps/technique cascade): on Metal/MoltenVK, when the FORWARD
# pass's render target is multisampled AND the mesh stage's per-workgroup output payload is
# ABOVE the measured cliff, terrain mesh mode steps DOWN to the SSBO-pull VS.
#
# WHY IT IS PAYLOAD-CONDITIONED AND NOT "Metal + MSAA" (measured jul26, M3 Ultra):
# Metal tile memory is shared between the multisampled attachment set and the mesh stage's
# output payload, so the mesh path's MSAA cost is a function of the MESHLET SIZE, and it
# falls off a cliff between meshlet 9 and 8. At the old 11 payload mesh lost to pull-VS at
# 2x/4x (1.18-1.69x slower from 2560x1280 up); at the SHIPPED meshlet 8 mesh WINS even at 4x
# (130.5 vs 120.3 fps in the real forward pass). So at the shipped default this policy is
# DORMANT — mesh runs under MSAA on mac — and it arms only if a fatter payload is selected
# (ORKID_TERRAIN_MESHLET, or a future codegen change that re-fattens the workgroup output).
# Off darwin it never fires: the discrete-GPU platforms are payload-flat.
#
# WHAT THIS GATE ASSERTS:
#
#   0. THE TWO DEFAULTS ARE ONE. The C++ shipped meshlet (terrain.meshlet_dim() with the env
#      unset — what sizes the dispatch grid) EQUALS gpu_chunk.DEFAULT_MESHLET_DIM (what sizes
#      the generated payload). A split default dispatches a grid the shader does not agree
#      with, and nothing else in the build would say so.
#   1. THE INPUT IS REAL. ctx.msaa_forward_samples — the resolver ForwardPbrNodeImpl sizes
#      its primary RtgSet from — tracks the live forward MSAA level, device-clamped, and a
#      target at THAT count is genuinely constructible and multisampled, proven with the
#      RtGroup msaa knob (rtg.msaa_samples). Without this leg the policy could be fed a
#      permanent 1x in production and the decision half would still look correct.
#   2. THE DECISION TABLE, every (platform, sample count, payload, force) combination:
#        darwin  1x       any payload  -> mesh          (never step down at 1x)
#        darwin  2x/4x    meshlet<=8   -> mesh          (at/below the cliff: mesh WINS)
#        darwin  2x/4x    meshlet>8    -> pull-VS       (above the cliff)
#        darwin  2x/4x    meshlet>8 + ORKID_TERRAIN_MESH_FORCE=1 -> mesh (measurement path)
#        non-darwin  any  any payload  -> mesh
#   3. THE LINE IS LOUD AND NAMES THE PAYLOAD. Stepping down prints one named TERRAIN line
#      naming BOTH the meshlet payload and the multisampled attachment context, and prints
#      nothing when the policy does not act (including force with a payload at the default —
#      an override that announces itself while overriding nothing is a lie). Forcing prints
#      its own named line, because perf sweeps must still be able to time a fat payload under
#      MSAA on mac.
#
# FOUR CHILD PROCESSES: both the force override and the meshlet dimension are read once per
# process (statics, like every other terrain env knob), so each (force, payload) pair is its
# own run. Each run walks every sample count.
#
# SCOPE (honest): this exercises the policy function and its input resolver — the SAME
# function the drawable calls, at its only call site. It does NOT materialize a terrain
# scene, so "pull-VS is active downstream of a TRUE verdict" (the drawable's single branch)
# is covered by a real-scene run, not here. Fully synthetic: no assets, no window.
#
# SKIP is loud and only where the policy has no subject: no VK_EXT_mesh_shader (mesh mode
# never engages) or msaa_max_samples == 1 (no multisampled target is constructible).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)
import subprocess

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from ork.testing import headless_app, verdict
from ork.hypergraph.dflow.terrain.gpu_chunk import DEFAULT_MESHLET_DIM, MAX_MESHLET_DIM

IS_DARWIN = sys.platform.startswith("darwin")

# forward MSAA LEVEL -> the hw sample count the forward target asks for (msaaLevelToSamples)
LEVELS = ((0, 1), (1, 2), (2, 4))

# payloads under test: the shipped default (env unset -> the policy must stay dormant) and the
# fattest payload the taskless tier allows (env set -> the policy must arm on darwin).
MESHLET_FAT = MAX_MESHLET_DIM
assert MESHLET_FAT > DEFAULT_MESHLET_DIM, \
    "no payload above the shipped default exists — the step-down half of this gate has no subject"

STEPDOWN_TAG = "TERRAIN-MESHSHADER: STEP-DOWN pull-VS"
FORCE_TAG    = "TERRAIN-MESHSHADER: FORCE-OVERRIDE mesh kept"

W, H = 64, 64

################################################################################
# CHILD — one process per (force, payload); walks every level in that process.
################################################################################

def child(force):
  from orkengine import core   # core before lev2
  from orkengine import lev2

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx    = app.ctx
    devmax = int(ctx.msaa_max_samples)
    # the EFFECTIVE payload this process runs with — asked of the engine, not re-derived here
    print("MESHMSAA-DEV devmax=%d mesh_ext=%d meshlet=%d"
          % (devmax, int(ctx.supports_mesh_shader), int(lev2.terrain.meshlet_dim())))
    for level, count in LEVELS:
      if count > devmax:
        continue
      # the live level the forward compositor reads (the scene `msaa=` seam)
      lev2.setForwardMsaaLevel(level)
      fwd = int(ctx.msaa_forward_samples)
      # the resolver's claim must be a target the device can really build at that count
      rtg = int(lev2.RtGroup(ctx, W, H, fwd).msaa_samples)
      # MARK brackets the decision so its loud line is attributable to THIS count
      print("MESHMSAA-MARK level=%d" % level)
      stepdown = bool(lev2.terrain.mesh_msaa_stepdown(fwd))
      print("MESHMSAA-LEG force=%d level=%d devmax=%d fwd=%d rtg=%d stepdown=%d"
            % (int(force), level, devmax, fwd, rtg, int(stepdown)))
  return 0

################################################################################
# PARENT
################################################################################

def run_child(force, meshlet):
  """meshlet=None -> leave ORKID_TERRAIN_MESHLET unset (the SHIPPED default path)."""
  env = dict(os.environ)
  env.pop("ORKID_TERRAIN_MESH_FORCE", None)
  env.pop("ORKID_TERRAIN_MESHLET", None)
  if force:
    env["ORKID_TERRAIN_MESH_FORCE"] = "1"
  if meshlet is not None:
    env["ORKID_TERRAIN_MESHLET"] = str(int(meshlet))
  # re-exec THIS file through its own ork.python shebang (the +x launcher)
  cmd = [os.path.abspath(__file__), "--child", str(int(force))]
  pr = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=300)
  out = pr.stdout + pr.stderr
  legs, dev = {}, None
  for ln in out.splitlines():
    if ln.startswith("MESHMSAA-DEV "):
      dev = dict((kv.split("=", 1)[0], int(kv.split("=", 1)[1])) for kv in ln.split()[1:])
    elif ln.startswith("MESHMSAA-LEG "):
      rec = dict((kv.split("=", 1)[0], int(kv.split("=", 1)[1])) for kv in ln.split()[1:])
      legs[rec["level"]] = rec
  if dev is None:
    print(out[-4000:])
    raise RuntimeError("child force=%d meshlet=%s produced no MESHMSAA-DEV line (rc=%d)"
                       % (force, meshlet, pr.returncode))
  return dev, legs, out


def lines_for_level(out, level):
  """The TERRAIN policy lines printed between this level's MARK and the next one."""
  block, active = [], False
  for ln in out.splitlines():
    if ln.startswith("MESHMSAA-MARK "):
      active = (int(ln.split("level=")[1]) == level)
      continue
    if active and (STEPDOWN_TAG in ln or FORCE_TAG in ln):
      block.append(ln.strip())
  return block


def main():
  fails = []

  dev, legs, out = run_child(False, None)
  print("  device: msaa max %dx, mesh_ext=%d" % (dev["devmax"], dev["mesh_ext"]))
  print("  shipped meshlet: C++ terrain.meshlet_dim()=%d, gpu_chunk.DEFAULT_MESHLET_DIM=%d"
        % (dev["meshlet"], DEFAULT_MESHLET_DIM))

  # 0. the two defaults are ONE (checked before any SKIP — it needs no MSAA and no mesh ext)
  if dev["meshlet"] != DEFAULT_MESHLET_DIM:
    fails.append("SHIPPED DEFAULT SPLIT: the drawable sizes its dispatch grid from meshlet %d but "
                 "gpu_chunk generates a payload for meshlet %d — the grid and the shader disagree"
                 % (dev["meshlet"], DEFAULT_MESHLET_DIM))

  if not dev["mesh_ext"] or dev["devmax"] < 2:
    why = ("device has no VK_EXT_mesh_shader (terrain mesh mode never engages here)"
           if not dev["mesh_ext"]
           else "device msaa max is 1x (no multisampled forward target is constructible)")
    print("SKIP: %s" % why)
    detail = "terrain mesh MSAA step-down SKIPPED — %s | shipped_meshlet=%d" % (why, DEFAULT_MESHLET_DIM)
    sys.exit(verdict(not fails, detail if not fails else detail + " | " + "; ".join(fails)))

  # (force, meshlet-env) -> run. meshlet-env None = the shipped default.
  COMBOS = ((False, None), (True, None), (False, MESHLET_FAT), (True, MESHLET_FAT))
  runs = {(False, None): (legs, out)}
  for key in COMBOS[1:]:
    runs[key] = run_child(key[0], key[1])[1:]

  seen = []
  for (force, mlenv) in COMBOS:
    legs, out = runs[(force, mlenv)]
    meshlet = DEFAULT_MESHLET_DIM if mlenv is None else mlenv
    above_cliff = meshlet > DEFAULT_MESHLET_DIM
    for level, count in LEVELS:
      want_fwd = min(count, dev["devmax"])
      rec = legs.get(level)
      if rec is None:
        if count <= dev["devmax"]:
          fails.append("force=%d meshlet=%d level=%d: no leg reported" % (int(force), meshlet, level))
        continue
      # 1. the input is real
      if rec["fwd"] != want_fwd:
        fails.append("level=%d: forward samples %dx, expected %dx (level->count, device-clamped)"
                     % (level, rec["fwd"], want_fwd))
      if rec["rtg"] != rec["fwd"]:
        fails.append("level=%d: RtGroup at the resolved count came back %dx, not %dx"
                     % (level, rec["rtg"], rec["fwd"]))
      # 2. the decision table
      multisampled  = (rec["fwd"] > 1)
      arms          = bool(IS_DARWIN and multisampled and above_cliff)
      want_stepdown = bool(arms and not force)
      if bool(rec["stepdown"]) != want_stepdown:
        fails.append("force=%d meshlet=%d level=%d: stepdown=%d, expected %d (%s, %dx forward "
                     "target, cliff at %d)"
                     % (int(force), meshlet, level, rec["stepdown"], int(want_stepdown),
                        sys.platform, rec["fwd"], DEFAULT_MESHLET_DIM))
      # 3. the line is loud, names the payload AND the count, and fires ONLY when the policy acts
      block     = lines_for_level(out, level)
      has_step  = any(STEPDOWN_TAG in l for l in block)
      has_force = any(FORCE_TAG in l for l in block)
      want_force_line = bool(arms and force)
      if want_stepdown and not has_step:
        fails.append("meshlet=%d level=%d: stepped down SILENTLY — no '%s' line"
                     % (meshlet, level, STEPDOWN_TAG))
      if has_step and not want_stepdown:
        fails.append("force=%d meshlet=%d level=%d: step-down line fired but the policy did not "
                     "step down" % (int(force), meshlet, level))
      if want_force_line and not has_force:
        fails.append("meshlet=%d level=%d: ORKID_TERRAIN_MESH_FORCE kept mesh SILENTLY — no '%s' "
                     "line" % (meshlet, level, FORCE_TAG))
      if has_force and not want_force_line:
        fails.append("force=%d meshlet=%d level=%d: FORCE-OVERRIDE line fired without an armed "
                     "step-down to override" % (int(force), meshlet, level))
      joined = " ".join(block)
      if has_step or has_force:
        if ("MSAA %dx forward target" % rec["fwd"]) not in joined:
          fails.append("meshlet=%d level=%d: policy line does not name the %dx target"
                       % (meshlet, level, rec["fwd"]))
        if ("meshlet %d" % meshlet) not in joined:
          fails.append("meshlet=%d level=%d: policy line does not name the payload it decided on"
                       % (meshlet, level))
      for l in block:
        print("  %s" % l)
      seen.append("%dx/n%d%s->%s" % (rec["fwd"], meshlet, "/force" if force else "",
                                     "pull-VS" if rec["stepdown"] else "mesh"))

  detail = "terrain mesh MSAA step-down | %s | %s devmax=%dx shipped_meshlet=%d" % (
      " ".join(seen), sys.platform, dev["devmax"], DEFAULT_MESHLET_DIM)
  sys.exit(verdict(not fails, detail if not fails else detail + " | " + "; ".join(fails)))


if len(sys.argv) > 1 and sys.argv[1] == "--child":
  sys.exit(child(bool(int(sys.argv[2]))))
main()
